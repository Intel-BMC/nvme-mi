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

namespace nvmemi::protocol::identify
{
struct DWord10
{
    uint8_t cns;
    uint8_t reserved;
    uint16_t controllerId;
} __attribute__((packed));
struct DWord11
{
    uint16_t nvmSetId;
    uint16_t reserved;
} __attribute__((packed));
struct DWord14
{
    uint8_t uuidIndex : 7;
    uint32_t reserved : 25;
} __attribute__((packed));

enum class ControllerNamespaceStruct : uint8_t
{
    namespaceCapablities = 0x00,
    controllerIdentify = 0x01,
    activeNamespace = 0x02,
    namespaceIdDescriptorList = 0x03,
};
} // namespace nvmemi::protocol::identify