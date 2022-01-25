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
#include <fstream>
#include <iostream>
#include <mctp_wrapper.hpp>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

TestInfo gTestInfo;

struct GlobalData
{
    std::shared_ptr<boost::asio::io_context> ioContext;
    std::shared_ptr<sdbusplus::asio::connection> dbusConnection{};
    int pid = 0;
    int pToC[2];
    int cToP[2];
};

std::unique_ptr<GlobalData> gAppData;
static constexpr size_t readIdx = 0;
static constexpr size_t writeIdx = 1;

void parentTask()
{
    close(gAppData->cToP[readIdx]);
    close(gAppData->pToC[writeIdx]);

    uint8_t readData = 0x00;
    std::cout << "Waiting for child to setup DBus objects" << '\n';
    boost::asio::posix::stream_descriptor stream{*gAppData->ioContext,
                                                 gAppData->pToC[readIdx]};
    auto handler = [&readData](const boost::system::error_code& ec,
                               std::size_t n) {
        if (ec)
        {
            std::cout << "Read error. " << ec.message() << '\n';
        }
        else
        {
            std::cout << "Read " << n << " bytes from child task" << '\n';
        }
        boost::asio::spawn([&](boost::asio::yield_context yield) {
            try
            {
                std::tuple<int, std::string> status;

                gTestInfo.subTestId = SubTestID::collectLogSuccess;
                boost::system::error_code collectLogError;

                status = gAppData->dbusConnection
                             ->yield_method_call<decltype(status)>(
                                 yield, collectLogError,
                                 "xyz.openbmc_project.nvmemi_test",
                                 "/xyz/openbmc_project/CollectLogDrive",
                                 "xyz.openbmc_project.drive_log", "CollectLog");
                if (collectLogError)
                {
                    std::cout << "Error calling collect log. "
                              << collectLogError.message() << '\n';
                    throw collectLogError;
                }
                else
                {
                    std::cout << "Status " << std::get<0>(status) << " Message "
                              << std::get<1>(status) << '\n';
                    std::ifstream ifs(std::get<1>(status));
                    nlohmann::json nvmeDumpFile = nlohmann::json::parse(ifs);
                    auto activeNamespacesActual =
                        nvmeDumpFile["Identify"]["ActiveNamespaces"]
                            .get<std::vector<uint8_t>>();
                    std::vector<uint8_t> activeNamespacesExpected = {1};
                    EXPECT_TRUE(activeNamespacesActual ==
                                activeNamespacesExpected);
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << "Collect log. " << e.what() << '\n';
            }
            catch (...)
            {
                std::cerr << "Collect log. Generic error." << '\n';
            }

            gAppData->ioContext->stop();
            write(gAppData->cToP[writeIdx], &readData, sizeof(readData));
        });
    };
    boost::asio::async_read(
        stream, boost::asio::buffer(&readData, sizeof(readData)), handler);
    gAppData->ioContext->run();
}
void childTask()
{
    close(gAppData->pToC[readIdx]);
    close(gAppData->cToP[writeIdx]);

    auto objectServer = std::make_shared<sdbusplus::asio::object_server>(
        gAppData->dbusConnection);
    gAppData->dbusConnection->request_name("xyz.openbmc_project.nvmemi_test");

    constexpr auto bindingType = mctpw::BindingType::mctpOverSmBus;
    mctpw::MCTPConfiguration config(mctpw::MessageType::nvmeMgmtMsg,
                                    bindingType);
    auto mctpWrapper =
        std::make_shared<mctpw::MCTPWrapper>(gAppData->dbusConnection, config);

    static constexpr auto dummyEid = 8;
    auto drive = std::make_shared<nvmemi::Drive>("CollectLogDrive", dummyEid,
                                                 *objectServer, mctpWrapper);
    boost::asio::posix::stream_descriptor stream{*gAppData->ioContext,
                                                 gAppData->cToP[readIdx]};
    uint8_t writeData = 'A';

    write(gAppData->pToC[writeIdx], &writeData, sizeof(writeData));
    std::cout << "Waiting for parent to finish unit tests" << '\n';
    auto handler = [&writeData](const boost::system::error_code& ec,
                                std::size_t n) {
        if (ec)
        {
            std::cout << "Read error. " << ec.message() << '\n';
        }
        std::cout << "Parent unit tests complete" << writeData << ' ' << n
                  << '\n';
        gAppData->ioContext->stop();
    };
    boost::asio::async_read(
        stream, boost::asio::buffer(&writeData, sizeof(writeData)), handler);

    gAppData->ioContext->run();
}

TEST(CollectLog, Success)
{
    // Child process will create dbus objects. EQ_ macros will be called from
    // parent process
    if (gAppData->pid == 0)
    {
        childTask();
    }
    else
    {
        parentTask();
    }
}

int main(int argc, char** argv)
{
    gTestInfo.testId = TestID::collectLog;
    gAppData = std::make_unique<GlobalData>();
    pipe(gAppData->pToC);
    pipe(gAppData->cToP);
    gAppData->pid = fork();
    gAppData->ioContext = std::make_shared<boost::asio::io_context>();
    gAppData->dbusConnection =
        std::make_shared<sdbusplus::asio::connection>(*gAppData->ioContext);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
