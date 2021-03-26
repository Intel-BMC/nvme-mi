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

#include <cstdint>
#include <stdexcept>

namespace nvmemi::protocol::subsystemhs
{

struct RequestDWord1
{
    uint32_t reserved : 31;
    bool clearStatus : 1;
} __attribute__((packed));

struct ResponseData
{
    struct SubsystemStatus
    {
        uint8_t reservedBit1 : 2;
        bool port1PCIeActive : 1;
        bool port0PCIeActive : 1;
        bool resetNoRequired : 1;
        bool driveFunctional : 1;
        uint8_t reservedBit2 : 2;
    } __attribute__((packed));
    SubsystemStatus subsystemStatus;
    uint8_t smartWarnings;
    uint8_t cTemp;
    uint8_t driveLifeUsed;
    uint16_t ccs;
    uint16_t reserved;
} __attribute__((packed));

static inline int8_t convertToCelsius(uint8_t tempByte)
{
    switch (tempByte)
    {
        case 0x80: {
            throw std::invalid_argument(
                "No temperature data or temperature data "
                "is more the 5 seconds old");
        }
        case 0x81: {
            throw std::invalid_argument("Temperature sensor failure");
        }
        case 0x7F: {
            throw std::invalid_argument("Temperature is 127C or higher");
        }
        case 0xC4: {
            throw std::invalid_argument("Temperature is -60C or lower");
        }
        default: {
            if (0x82 <= tempByte && tempByte <= 0xC3)
            {
                throw std::invalid_argument("Reserved value for temperature");
            }
            break;
        }
    }

    constexpr uint8_t negativeMin = 0xC5;
    constexpr uint8_t negativeMax = 0xFF;
    if (negativeMin <= tempByte && tempByte <= negativeMax)
    {
        auto tempVal = static_cast<int8_t>(-1 * (256 - tempByte));
        return tempVal;
    }
    else
    {
        return static_cast<int8_t>(tempByte);
    }
}

} // namespace nvmemi::protocol::subsystemhs