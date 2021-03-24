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

#include <cstdint>

namespace nvmemi::protocol::getlog
{
struct Request
{
    uint8_t logPageId;
    uint8_t logSpecificField : 4;
    uint8_t reserved1 : 3;
    bool retainAsyncEvents : 1;
    uint32_t numberOfDwords;
    uint16_t logSpecificId;
    uint64_t logPageOffset;
    uint8_t uuidIndex : 7;
    uint32_t reserved2 : 25;
} __attribute__((packed));

enum class LogPage : uint8_t
{
    errorInformation = 0x01,
    smartHealthInformation = 0x02,
    firmwareSlotInformation = 0x03,
    changedNamespaceList = 0x04,
    commandsSupportedEffects = 0x05,
    deviceSelfTest = 0x06,
    telemetryHostInitiated = 0x07,
    telemetryControllerInitiated = 0x08,
    enduranceGroupInformation = 0x09,
    predictableLatencyPerNVMSet = 0x0A,
    predictableLatencyEventAggregate = 0x0B,
    asymmetricNamespaceAccess = 0x0C,
    persistentEventLog = 0x0D,
    lbaStatusInformation = 0x0E,
    enduranceGroupEventAggregate = 0x0F,
};
} // namespace nvmemi::protocol::getlog