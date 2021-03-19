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

#include "drive.hpp"

#include "constants.hpp"
#include "protocol/mi/read_nvmemi_ds.hpp"
#include "protocol/mi/subsystem_hs_poll.hpp"
#include "protocol/mi_msg.hpp"
#include "protocol/mi_rsp.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <phosphor-logging/log.hpp>
#include <regex>

using nvmemi::Drive;
using nvmemi::thresholds::Threshold;
using DataStructureType = nvmemi::protocol::readnvmeds::DataStructureType;

static constexpr double nvmeTemperatureMin = -60.0;
static constexpr double nvmeTemperatureMax = 127.0;
static const std::chrono::milliseconds normalRespTimeout{600};

static std::vector<Threshold> getDefaultThresholds()
{
    using nvmemi::thresholds::Direction;
    using nvmemi::thresholds::Level;
    // Using hardcoded values temporarily.
    std::vector<Threshold> thresholds{
        Threshold(Level::critical, Direction::high, 115.0),
        Threshold(Level::critical, Direction::low, 0.0),
        Threshold(Level::warning, Direction::high, 110.0),
        Threshold(Level::warning, Direction::low, 5.0)};
    return thresholds;
}

Drive::Drive(const std::string& driveName, mctpw::eid_t eid,
             sdbusplus::asio::object_server& objServer,
             std::shared_ptr<mctpw::MCTPWrapper> wrapper) :
    name(std::regex_replace(driveName, std::regex("[^a-zA-Z0-9_/]+"), "_")),
    subsystemTemp(objServer, driveName + "_Temp", getDefaultThresholds(),
                  nvmeTemperatureMin, nvmeTemperatureMax),
    mctpEid(eid), mctpWrapper(wrapper)
{
    std::string objectName = nvmemi::constants::openBmcDBusPrefix + driveName;
    std::string interfaceName =
        nvmemi::constants::interfacePrefix + std::string("drive_log");

    driveLogInterface =
        objServer.add_unique_interface(objectName, interfaceName);

    if (!this->driveLogInterface->register_method(
            "CollectLog", [this](boost::asio::yield_context yield) {
                return this->collectDriveLog(yield);
            }))
    {
        throw std::runtime_error("Register method failed: CollectLog");
    }
    driveLogInterface->initialize();
}

template <typename It>
static std::string getHexString(It begin, It end)
{
    std::stringstream ss;
    while (begin < end)
    {
        ss << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<int>(*begin) << ' ';
        begin++;
    }
    return ss.str();
}

void Drive::pollSubsystemHealthStatus(boost::asio::yield_context yield)
{
    using Message = nvmemi::protocol::ManagementInterfaceMessage<uint8_t*>;
    using DWord1 = nvmemi::protocol::subsystemhs::RequestDWord1;
    using Response = nvmemi::protocol::subsystemhs::ResponseData;
    static constexpr size_t reqSize =
        Message::minSize + sizeof(Message::CRC32C);
    std::vector<uint8_t> reqBuffer(reqSize, 0x00);
    nvmemi::protocol::ManagementInterfaceMessage reqMsg(reqBuffer);
    reqMsg.setMiOpCode(nvmemi::protocol::MiOpCode::subsystemHealthStatusPoll);
    auto dword1 = reinterpret_cast<DWord1*>(reqMsg.getDWord1());
    dword1->clearStatus = false;
    reqMsg.setCRC();
    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        getHexString(reqBuffer.begin(), reqBuffer.end()).c_str());

    auto [ec, response] = mctpWrapper->sendReceiveYield(
        yield, this->mctpEid, reqBuffer, hsPollTimeout);
    if (ec)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Poll Subsystem health status error",
            phosphor::logging::entry("MSG=%s", ec.message().c_str()));
        return;
    }
    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        getHexString(response.begin(), response.end()).c_str());

    try
    {
        nvmemi::protocol::ManagementInterfaceResponse respMsg(response);
        auto [optData, len] = respMsg.getOptionalResponseData();
        if (len <= 0)
        {
            throw std::runtime_error("Optional data not found");
        }
        auto respPtr = reinterpret_cast<const Response*>(optData);
        auto temperature =
            nvmemi::protocol::subsystemhs::convertToCelsius(respPtr->cTemp);
        this->subsystemTemp.updateValue(temperature);
        this->logCWarnState(respPtr->ccs.criticalWarning);
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::WARNING>(
            (std::string("NVM Poll error. ") + e.what()).c_str());
    }
}

