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

#include "numeric_sensor.hpp"

#include <mctp_wrapper.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <string>

namespace nvmemi
{
/**
 * @brief Represents NVMe drive
 *
 */
class Drive
{
  public:
    /**
     * @brief Construct a new Drive object
     *
     * @param driveName Human readable name for the drive
     * @param objServer Existing sdbusplus object_server
     * @param wrapper MCTPWrapper object
     */
    Drive(const std::string& driveName,
          sdbusplus::asio::object_server& objServer,
          std::shared_ptr<mctpw::MCTPWrapper> wrapper);

  private:
    std::string name{};
    std::shared_ptr<mctpw::MCTPWrapper> mctpWrapper{};
    NumericSensor subsystemTemp;
};
} // namespace nvmemi