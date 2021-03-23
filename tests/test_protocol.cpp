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
#include "../protocol/admin/admin_cmd.hpp"
#include "../protocol/admin/admin_rsp.hpp"
#include "../protocol/mi/subsystem_hs_poll.hpp"
#include "../protocol/mi_msg.hpp"
#include "../protocol/mi_rsp.hpp"
#include "../protocol/nvme_msg.hpp"
#include "../protocol/nvme_rsp.hpp"

#include <gtest/gtest.h>

TEST(NVMeMsg, Create)
{
    namespace prot = nvmemi::protocol;
    using ConstNVMeMsg = prot::NVMeMessage<const uint8_t*>;
    std::array<uint8_t, 7> data{};
    EXPECT_THROW(ConstNVMeMsg msg(nullptr, 0), std::invalid_argument);
    EXPECT_THROW(prot::NVMeMessage<uint8_t*> msg(nullptr, 0),
                 std::invalid_argument);
    EXPECT_THROW(prot::NVMeMessage msg(data), std::length_error);
    std::array<uint8_t, 8> data2{};
    EXPECT_NO_THROW(prot::NVMeMessage msg(data2));
    std::array<uint8_t, 9> data3{};
    EXPECT_NO_THROW(prot::NVMeMessage msg(data3));
    std::vector<uint8_t> data4(
        ConstNVMeMsg::minSize + sizeof(ConstNVMeMsg::CRC32C), 0x00);
    EXPECT_EQ(data4.size(), 8);
    EXPECT_NO_THROW(prot::NVMeMessage msg(data4));
    {
        std::array<uint8_t, 8> data4{};
        prot::NVMeMessage msg(data4);
        msg.setCRC();

        EXPECT_EQ(data4[4], 0xc7);
        EXPECT_EQ(data4[5], 0x4b);
        EXPECT_EQ(data4[6], 0x67);
        EXPECT_EQ(data4[7], 0x48);

        prot::NVMeMessage msg2(data4, prot::NVMeMessageTye::miCommand,
                               prot::CommandSlot::slot0, true);
        EXPECT_EQ(data4[0], 0x84);
        EXPECT_EQ(data4[1], 0x08);
        msg2.setCRC();
        EXPECT_EQ(data4[4], 0xce);
        EXPECT_EQ(data4[5], 0x8d);
        EXPECT_EQ(data4[6], 0xb4);
        EXPECT_EQ(data4[7], 0x59);
    }
}

TEST(NVMeMsg, Get)
{
    namespace prot = nvmemi::protocol;
    std::array<uint8_t, 8> expected = {0x84, 0x08, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00};
    std::array<uint8_t, 8> testInput{};
    prot::NVMeMessage msg(testInput, prot::NVMeMessageTye::miCommand,
                          prot::CommandSlot::slot0, true);
    EXPECT_EQ(expected, testInput);
    EXPECT_EQ(msg.getMctpMsgType(), prot::mctpMsgTypeNvmeMI);
    EXPECT_EQ(msg.getNvmeMiMsgType(), prot::NVMeMessageTye::miCommand);
    EXPECT_EQ(msg.isCrcEnabled(), true);
    EXPECT_EQ(msg.isResponse(), false);
    EXPECT_EQ(msg.getCommandSlot(), prot::CommandSlot::slot0);
    EXPECT_EQ(msg->nvmeMiMsgType, prot::NVMeMessageTye::miCommand);
    EXPECT_EQ(msg->crcEnabled, true);
}

TEST(NVMeMsg, Set)
{
    namespace prot = nvmemi::protocol;
    std::array<uint8_t, 8> expected = {0x84, 0x08, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00};
    std::array<uint8_t, 8> testInput{};
    prot::NVMeMessage msg(testInput);
    msg.setMctpMsgType(prot::mctpMsgTypeNvmeMI);
    msg.setNvmeMiMsgType(prot::NVMeMessageTye::miCommand);
    msg.setCrcEnabled(true);
    msg.setIsResponse(false);
    msg.setCommandSlot(prot::CommandSlot::slot0);
    EXPECT_EQ(expected, testInput);

    expected[1] = 0x10;
    msg.setNvmeMiMsgType(prot::NVMeMessageTye::adminCommand);
    EXPECT_EQ(expected, testInput);
    expected[1] = 0x90;
    msg.setNvmeMiMsgType(prot::NVMeMessageTye::adminCommand);
    msg.setIsResponse(true);
    EXPECT_EQ(expected, testInput);
    expected[1] = 0x91;
    msg.setCommandSlot(prot::CommandSlot::slot1);
    EXPECT_EQ(expected, testInput);
}