void Drive::logCWarnState(bool cwarn)
{
    if (this->cwarnState == cwarn)
    {
        return;
    }
    this->cwarnState = cwarn;
    static const char* messageIdWarning = "OpenBMC.0.1.StateSensorWarning";
    static const char* messageIdNormal = "OpenBMC.0.1.StateSensorNormal";
    std::string message;
    if (this->cwarnState)
    {
        message = "Controller health status warning asserted in " + this->name;
        phosphor::logging::log<phosphor::logging::level::WARNING>(
            message.c_str(),
            phosphor::logging::entry("REDFISH_MESSAGE_ID=%s", messageIdWarning),
            phosphor::logging::entry("REDFISH_MESSAGE_ARGS=%s,%s,%s,%s",
                                     "NVM Subsystem", this->name.c_str(),
                                     "False", "True"));
    }
    else
    {
        message =
            "Controller health status warning de-asserted in " + this->name;
        phosphor::logging::log<phosphor::logging::level::INFO>(
            message.c_str(),
            phosphor::logging::entry("REDFISH_MESSAGE_ID=%s", messageIdNormal),
            phosphor::logging::entry("REDFISH_MESSAGE_ARGS=%s,%s,%s,%s",
                                     "NVM Subsystem", this->name.c_str(),
                                     "True", "False"));
    }
}

static std::pair<const uint8_t*, ssize_t>
    getNVMeDatastructOptionalData(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                                  boost::asio::yield_context yield,
                                  DataStructureType dsType, uint8_t portId,
                                  uint16_t controllerId)
{
    using MIRequest =
        nvmemi::protocol::ManagementInterfaceMessage<const uint8_t*>;
    std::vector<uint8_t> requestBuffer(
        MIRequest::minSize + sizeof(MIRequest::CRC32C), 0x00);
    nvmemi::protocol::ManagementInterfaceMessage reqMsg(requestBuffer);
    reqMsg.setMiOpCode(nvmemi::protocol::MiOpCode::readDataStructure);

    auto reqPtr = reinterpret_cast<nvmemi::protocol::readnvmeds::RequestData*>(
        reqMsg.getDWord0());
    reqPtr->controllerId = controllerId;
    reqPtr->dataStructureType = dsType;
    reqPtr->portId = portId;
    reqMsg.setCRC();

    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        ("ReadNVMe data structure request " +
         getHexString(requestBuffer.begin(), requestBuffer.end()))
            .c_str());

    auto [ec, response] =
        wrapper.sendReceiveYield(yield, eid, requestBuffer, normalRespTimeout);
    if (ec)
    {
        throw boost::system::system_error(ec);
    }

    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        ("ReadNVMe data structure response " +
         getHexString(response.begin(), response.end()))
            .c_str());

    nvmemi::protocol::ManagementInterfaceResponse miRsp(response);
    // TODO Check status code
    auto [data, len] = miRsp.getOptionalResponseData();
    if (len <= 0)
    {
        throw std::runtime_error("Optional data not found in response");
    }

    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        ("Optional data " + getHexString(data, data + len)).c_str());

    return std::make_pair(data, len);
}

static nvmemi::protocol::readnvmeds::SubsystemInfo
    getSubsystemInfo(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                     boost::asio::yield_context yield)
{
    auto [data, len] = getNVMeDatastructOptionalData(
        wrapper, eid, yield, DataStructureType::nvmSubsystemInfo, 0, 0);
    using SubsystemInfo = nvmemi::protocol::readnvmeds::SubsystemInfo;
    if (len < sizeof(SubsystemInfo))
    {
        throw std::runtime_error("Expected more bytes for subsystem info");
    }
    auto subsystemInfo = reinterpret_cast<const SubsystemInfo*>(data);
    return *subsystemInfo;
}

std::tuple<int, std::string>
    Drive::collectDriveLog(boost::asio::yield_context yield)
{
    enum ErrorStatus : uint8_t
    {
        success = 0,
        fileSystem,
        emptyJson,
    };

    nlohmann::json jsonObject;
    std::optional<nvmemi::protocol::readnvmeds::SubsystemInfo> subsystemInfo =
        std::nullopt;
    try
    {
        subsystemInfo =
            getSubsystemInfo(*this->mctpWrapper, this->mctpEid, yield);
        nlohmann::json subsystemJson;
        subsystemJson["Major"] = static_cast<int>(subsystemInfo->majorVersion);
        subsystemJson["Minor"] = static_cast<int>(subsystemInfo->minorVersion);
        subsystemJson["Ports"] =
            static_cast<int>(subsystemInfo->numberOfPorts + 1);
        jsonObject["NVM_Subsystem_Info"] = subsystemJson;
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::WARNING>(
            "Error getting NVM subsystem information",
            phosphor::logging::entry("MSG=%s", e.what()));
    }
    if (jsonObject.empty())
    {
        std::make_tuple(ErrorStatus::emptyJson,
                        "All commands failed to get response");
    }

    unsigned long fileCount =
        std::chrono::system_clock::now().time_since_epoch() /
        std::chrono::milliseconds(1);
    std::string fileName =
        "/tmp/nvmemi_jsondump_" + std::to_string(fileCount) + ".json";
    std::fstream jsonDump(fileName, std::ios::out);
    if (!jsonDump.is_open())
    {
        std::string errString =
            "Error opening " + fileName + ". " + strerror(errno);
        return std::make_tuple(ErrorStatus::fileSystem, errString);
    }
    jsonDump << jsonObject.dump(2, ' ', true,
                                nlohmann::json::error_handler_t::replace);
    return std::make_tuple(ErrorStatus::success, fileName);
}
