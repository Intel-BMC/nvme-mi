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
#include "../protocol/nvme_msg.hpp"

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

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
