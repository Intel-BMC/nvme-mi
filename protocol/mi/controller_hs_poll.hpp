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

namespace nvmemi::protocol::controllerhspoll
{
struct DWord0
{
    uint16_t startId;
    uint8_t maxEntries; // 0's based value
    bool includePCIFunctions : 1;
    bool includeSRIOVPhysical : 1;
    bool includeSRIOVVirtual : 1;
    uint8_t rsvd1 : 4;
    bool reportAll : 1;
} __attribute__((packed));
struct DWord1
{
    bool controllerStatusChanges : 1;
    bool compositeTemperatureChanges : 1;
    bool percentageUsed : 1;
    bool availableSpare : 1;
    bool criticalWarning : 1;
    uint32_t rsvd2 : 26;
    bool clearChangedFlags : 1;
} __attribute__((packed));
} // namespace nvmemi::protocol::controllerhspoll
