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

#include "../drive.hpp"
#include "test_info.hpp"

#include <boost/asio.hpp>
#include <iostream>
#include <mctp_wrapper.hpp>

#include <gtest/gtest.h>

struct GlobalData
{
    std::shared_ptr<boost::asio::io_context> ioContext;
    std::shared_ptr<sdbusplus::asio::connection> dbusConnection{};
    uint8_t signalCaught;
};

struct SignalData
{
    std::string driveName;
    std::string interface;
    std::string alarm;
    bool asserted;
    double value;
};

TestInfo gTestInfo;
std::unique_ptr<GlobalData> gAppData;
static std::vector<sdbusplus::bus::match::match> signalMatches;

void monitorSignal()
{
    gAppData->signalCaught = 0;
    signalMatches.emplace_back(
        *(gAppData->dbusConnection),
        "type='signal', "
        "path_namespace='/xyz/openbmc_project/sensors/temperature/"
        "NVMeDrive1_Temp'"
        ",member='ThresholdAsserted'",
        [](sdbusplus::message::message& message) {
            SignalData data;
            try
            {
                message.read(data.driveName, data.interface, data.alarm,
                             data.asserted, data.value);
            }
            catch (sdbusplus::exception_t&)
            {
                std::cout << "Error Getting data\n";
                return;
            }

            std::cout << "Threshold asserted... Alarm " << data.alarm
                      << " for value " << data.value << "\n";

            EXPECT_EQ(data.value, 120);
            EXPECT_EQ(data.asserted, true);

            if (data.interface ==
                "xyz.openbmc_project.Sensor.Threshold.Warning")
            {
                EXPECT_EQ(data.alarm, "WarningAlarmHigh");
                gAppData->signalCaught++;
            }
            else if (data.interface ==
                     "xyz.openbmc_project.Sensor.Threshold.Critical")
            {
                EXPECT_EQ(data.alarm, "CriticalAlarmHigh");
                gAppData->signalCaught++;
            }

            // If Warning and Critical both the signals are emitted the stop
            if (gAppData->signalCaught == 2)
            {
                gAppData->ioContext->stop();
            }
        });
}

TEST(TestThreshold, Test1)
{
    auto objectServer = std::make_shared<sdbusplus::asio::object_server>(
        gAppData->dbusConnection);
    gAppData->dbusConnection->request_name("xyz.openbmc_project.nvmemi_test1");

    constexpr auto bindingType = mctpw::BindingType::mctpOverSmBus;
    mctpw::MCTPConfiguration config(mctpw::MessageType::nvmeMgmtMsg,
                                    bindingType);
    auto mctpWrapper =
        std::make_shared<mctpw::MCTPWrapper>(gAppData->dbusConnection, config);

    auto drive = std::make_shared<nvmemi::Drive>("NVMeDrive1", 1, *objectServer,
                                                 mctpWrapper);
    monitorSignal();

    boost::asio::spawn([&](boost::asio::yield_context yield) {
        drive->pollSubsystemHealthStatus(yield);
        return;
    });

    gAppData->ioContext->run();
}

int main(int argc, char** argv)
{
    gTestInfo.testId = TestID::highThresholdTest;
    gAppData = std::make_unique<GlobalData>();
    gAppData->ioContext = std::make_shared<boost::asio::io_context>();
    gAppData->dbusConnection =
        std::make_shared<sdbusplus::asio::connection>(*gAppData->ioContext);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
