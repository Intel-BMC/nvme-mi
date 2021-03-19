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

static constexpr const char* serviceName = "xyz.openbmc_project.nvme_mi";
static const std::chrono::seconds subsystemHsPollInterval(1);

void doPoll(
    boost::asio::yield_context yield,
    std::unordered_map<mctpw::eid_t, std::shared_ptr<nvmemi::Drive>>& drives,
    std::shared_ptr<boost::asio::steady_timer> timer)
{
    // TODO. Add a mechanism to trigger the poll when new drive is added and
    // stop the same when there is no drive connected
    // TODO. Convert while loop to tail recursion
    while (true)
    {
        boost::system::error_code ec;
        timer->expires_after(subsystemHsPollInterval);
        timer->async_wait(yield[ec]);
        if (ec == boost::asio::error::operation_aborted)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Poll timer aborted");
            break;
        }
        else if (ec)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Sensor poll timer failed");
            break;
        }

        for (auto& [eid, drive] : drives)
        {
            drive->pollSubsystemHealthStatus(yield);
        }
    }
}

int main()
{
    auto ioContext = std::make_shared<boost::asio::io_context>();

    boost::asio::signal_set signals(*ioContext, SIGINT, SIGTERM);
    signals.async_wait([ioContext](const boost::system::error_code&,
                                   const int&) { ioContext->stop(); });

    std::shared_ptr<sdbusplus::asio::connection> dbusConnection{};
    std::shared_ptr<sdbusplus::asio::object_server> objectServer{};
    try
    {
        dbusConnection =
            std::make_shared<sdbusplus::asio::connection>(*ioContext);
        objectServer =
            std::make_shared<sdbusplus::asio::object_server>(dbusConnection);
        dbusConnection->request_name(serviceName);
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Initialization error",
            phosphor::logging::entry("MSG=%s", e.what()));
        return -1;
    }

    std::vector<std::shared_ptr<mctpw::MCTPWrapper>> mctpWrappers{};
    std::unordered_map<mctpw::eid_t, std::shared_ptr<nvmemi::Drive>> drives{};
    size_t driveCounter = 1;
    auto timer = std::make_shared<boost::asio::steady_timer>(*ioContext);

    boost::asio::spawn([dbusConnection, objectServer, &mctpWrappers,
                        &driveCounter, timer,
                        &drives](boost::asio::yield_context yield) {
        constexpr auto bindingType = mctpw::BindingType::mctpOverSmBus;
        mctpw::MCTPConfiguration config(mctpw::MessageType::nvmeMgmtMsg,
                                        bindingType);
        auto wrapper =
            std::make_shared<mctpw::MCTPWrapper>(dbusConnection, config);
        wrapper->detectMctpEndpoints(yield);
        for (auto& [eid, service] : wrapper->getEndpointMap())
        {
            std::string driveName =
                "NVMeDrive" + std::to_string(driveCounter++);
            auto drive = std::make_shared<nvmemi::Drive>(
                driveName, eid, *objectServer, wrapper);
            drives.emplace(eid, drive);
        }

        doPoll(yield, boost::ref(drives), timer);
    });

    ioContext->run();
    return 0;
}
