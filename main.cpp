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

class Application
{
    using DriveMap =
        std::unordered_map<mctpw::eid_t, std::shared_ptr<nvmemi::Drive>>;

  public:
    Application() :
        ioContext(std::make_shared<boost::asio::io_context>()),
        signals(*ioContext, SIGINT, SIGTERM),
        pollTimer(std::make_shared<boost::asio::steady_timer>(*ioContext))
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

        boost::asio::spawn([this](boost::asio::yield_context yield) {
            constexpr auto bindingType = mctpw::BindingType::mctpOverSmBus;
            mctpw::MCTPConfiguration config(mctpw::MessageType::nvmeMgmtMsg,
                                            bindingType);
            auto wrapper = std::make_shared<mctpw::MCTPWrapper>(
                this->dbusConnection, config);
            wrapper->detectMctpEndpoints(yield);
            for (auto& [eid, service] : wrapper->getEndpointMap())
            {
                std::string driveName =
                    "NVMeDrive" + std::to_string(this->driveCounter++);
                auto drive = std::make_shared<nvmemi::Drive>(
                    driveName, eid, *this->objectServer, wrapper);
                this->drives.emplace(eid, drive);
            }

            doPoll(yield, this);
        });
    }
    static void doPoll(boost::asio::yield_context yield, Application* app)
    {
        // TODO. Add a mechanism to trigger the poll when new drive is added and
        // stop the same when there is no drive connected
        // TODO. Convert while loop to tail recursion
        while (true)
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

            for (auto& [eid, drive] : app->drives)
            {
                drive->pollSubsystemHealthStatus(yield);
            }
        }
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
    std::vector<std::shared_ptr<mctpw::MCTPWrapper>> mctpWrappers{};
    DriveMap drives{};
    size_t driveCounter = 1;
    std::shared_ptr<boost::asio::steady_timer> pollTimer;
    static constexpr const char* serviceName = "xyz.openbmc_project.nvme_mi";
    static const inline std::chrono::seconds subsystemHsPollInterval{1};
};

int main()
{
    Application app;
    app.init();
    app.run();
    return 0;
}