TEST(NVMeRsp, Create)
{
    namespace prot = nvmemi::protocol;
    using ConstResp = prot::NVMeResponse<const uint8_t*>;
    uint8_t* nullPtr = nullptr;
    EXPECT_THROW(prot::NVMeResponse msg(nullPtr, 0), std::invalid_argument);
    EXPECT_THROW(
        prot::NVMeResponse msg(static_cast<const uint8_t*>(nullPtr), 0),
        std::invalid_argument);
    std::array<uint8_t, 3> data{};
    EXPECT_THROW(prot::NVMeResponse msg(data), std::length_error);
    EXPECT_THROW(prot::NVMeResponse msg(
                     static_cast<const uint8_t*>(data.data()), data.size()),
                 std::length_error);
    std::array<uint8_t, 4> data2{};
    EXPECT_ANY_THROW(prot::NVMeResponse msg(data2));
    std::array<uint8_t, 5> data3{};
    EXPECT_ANY_THROW(prot::NVMeResponse msg(data3));
    std::array<uint8_t, 6> data4{};
    EXPECT_ANY_THROW(prot::NVMeResponse msg(data4));
    std::array<uint8_t, ConstResp::minSize + sizeof(ConstResp::CRC32C)> data5 =
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x35, 0x76, 0x72, 0x45};
    EXPECT_NO_THROW(prot::NVMeResponse msg(data5));
    data5[5] = 0x00;
    EXPECT_ANY_THROW(prot::NVMeResponse msg(data5));
    {
        std::array<uint8_t, 12> data5 = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
                                         0x00, 0x00, 0x32, 0x18, 0x6d, 0x51};
        EXPECT_NO_THROW(prot::NVMeResponse msg(data5));
        prot::NVMeResponse msg(data5);
        EXPECT_EQ(msg.getCRC(), 0x516d1832);
        EXPECT_NO_THROW(msg.checkCRC());
    }
}

TEST(NVMeRsp, GetSet)
{
    namespace prot = nvmemi::protocol;
    {
        std::array<uint8_t, 12> data = {0x084, 0x08, 0x00, 0x00, 0x02, 0x00,
                                        0x00,  0x00, 0x51, 0x78, 0x7e, 0x68};
        prot::NVMeResponse msg(data);
        EXPECT_EQ(msg.getStatus(), 0x02);
        EXPECT_EQ(msg.getNvmeMiMsgType(), prot::NVMeMessageTye::miCommand);
    } // namespace nvmemi::protocol;
}

TEST(ManagementIntgerfaceMessage, Create)
{
    namespace prot = nvmemi::protocol;
    using ConstMIMessage = prot::ManagementInterfaceMessage<const uint8_t*>;
    EXPECT_THROW(
        prot::ManagementInterfaceMessage<const uint8_t*> msg(nullptr, 0),
        std::invalid_argument);
    EXPECT_THROW(prot::ManagementInterfaceMessage<uint8_t*> msg(nullptr, 0),
                 std::invalid_argument);
    {
        std::array<uint8_t, 19> data{};
        EXPECT_THROW(prot::ManagementInterfaceMessage msg(data),
                     std::length_error);
        EXPECT_THROW(prot::ManagementInterfaceMessage msg(
                         static_cast<const uint8_t*>(data.data()), data.size()),
                     std::length_error);
        std::array<uint8_t, 20> data2{};
        EXPECT_NO_THROW(prot::ManagementInterfaceMessage msg(data2));
        std::array<uint8_t, 21> data3{};
        EXPECT_NO_THROW(prot::ManagementInterfaceMessage msg(data3));
    } // namespace nvmemi::protocol;
    {
        std::vector<uint8_t> data{};
        data.resize(19);
        EXPECT_ANY_THROW(prot::ManagementInterfaceMessage msg(data));
        data.resize(20);
        EXPECT_NO_THROW(prot::ManagementInterfaceMessage msg(data));
        data.resize(21);
        EXPECT_NO_THROW(prot::ManagementInterfaceMessage msg(data));
        data.resize(ConstMIMessage::minSize + sizeof(ConstMIMessage::CRC32C));
        EXPECT_NO_THROW(prot::ManagementInterfaceMessage msg(data));
    }
}

