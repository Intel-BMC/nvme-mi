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

#include "threshold.hpp"

#include <vector>

namespace nvmemi::thresholds
{
/**
 * @brief Check if the list contains any critical threshold
 *
 * @param thresholdVector List of threshold
 * @return true If the list contains any critical threshold
 * @return false if the list doesnt contains any critical threshold
 */
bool hasCriticalInterface(const std::vector<Threshold>& thresholdVector);

/**
 * @brief Check if the list contains any warning threshold
 *
 * @param thresholdVector List of threshold
 * @return true If the list contains any warning threshold
 * @return false if the list doesnt contains any warning threshold
 */
bool hasWarningInterface(const std::vector<Threshold>& thresholdVector);
} // namespace nvmemi::thresholds