/*
// Copyright (c) 2021 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "drive.hpp"

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <mctp_wrapper.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

class Application;

struct DeviceUpdateHandler
{
    DeviceUpdateHandler(Application& appn, mctpw::BindingType binding) :
        app(appn), bindingType(binding)
    {
    }
    void operator()(void*, const mctpw::Event& evt,
                    boost::asio::yield_context& yield);
    Application& app;
    mctpw::BindingType bindingType;
};

class Application
{
    using DriveMap =
        std::unordered_map<mctpw::eid_t, std::shared_ptr<nvmemi::Drive>>;

  public:
    Application() :
        ioContext(std::make_shared<boost::asio::io_context>()),
        signals(*ioContext, SIGINT, SIGTERM), pollTimer(nullptr)
    {
    }
    void init()
    {
        signals.async_wait([this](const boost::system::error_code&,
                                  const int&) { this->ioContext->stop(); });

        dbusConnection =
            std::make_shared<sdbusplus::asio::connection>(*ioContext);
        objectServer =
            std::make_shared<sdbusplus::asio::object_server>(dbusConnection);
        dbusConnection->request_name(serviceName);

        boost::asio::spawn(
            *ioContext, [this](boost::asio::yield_context yield) {
                constexpr auto bindingType = mctpw::BindingType::mctpOverSmBus;
                mctpw::MCTPConfiguration config(mctpw::MessageType::nvmeMgmtMsg,
                                                bindingType);
                auto wrapper = std::make_shared<mctpw::MCTPWrapper>(
                    this->dbusConnection, config,
                    DeviceUpdateHandler(*this, bindingType));
                mctpWrappers.emplace(bindingType, wrapper);
                wrapper->detectMctpEndpoints(yield);
                for (auto& [eid, service] : wrapper->getEndpointMap())
                {
                    std::string driveName =
                        "NVMeDrive" + std::to_string(this->driveCounter++);
                    auto drive = std::make_shared<nvmemi::Drive>(
                        driveName, eid, *this->objectServer, wrapper);
                    this->drives.emplace(eid, drive);
                }
                if (!this->drives.empty())
                {
                    resumeHealthStatusPolling();
                }
            });

        if (auto envPtr = std::getenv("NVME_DEBUG"))
        {
            std::string value(envPtr);
            if (value == "1")
            {
                initializeHealthStatusPollIntf();
            }
        }
    }
    static void doPoll(boost::asio::yield_context yield, Application* app)
    {
        while (app->pollTimer != nullptr)
        {
            boost::system::error_code ec;
            app->pollTimer->expires_after(subsystemHsPollInterval);
            app->pollTimer->async_wait(yield[ec]);
            if (ec == boost::asio::error::operation_aborted)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "Poll timer aborted");
                return;
            }
            else if (ec)
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "Sensor poll timer failed");
                return;
            }

            DriveMap copyDrives(app->drives);
            for (auto& [eid, drive] : copyDrives)
            {
                drive->pollSubsystemHealthStatus(yield);
            }
        }
        phosphor::logging::log<phosphor::logging::level::WARNING>(
            "Drive polling task stopped. Timer is null now");
    }
    void pauseHealthStatusPolling()
    {
        if (pollTimer)
        {
            pollTimer->cancel();
            pollTimer = nullptr;
        }

        phosphor::logging::log<phosphor::logging::level::INFO>(
            "health status polling paused");
    }

    void resumeHealthStatusPolling()
    {
        if (!pollTimer)
        {
            pollTimer = std::make_shared<boost::asio::steady_timer>(*ioContext);
            boost::asio::spawn([this](boost::asio::yield_context yield) {
                doPoll(yield, this);
            });
        }

        phosphor::logging::log<phosphor::logging::level::INFO>(
            "health status polling resumed");
    }
    void initializeHealthStatusPollIntf()
    {
        if (healthStatusPollInterface != nullptr)
        {
            phosphor::logging::log<phosphor::logging::level::DEBUG>(
                "healthStatusPollInterface already initialized");
            return;
        }

        const char* objPath = "/xyz/openbmc_project/healthstatus";
        healthStatusPollInterface = objectServer->add_unique_interface(
            objPath, "xyz.openbmc_project.NVM.HealthStatusPoll");
        healthStatusPollInterface->register_method(
            "PauseHealthStatusPoll", [this](const bool pause) {
                if (pause)
                {
                    pauseHealthStatusPolling();
                }
                else
                {
                    resumeHealthStatusPolling();
                }
            });
        healthStatusPollInterface->initialize();
    }
    void run()
    {
        this->ioContext->run();
    }

  private:
    std::shared_ptr<boost::asio::io_context> ioContext;
    boost::asio::signal_set signals;
    std::shared_ptr<sdbusplus::asio::connection> dbusConnection{};
    std::shared_ptr<sdbusplus::asio::object_server> objectServer{};
    std::unique_ptr<sdbusplus::asio::dbus_interface> healthStatusPollInterface =
        nullptr;
    std::unordered_map<mctpw::BindingType, std::shared_ptr<mctpw::MCTPWrapper>>
        mctpWrappers{};
    DriveMap drives{};
    size_t driveCounter = 1;
    std::shared_ptr<boost::asio::steady_timer> pollTimer;
    static constexpr const char* serviceName = "xyz.openbmc_project.nvme_mi";
    static const inline std::chrono::seconds subsystemHsPollInterval{1};
    friend struct DeviceUpdateHandler;
};

void DeviceUpdateHandler::operator()(
    void*, const mctpw::Event& evt,
    [[maybe_unused]] boost::asio::yield_context& wrapperContext)
{
    switch (evt.type)
    {
        case mctpw::Event::EventType::deviceAdded: {
            std::string driveName =
                "NVMeDrive" + std::to_string(app.driveCounter++);
            auto drive = std::make_shared<nvmemi::Drive>(
                driveName, evt.eid, *app.objectServer,
                app.mctpWrappers.at(bindingType));
            app.drives.emplace(evt.eid, drive);
            phosphor::logging::log<phosphor::logging::level::INFO>(
                "New drive inserted",
                phosphor::logging::entry("EID=%d", evt.eid));
            if (app.drives.size() == 1)
            {
                app.resumeHealthStatusPolling();
            }
        }
        break;
        case mctpw::Event::EventType::deviceRemoved: {
            if (app.drives.erase(evt.eid) == 1)
            {
                phosphor::logging::log<phosphor::logging::level::INFO>(
                    "Drive removed",
                    phosphor::logging::entry("EID=%d", evt.eid));
            }
            else
            {
                phosphor::logging::log<phosphor::logging::level::ERR>(
                    "No drive found mapped to eid",
                    phosphor::logging::entry("EID=%d", evt.eid));
            }
            // Timer cancellation if all drives are removed
            if (app.drives.empty())
            {
                app.pauseHealthStatusPolling();
            }
        }
        break;
        default:
            break;
    }
}

int main()
{
    Application app;
    app.init();
    app.run();
    return 0;
}
