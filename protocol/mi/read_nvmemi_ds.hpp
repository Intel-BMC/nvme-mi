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

#pragma once

#include "../mi_msg.hpp"

namespace nvmemi::protocol::readnvmeds
{
enum class DataStructureType : uint8_t
{
    nvmSubsystemInfo = 0x00,
    portInfo = 0x01,
    controllerList = 0x02,
    controllerInfo = 0x03,
    optionalCommands = 0x04,
    reserverd = 0x05
};

struct RequestData
{
    uint16_t controllerId;
    uint8_t portId;
    DataStructureType dataStructureType;
} __attribute__((packed));

struct SubsystemInfo
{
    uint8_t numberOfPorts;
    uint8_t majorVersion;
    uint8_t minorVersion;
    uint8_t reserverd[29];
} __attribute__((packed));

} // namespace nvmemi::protocol::readnvmeds