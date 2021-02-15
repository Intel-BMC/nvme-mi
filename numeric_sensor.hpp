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

#include <limits>
#include <memory>
#include <sdbusplus/asio/object_server.hpp>
#include <string>

namespace nvmemi
{
/**
 * @brief Represents and NVMe numeric sensor
 *
 */
class NumericSensor
{
  public:
    /**
     * @brief Construct a new Numeric Sensor object
     *
     * @param objServer Reference to sdbusplus object_server to create
     * interfaces
     * @param sensorName Human readable name for the sensor
     * @param min Minimum value for the sensor
     * @param max Maximum value for the sensor
     */
    NumericSensor(sdbusplus::asio::object_server& objServer,
                  const std::string& sensorName,
                  const double min = std::numeric_limits<double>::quiet_NaN(),
                  const double max = std::numeric_limits<double>::quiet_NaN());
    /**
     * @brief Mark sensor as functional or not
     *
     * @param isFunctional Functional state of the sensor
     */
    void markFunctional(const bool isFunctional);

    /**
     * @brief Mark sensor as available or not
     *
     * @param isAvailable Sensor availablity
     */
    void markAvailable(const bool isAvailable);

    /**
     * @brief Update the sensor value
     *
     * @param newValue Sensor value to be set
     */
    void updateValue(const double newValue);

  private:
    std::string name{};
    std::unique_ptr<sdbusplus::asio::dbus_interface> sensorInterface{};
    std::unique_ptr<sdbusplus::asio::dbus_interface> availableInterface{};
    std::unique_ptr<sdbusplus::asio::dbus_interface> operationalInterface{};

    double value{std::numeric_limits<double>::quiet_NaN()};
    double minValue{std::numeric_limits<double>::quiet_NaN()};
    double maxValue{std::numeric_limits<double>::quiet_NaN()};
    size_t errCount{0};

  protected:
    void incrementError();
    void setInitialProperties(const bool sensorDisabled);
};
} // namespace nvmemi