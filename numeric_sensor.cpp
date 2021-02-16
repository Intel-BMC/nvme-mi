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

#include "numeric_sensor.hpp"

#include "threshold_helper.hpp"

#include <phosphor-logging/log.hpp>
#include <regex>

using nvmemi::NumericSensor;

static constexpr const char* objPathTemperature =
    "/xyz/openbmc_project/sensors/temperature/";
static constexpr const char* availableInterfaceName =
    "xyz.openbmc_project.State.Decorator.Availability";
static constexpr const char* operationalInterfaceName =
    "xyz.openbmc_project.State.Decorator.OperationalStatus";
static constexpr const char* sensorInterfaceName =
    "xyz.openbmc_project.Sensor.Value";
constexpr const size_t errorThreshold = 5;

NumericSensor::NumericSensor(sdbusplus::asio::object_server& objServer,
                             const std::string& sensorName,
                             std::vector<thresholds::Threshold> thresholdVals,
                             const double min, const double max) :
    name(std::regex_replace(sensorName, std::regex("[^a-zA-Z0-9_/]+"), "_")),
    thresholds(std::move(thresholdVals)), minValue(min), maxValue(max)
{
    std::string currentObjectPath = objPathTemperature + name;
    sensorInterface =
        objServer.add_unique_interface(currentObjectPath, sensorInterfaceName);
    availableInterface = objServer.add_unique_interface(currentObjectPath,
                                                        availableInterfaceName);
    operationalInterface = objServer.add_unique_interface(
        currentObjectPath, operationalInterfaceName);

    if (thresholds::hasWarningInterface(thresholds))
    {
        thresholdInterfaceWarning = objServer.add_unique_interface(
            currentObjectPath, "xyz.openbmc_project.Sensor.Threshold.Warning");
    }
    if (thresholds::hasCriticalInterface(thresholds))
    {
        thresholdInterfaceCritical = objServer.add_unique_interface(
            currentObjectPath, "xyz.openbmc_project.Sensor.Threshold.Critical");
    }
    setInitialProperties(false);
}

void NumericSensor::markFunctional(bool isFunctional)
{
    operationalInterface->set_property("Functional", isFunctional);

    if (isFunctional)
    {
        errCount = 0;
    }
    else
    {
        updateValue(std::numeric_limits<double>::quiet_NaN());
    }
}

void NumericSensor::markAvailable(bool isAvailable)
{
    availableInterface->set_property("Available", isAvailable);
    errCount = 0;
}

void NumericSensor::updateValue(const double newValue)
{
    if (!sensorInterface->set_property("Value", newValue))
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            ("Error setting property: Sensor Value "),
            phosphor::logging::entry("VALUE=%l", newValue));
    }

    // TDOD Check threshols
}

void NumericSensor::incrementError()
{
    if (errCount >= errorThreshold)
    {
        return;
    }

    errCount++;
    if (errCount == errorThreshold)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            ("Sensor " + name + " reading error").c_str());
        markFunctional(false);
    }
}

void NumericSensor::setInitialProperties(const bool sensorDisabled)
{
    sensorInterface->register_property("MaxValue", maxValue);
    sensorInterface->register_property("MinValue", minValue);
    sensorInterface->register_property("Value", value);
    sensorInterface->initialize();

    availableInterface->register_property(
        "Available", true, [this](const bool propIn, bool& old) {
            if (propIn == old)
            {
                return 1;
            }
            old = propIn;
            if (!propIn)
            {
                updateValue(std::numeric_limits<double>::quiet_NaN());
            }
            return 1;
        });
    availableInterface->initialize();

    operationalInterface->register_property("Functional", !sensorDisabled);
    operationalInterface->initialize();

    for (const auto& threshold : thresholds)
    {
        auto thresholdIntf = selectThresholdInterface(threshold);
        if (!thresholdIntf)
        {
            continue;
        }

        // Interface is 0th tuple member
        if (!thresholdIntf->iface->register_property(thresholdIntf->level,
                                                     threshold.value))
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Error registering threshold level property");
        }
        if (!thresholdIntf->iface->register_property(thresholdIntf->alarm,
                                                     false))
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Error registering threshold alarm property");
        }
    }

    if (thresholdInterfaceWarning &&
        !thresholdInterfaceWarning->initialize(true))
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error initializing warning threshold interface");
    }

    if (thresholdInterfaceCritical &&
        !thresholdInterfaceCritical->initialize(true))
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error initializing critical threshold interface");
    }
}

std::optional<NumericSensor::ThresholdInterface>
    NumericSensor::selectThresholdInterface(
        const thresholds::Threshold& threshold)
{
    switch (threshold.level)
    {
        case thresholds::Level::critical: {
            switch (threshold.direction)
            {
                case thresholds::Direction::high:
                    return ThresholdInterface{thresholdInterfaceCritical,
                                              "CriticalHigh",
                                              "CriticalAlarmHigh"};
                case thresholds::Direction::low:
                    return ThresholdInterface{thresholdInterfaceCritical,
                                              "CriticalLow",
                                              "CriticalAlarmLow"};
                default:
                    return std::nullopt;
            }
        }
        case thresholds::Level::warning: {
            switch (threshold.direction)
            {
                case thresholds::Direction::high:
                    return ThresholdInterface{thresholdInterfaceWarning,
                                              "WarningHigh",
                                              "WarningAlarmHigh"};
                case thresholds::Direction::low:
                    return ThresholdInterface{thresholdInterfaceWarning,
                                              "WarningLow", "WarningAlarmLow"};
                default:
                    return std::nullopt;
            }
        }
        default:
            return std::nullopt;
    }
}