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
namespace nvmemi::protocol
{
enum class FeatureID : uint8_t
{
    arbitration = 0x01,
    power = 0x02,
    temperatureThreshold = 0x04,
    errorRecovery = 0x05,
    numberOfQueues = 0x07,
    interruptCoalescing = 0x08,
    interruptVectorConfiguration = 0x09,
    writeAtomicityNormal = 0x0A,
    asynchronousEventConfiguration = 0x0B,
    // Optional features are not added
};
}