TEST(ManagementIntgerfaceMessage, GetSet)
{
    namespace prot = nvmemi::protocol;
    {
        const std::vector<uint8_t> expected = {
            0x84, 0x08, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        prot::ManagementInterfaceMessage msg(expected);
        EXPECT_EQ(msg->opCode, prot::MiOpCode::controllerHealthStatusPoll);
        EXPECT_EQ(msg.getMiOpCode(),
                  prot::MiOpCode::controllerHealthStatusPoll);
    } // namespace nvmemi::protocol;
    std::vector<uint8_t> expected = {0x84, 0x08, 0x00, 0x00, 0x02, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x82, 0xa8, 0xe5, 0x65};
    std::vector<uint8_t> testInput(20);
    prot::ManagementInterfaceMessage msg(
        testInput, prot::MiOpCode::controllerHealthStatusPoll);
    EXPECT_EQ(expected, testInput);
    EXPECT_EQ(msg.getNvmeMiMsgType(), prot::NVMeMessageTye::miCommand);

    msg.setMiOpCode(prot::MiOpCode::subsystemHealthStatusPoll);
    expected[4] = 1;
    EXPECT_EQ(expected[4], testInput[4]);
    msg.setCommandSlot(prot::CommandSlot::slot1);
    expected[1] = 0x09;
    EXPECT_EQ(expected[1], testInput[1]);
    auto locDword0 = msg.getDWord0();
    EXPECT_EQ(testInput.data() + 8, locDword0);
    uint32_t dword0 = 0x12345678;
    msg.setDWord0(dword0);
    EXPECT_EQ(locDword0[0], 0x78);
    EXPECT_EQ(locDword0[1], 0x56);
    EXPECT_EQ(locDword0[2], 0x34);
    EXPECT_EQ(locDword0[3], 0x12);
    auto dwordVal = reinterpret_cast<const uint32_t*>(locDword0);
    EXPECT_EQ(le32toh(*dwordVal), dword0);
    std::array<uint8_t, 4> dw0Array = {0x00, 0x01, 0x02, 0x03};
    msg.setDWord0(dw0Array.begin(), dw0Array.end());
    EXPECT_EQ(locDword0[0], 0x00);
    EXPECT_EQ(locDword0[1], 0x01);
    EXPECT_EQ(locDword0[2], 0x02);
    EXPECT_EQ(locDword0[3], 0x03);

    auto locDword1 = msg.getDWord1();
    EXPECT_EQ(testInput.data() + 12, locDword1);
    uint32_t dword1 = 0x23456789;
    msg.setDWord1(dword1);
    EXPECT_EQ(locDword1[0], 0x89);
    EXPECT_EQ(locDword1[1], 0x67);
    EXPECT_EQ(locDword1[2], 0x45);
    EXPECT_EQ(locDword1[3], 0x23);
    dwordVal = reinterpret_cast<const uint32_t*>(locDword1);
    EXPECT_EQ(le32toh(*dwordVal), dword1);
    std::array<uint8_t, 4> dw1Array = {0x01, 0x02, 0x03, 0x04};
    msg.setDWord1(dw1Array.begin(), dw1Array.end());
    EXPECT_EQ(locDword1[0], 0x01);
    EXPECT_EQ(locDword1[1], 0x02);
    EXPECT_EQ(locDword1[2], 0x03);
    EXPECT_EQ(locDword1[3], 0x04);
}

TEST(ManagementIntgerfaceResponse, GetSet)
{
    namespace prot = nvmemi::protocol;
    using ConstMIResponse = prot::ManagementInterfaceResponse<const uint8_t*>;
    using MIResponse = prot::ManagementInterfaceResponse<uint8_t*>;
    uint8_t* nullPtr = nullptr;
    EXPECT_THROW(
        ConstMIResponse msg(nullPtr, ConstMIResponse::minSize +
                                         sizeof(ConstMIResponse::CRC32C)),
        std::invalid_argument);
    EXPECT_THROW(MIResponse msg(nullPtr, ConstMIResponse::minSize +
                                             sizeof(ConstMIResponse::CRC32C)),
                 std::invalid_argument);
    {
        std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x35, 0x76, 0x72, 0x45};
        EXPECT_THROW(ConstMIResponse msg(data), std::length_error);
        EXPECT_THROW(MIResponse msg(data), std::length_error);
        data[0] = 0x01;
        EXPECT_THROW(MIResponse msg(data), std::runtime_error);
    }
    {
        std::array<uint8_t, 14> data1 = {0x84, 0x88, 0x0,  0x0, 0x2,
                                         0x3,  0x4,  0x5,  0x6, 0x7,
                                         0xcd, 0x3b, 0xb3, 0xca};
        EXPECT_NO_THROW(prot::ManagementInterfaceResponse response(data1));

        std::array<uint8_t,
                   ConstMIResponse::minSize + sizeof(ConstMIResponse::CRC32C)>
            data2 = {0x84, 0x88, 0x0,  0x0,  0x2,  0x3,
                     0x4,  0x5,  0x16, 0xc3, 0x45, 0xc};
        EXPECT_NO_THROW(prot::ManagementInterfaceResponse response(data2));
        prot::ManagementInterfaceResponse response(data1);
        {
            auto [rsp, len] = response.getNVMeManagementResponse();
            EXPECT_EQ(len, 3);
            EXPECT_EQ(rsp[0], 0x03);
            EXPECT_EQ(rsp[1], 0x04);
            EXPECT_EQ(rsp[2], 0x05);
        }
        {
            auto [rsp, len] = response.getOptionalResponseData();
            EXPECT_EQ(len, 2);
            EXPECT_EQ(rsp[0], 0x06);
            EXPECT_EQ(rsp[1], 0x07);
        }
    } // namespace nvmemi::protocol;
}

TEST(SubsystemHealthStatusPoll, Struct)
{
    using Request = nvmemi::protocol::subsystemhs::RequestDWord1;
    using Response = nvmemi::protocol::subsystemhs::ResponseData;
    uint8_t reqData[] = {0, 0, 0, 0};
    auto reqPtr = reinterpret_cast<Request*>(reqData);
    reqPtr->clearStatus = true;
    EXPECT_EQ(reqData[3], 0x80);
    reqData[3] = 0xFF;
    reqPtr->clearStatus = false;
    EXPECT_EQ(reqData[3], 0x7F);

    std::array<uint8_t, 8> respData{};
    constexpr uint8_t temperature = 0x12;
    respData[2] = temperature;
    auto respPtr = reinterpret_cast<Response*>(respData.data());
    EXPECT_EQ(respPtr->cTemp, temperature);
    respPtr->cTemp = temperature + 1;
    EXPECT_EQ(respData[2], temperature + 1);
}

TEST(SubsystemHealthStatusPoll, convertToCelsius)
{
    namespace prot = nvmemi::protocol;
    auto func = nvmemi::protocol::subsystemhs::convertToCelsius;
    EXPECT_EQ(func(0x00), 0x00);
    EXPECT_EQ(func(0x7E), 0x7E);
    EXPECT_EQ(func(0x48), 0x48);
    EXPECT_EQ(func(0xC5), -59);
    EXPECT_EQ(func(0xFF), -1);
    EXPECT_EQ(func(0xD8), -40);
    EXPECT_THROW(func(0x7F), std::invalid_argument);
    EXPECT_THROW(func(0x80), std::invalid_argument);
    EXPECT_THROW(func(0x81), std::invalid_argument);
    EXPECT_THROW(func(0xC4), std::invalid_argument);
}

TEST(AdminCommand, Create)
{
    namespace prot = nvmemi::protocol;
    using AdminCmd = prot::AdminCommand<uint8_t*>;
    std::vector<uint8_t> test(AdminCmd::minSize + sizeof(AdminCmd::CRC32C),
                              0x00);
    prot::AdminCommand msg(test, prot::AdminOpCode::identify);
    EXPECT_ANY_THROW(prot::AdminCommand msg2(std::vector<uint8_t>(71, 0)));
    EXPECT_NO_THROW(prot::AdminCommand msg2(std::vector<uint8_t>(72, 0)));

    EXPECT_EQ(msg.getAdminOpCode(), prot::AdminOpCode::identify);
    EXPECT_EQ(test[0], 0x84);
    EXPECT_EQ(test[1], 0x10);
    EXPECT_EQ(test[4], 0x06);
    auto [data, len] = msg.getRequestData();
    EXPECT_EQ(len, 0);
    EXPECT_EQ(test[5], 0x00);
    msg.setContainsLength(true);
    EXPECT_EQ(test[5], 0x01);
    msg.setContainsOffset(true);
    EXPECT_EQ(test[5], 0x03);
    msg.setLength(0x12345678);
    EXPECT_EQ(test[32], 0x78);
    EXPECT_EQ(test[33], 0x56);
    EXPECT_EQ(test[34], 0x34);
    EXPECT_EQ(test[35], 0x12);
    msg.setOffset(0x12345678);
    EXPECT_EQ(test[28], 0x78);
    EXPECT_EQ(test[29], 0x56);
    EXPECT_EQ(test[30], 0x34);
    EXPECT_EQ(test[31], 0x12);
}

TEST(AdminCommandResponse, Create)
{
    std::vector<uint8_t> testData = {0x84, 0x88, 0x0,  0x0,  0x0,  0x0,  0x0,
                                     0x0,  0x0,  0x0,  0x0,  0x0,  0x0,  0x0,
                                     0x0,  0x0,  0x0,  0x0,  0x18, 0x80, 0x12,
                                     0x34, 0x1d, 0x2a, 0x42, 0x49};
    namespace prot = nvmemi::protocol;
    prot::AdminCommandResponse response(testData);
    auto [data, len] = response.getResponseData();
    EXPECT_EQ(len, 2);
    EXPECT_EQ(data[0], 0x12);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
