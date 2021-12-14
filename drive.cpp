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
#include "protocol/admin/admin_cmd.hpp"
#include "protocol/admin/admin_rsp.hpp"
#include "protocol/admin/feature_id.hpp"
#include "protocol/admin/get_log_page.hpp"
#include "protocol/admin/identify.hpp"
#include "protocol/mi/controller_hs_poll.hpp"
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

static constexpr double nvmeTemperatureMin = -128.0;
static constexpr double nvmeTemperatureMax = 127.0;
static const std::chrono::milliseconds normalRespTimeout{600};
static const std::chrono::milliseconds longRespTimeout{3000};
static constexpr uint32_t globalNamespaceId = 0xFFFFFFFF;

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
    mctpWrapper(wrapper),
    subsystemTemp(objServer, driveName + "_Temp", getDefaultThresholds(),
                  nvmeTemperatureMin, nvmeTemperatureMax),
    mctpEid(eid)
{
    std::string objectName = nvmemi::constants::openBmcDBusPrefix + driveName;
    std::string interfaceName =
        nvmemi::constants::interfacePrefix + std::string("drive_log");

    driveLogInterface =
        objServer.add_unique_interface(objectName, interfaceName);

    if (!this->driveLogInterface->register_method(
            "CollectLog", [this](boost::asio::yield_context yield) {
                pausePollRequested = true;
                std::tuple<int, std::string> status;
                try
                {
                    status = this->collectDriveLog(yield);
                }
                catch (std::exception& e)
                {
                    status = std::make_tuple(-1, std::string(e.what()));
                }
                pausePollRequested = false;
                return status;
            }))
    {
        throw std::runtime_error("Register method failed: CollectLog");
    }
    if (!this->driveLogInterface->register_property("EID", eid))
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Error registering EID property");
    }
    driveLogInterface->initialize();
}

template <typename It>
static std::string getHexString(It begin, It end)
{
    std::stringstream ss;
    while (begin < end)
    {
        ss << "0x" << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<int>(*begin) << ' ';
        begin++;
    }
    return ss.str();
}

void Drive::pollSubsystemHealthStatus(boost::asio::yield_context yield)
{
    if (curErrorCount >= maxHealthStatusCount)
    {
        return;
    }
    if (pausePollRequested)
    {
        return;
    }
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
        ++curErrorCount;

        if (curErrorCount == maxHealthStatusCount)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Excluded from the polling, reached max limit",
                phosphor::logging::entry("DRIVE=%s", this->name.c_str()));
        }
        return;
    }
    if (!validateResponse(response))
    {
        ++curErrorCount;
        return;
    }
    curErrorCount = 0;
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
    reqPtr->controllerId = htobe32(controllerId);
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
    if (len < static_cast<ssize_t>(sizeof(SubsystemInfo)))
    {
        throw std::runtime_error("Expected more bytes for subsystem info");
    }
    auto subsystemInfo = reinterpret_cast<const SubsystemInfo*>(data);
    return *subsystemInfo;
}

static std::optional<std::string> getPortInfo(mctpw::MCTPWrapper& wrapper,
                                              mctpw::eid_t eid, uint8_t portId,
                                              boost::asio::yield_context yield)
{
    auto [data, len] = getNVMeDatastructOptionalData(
        wrapper, eid, yield, DataStructureType::portInfo, portId, 0);
    return getHexString(data, data + len);
}

std::vector<uint16_t> getControllerList(mctpw::MCTPWrapper& wrapper,
                                        mctpw::eid_t eid,
                                        boost::asio::yield_context yield)
{
    auto [data, len] = getNVMeDatastructOptionalData(
        wrapper, eid, yield, DataStructureType::controllerList, 0, 0);
    std::vector<uint16_t> controllerList;
    int i = 0;
    if (len % 2 == 1)
    {
        throw std::invalid_argument("Expected even number of bytes");
    }
    for (i = 0; i < len; i = i + sizeof(uint16_t))
    {
        auto controllerId = reinterpret_cast<const uint16_t*>(data + i);
        controllerList.emplace_back(le16toh(*controllerId));
    }
    return controllerList;
}

std::optional<std::string> getControllerInfo(mctpw::MCTPWrapper& wrapper,
                                             mctpw::eid_t eid,
                                             uint16_t controllerId,
                                             boost::asio::yield_context yield)
{
    try
    {
        auto [data, len] = getNVMeDatastructOptionalData(
            wrapper, eid, yield, DataStructureType::controllerInfo, 0,
            controllerId);
        return getHexString(data, data + len);
    }
    catch (const std::exception& e)
    {

        phosphor::logging::log<phosphor::logging::level::WARNING>(
            "Error getting controller information",
            phosphor::logging::entry("MSG=%s", e.what()),
            phosphor::logging::entry("ID=%d", controllerId));
        return std::nullopt;
    }
}

std::vector<std::pair<nvmemi::protocol::NVMeMessageTye, uint8_t>>
    getOptionalCommands(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                        boost::asio::yield_context yield)
{
    static constexpr uint8_t cmdMask = 0x78;
    static constexpr uint8_t cmdIdx = 3;
    std::vector<std::pair<nvmemi::protocol::NVMeMessageTye, uint8_t>>
        optionalCommands;
    auto [data, len] = getNVMeDatastructOptionalData(
        wrapper, eid, yield, DataStructureType::optionalCommands, 0, 0);
    // Optional commands starts from index 2.
    for (int idx = 2; (idx + 1) < len; idx = idx + sizeof(uint16_t))
    {
        optionalCommands.emplace_back(
            static_cast<nvmemi::protocol::NVMeMessageTye>(
                (data[idx] & cmdMask) >> cmdIdx),
            data[idx + 1]);
    }
    return optionalCommands;
}

std::optional<nlohmann::json>
    getControllerHSPollResponse(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                                boost::asio::yield_context yield)
{
    using Request = nvmemi::protocol::ManagementInterfaceMessage<uint8_t*>;
    uint8_t respEntries = 0;
    uint8_t totalRespEntries = 0;
    constexpr uint8_t maximumEntries = 0xFE;
    uint8_t nextStartId = 0;
    constexpr uint8_t maxLoopCount = 32;
    uint8_t responseCount = 0;
    std::string hexString;
    do
    {
        if (++responseCount >= maxLoopCount)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "GetControllerHSPollResponse: exceed limit");
            return std::nullopt;
        }

        std::vector<uint8_t> requestBuffer(
            Request::minSize + sizeof(Request::CRC32C), 0x00);
        Request msg(requestBuffer);
        msg.setMiOpCode(nvmemi::protocol::MiOpCode::controllerHealthStatusPoll);
        auto dword0 =
            reinterpret_cast<nvmemi::protocol::controllerhspoll::DWord0*>(
                msg.getDWord0());

        dword0->maxEntries = maximumEntries;
        dword0->startId = nextStartId;
        dword0->reportAll = true;
        msg.setCRC();

        phosphor::logging::log<phosphor::logging::level::DEBUG>(
            ("GetControllerHSPollResponse request " +
             getHexString(requestBuffer.begin(), requestBuffer.end()))
                .c_str());

        auto [ec, response] = wrapper.sendReceiveYield(
            yield, eid, requestBuffer, normalRespTimeout);
        if (ec)
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                ("GetControllerHSPollResponse: " + ec.message()).c_str());
            return std::nullopt;
        }
        phosphor::logging::log<phosphor::logging::level::DEBUG>(
            ("GetControllerHSPollResponse response " +
             getHexString(response.begin(), response.end()))
                .c_str());

        nvmemi::protocol::ManagementInterfaceResponse miRsp(response);
        auto nvmeMiResponse = miRsp.getNVMeManagementResponse();
        respEntries = nvmeMiResponse.first[2];
        totalRespEntries += respEntries;
        nextStartId += respEntries;

        // TODO Check status code
        auto [data, len] = miRsp.getOptionalResponseData();
        if (len <= 0)
        {
            uint8_t startId = dword0->startId;
            phosphor::logging::log<phosphor::logging::level::WARNING>(
                "GetControllerHSPollResponse: Optional data not found for",
                phosphor::logging::entry("STARTID=%d", startId));

            continue;
        }
        hexString += getHexString(data, data + len);
        phosphor::logging::log<phosphor::logging::level::DEBUG>(
            ("Optional data " + hexString).c_str());
    } while (maximumEntries == respEntries);

    nlohmann::json jsonObject;
    jsonObject["Entries"] = totalRespEntries;
    jsonObject["Data"] = hexString;
    return jsonObject;
}

std::string
    getSubsystemHealthStatusPollResponse(mctpw::MCTPWrapper& wrapper,
                                         mctpw::eid_t eid,
                                         boost::asio::yield_context yield)
{
    using Request = nvmemi::protocol::ManagementInterfaceMessage<uint8_t*>;
    std::vector<uint8_t> requestBuffer(
        Request::minSize + sizeof(Request::CRC32C), 0x00);
    Request msg(requestBuffer);
    msg.setMiOpCode(nvmemi::protocol::MiOpCode::subsystemHealthStatusPoll);
    auto dword1 =
        reinterpret_cast<nvmemi::protocol::subsystemhs::RequestDWord1*>(
            msg.getDWord1());
    dword1->clearStatus = false;
    msg.setCRC();
    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        ("SubsystemHealthStatusPollResponse request " +
         getHexString(requestBuffer.begin(), requestBuffer.end()))
            .c_str());

    auto [ec, response] =
        wrapper.sendReceiveYield(yield, eid, requestBuffer, normalRespTimeout);
    if (ec)
    {
        throw boost::system::system_error(ec);
    }
    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        ("SubsystemHealthStatusPollResponse response " +
         getHexString(response.begin(), response.end()))
            .c_str());

    nvmemi::protocol::ManagementInterfaceResponse miRsp(response);
    // TODO Check status code
    auto [data, len] = miRsp.getOptionalResponseData();
    if (len <= 0)
    {
        throw std::runtime_error("Optional data not found in response");
    }
    return getHexString(data, data + len);
}

std::vector<uint8_t> getNVMeMiResponseData(mctpw::MCTPWrapper& wrapper,
                                           mctpw::eid_t eid,
                                           boost::asio::yield_context yield,
                                           const uint32_t dword0,
                                           const uint32_t dword1 = 0)
{
    using Request = nvmemi::protocol::ManagementInterfaceMessage<uint8_t*>;
    std::vector<uint8_t> requestBuffer(
        Request::minSize + sizeof(Request::CRC32C), 0x00);
    Request msg(requestBuffer, nvmemi::protocol::MiOpCode::configGet);
    msg->dword0 = htole32(dword0);
    msg->dword1 = htole32(dword1);
    msg.setCRC();

    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        ("getNVMeMiResponseData request " +
         getHexString(requestBuffer.begin(), requestBuffer.end()))
            .c_str());

    auto [ec, response] =
        wrapper.sendReceiveYield(yield, eid, requestBuffer, normalRespTimeout);
    if (ec)
    {
        throw boost::system::system_error(ec);
    }
    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        ("getNVMeMiResponseData response " +
         getHexString(response.begin(), response.end()))
            .c_str());

    nvmemi::protocol::ManagementInterfaceResponse miRsp(response);
    if (miRsp.getStatus() != 0)
    {
        throw std::runtime_error("Received error response");
    }

    auto [data, len] = miRsp.getNVMeManagementResponse();
    return std::vector<uint8_t>(data, data + len);
}

uint8_t getSMBusI2CFrequency(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                             boost::asio::yield_context yield, uint8_t portId)
{
    static constexpr uint8_t configGetSMBus = 0x01;
    struct RequestDword
    {
        uint8_t cfgId;
        uint16_t reserved;
        uint8_t portId;
    } __attribute__((packed));
    uint32_t reqData = 0;
    auto dword0 = reinterpret_cast<RequestDword*>(&reqData);
    dword0->cfgId = configGetSMBus;
    dword0->portId = portId;
    auto data = getNVMeMiResponseData(wrapper, eid, yield, reqData);
    return data[0] & 0xF;
}

uint16_t getMCTPTransportUnitSize(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                                  boost::asio::yield_context yield,
                                  uint8_t portId)
{
    static constexpr uint8_t configGetMCTPUnit = 0x03;
    struct RequestDword
    {
        uint8_t cfgId;
        uint16_t reserved;
        uint8_t portId;
    } __attribute__((packed));
    uint32_t reqData = 0;
    auto dword0 = reinterpret_cast<RequestDword*>(&reqData);
    dword0->cfgId = configGetMCTPUnit;
    dword0->portId = portId;
    auto data = getNVMeMiResponseData(wrapper, eid, yield, reqData);
    auto mctpUnitSize = reinterpret_cast<uint16_t*>(data.data());

    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        ("MCTPUnit response " + getHexString(data.begin(), data.end()))
            .c_str());
    return le16toh(*mctpUnitSize);
}

uint32_t getAdminGetFeaturesCQDWord0(mctpw::MCTPWrapper& wrapper,
                                     mctpw::eid_t eid,
                                     boost::asio::yield_context yield,
                                     nvmemi::protocol::FeatureID feature,
                                     uint32_t dword11 = 0)
{
    static constexpr uint32_t namespaceId = 0xFFFFFFFF;
    static constexpr uint8_t selectCurrent = 0x00;
    using Request = nvmemi::protocol::AdminCommand<uint8_t*>;
    std::vector<uint8_t> requestBuffer(
        Request::minSize + sizeof(Request::CRC32C), 0x00);
    Request msg(requestBuffer);
    msg.setAdminOpCode(nvmemi::protocol::AdminOpCode::getFeatures);
    struct DWord10
    {
        uint8_t featureId;
        uint8_t select : 3;
        uint32_t reserved : 21;
    } __attribute__((packed));
    auto dwordPtr = reinterpret_cast<DWord10*>(msg.getSQDword10());
    dwordPtr->featureId = static_cast<uint8_t>(feature);
    dwordPtr->select = selectCurrent;
    msg->sqdword1 = htole32(namespaceId);
    msg->sqdword11 = htole32(dword11);
    msg.setCRC();
    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        ("getAdminGetFeaturesCQDWord0 request " +
         getHexString(requestBuffer.begin(), requestBuffer.end()))
            .c_str());

    auto [ec, response] = wrapper.sendReceiveYield(
        yield, eid, requestBuffer, std::chrono::milliseconds(600));
    if (ec)
    {
        throw boost::system::system_error(ec);
    }
    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        ("getAdminGetFeaturesCQDWord0 response " +
         getHexString(response.begin(), response.end()))
            .c_str());

    nvmemi::protocol::AdminCommandResponse adminRsp(response);
    if (adminRsp.getStatus() != 0)
    {
        throw std::runtime_error("Error status set in response message");
    }
    return adminRsp->cqdword0;
}

template <typename T>
std::string getHexString(const T& val, size_t width = sizeof(T) * 2,
                         char fill = '0')
{
    std::stringstream ss;
    ss << "0x" << std::hex << std::setfill(fill) << std::setw(width) << val;
    return ss.str();
}

template <nvmemi::protocol::FeatureID feature>
std::optional<std::string>
    getFeatureString(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                     boost::asio::yield_context yield, uint32_t dword11 = 0)
{
    try
    {
        auto dword0 =
            getAdminGetFeaturesCQDWord0(wrapper, eid, yield, feature, dword11);
        return getHexString(dword0);
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::WARNING>(
            "Error getting response for get feature",
            phosphor::logging::entry("MSG=%s", e.what()),
            phosphor::logging::entry("FID=%d", feature));
        return std::nullopt;
    }
}

std::optional<std::string> getFeatureTemperatureThreshold(
    mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
    boost::asio::yield_context yield, bool over = true)
{
    struct DWord11
    {
        uint16_t temperatureThreshold;
        uint8_t temperatureSelect : 4;
        uint8_t typeSelect : 2;
        uint16_t reserved : 10;
    } __attribute__((packed));

    uint32_t dword11Val = 0;
    auto dword11Ptr = reinterpret_cast<DWord11*>(&dword11Val);
    dword11Ptr->typeSelect = over ? 0 : 1;
    return getFeatureString<nvmemi::protocol::FeatureID::temperatureThreshold>(
        wrapper, eid, yield, dword11Val);
}

std::optional<std::string>
    getLogPageResponse(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                       boost::asio::yield_context yield,
                       nvmemi::protocol::getlog::LogPage logPageId,
                       uint32_t expectedBytes, uint64_t offset = 0)
{
    try
    {
        constexpr auto logPageTimeout = std::chrono::milliseconds(3000);
        using LogPageRequest = nvmemi::protocol::getlog::Request;
        static constexpr uint32_t namespaceId = 0xFFFFFFFF;
        using Request = nvmemi::protocol::AdminCommand<uint8_t*>;
        std::vector<uint8_t> requestBuffer(
            Request::minSize + sizeof(Request::CRC32C), 0x00);
        Request msg(requestBuffer);
        msg.setAdminOpCode(nvmemi::protocol::AdminOpCode::getLogPage);
        msg.setContainsLength(true);
        if (offset > 0)
        {
            msg.setContainsOffset(true);
            msg.setOffset(offset);
        }
        msg.setLength(expectedBytes);

        auto dwordPtr = reinterpret_cast<LogPageRequest*>(msg.getSQDword10());
        dwordPtr->logPageId = static_cast<uint8_t>(logPageId);
        dwordPtr->numberOfDwords = htole32(expectedBytes / sizeof(uint32_t));
        dwordPtr->logPageOffset = htole64(offset);
        msg->sqdword1 = htole32(namespaceId);
        msg.setCRC();

        phosphor::logging::log<phosphor::logging::level::DEBUG>(
            ("getLogPageResponse request " +
             getHexString(requestBuffer.begin(), requestBuffer.end()))
                .c_str());

        auto [ec, response] =
            wrapper.sendReceiveYield(yield, eid, requestBuffer, logPageTimeout);
        if (ec)
        {
            throw boost::system::system_error(ec);
        }
        phosphor::logging::log<phosphor::logging::level::DEBUG>(
            ("getLogPageResponse response " +
             getHexString(response.begin(), response.end()))
                .c_str());

        nvmemi::protocol::AdminCommandResponse adminRsp(response);
        if (adminRsp.getStatus() != 0)
        {
            throw std::runtime_error("Error status set in response message");
        }
        auto [data, len] = adminRsp.getAdminResponseData();
        if (len <= 0)
        {
            throw std::runtime_error("No data in admin response");
        }
        return getHexString(data, data + len);
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::WARNING>(
            "Error getting response for get log page",
            phosphor::logging::entry("MSG=%s", e.what()),
            phosphor::logging::entry("LID=%d", logPageId));
        return std::nullopt;
    }
}

std::optional<std::string> getLogPageError(mctpw::MCTPWrapper& wrapper,
                                           mctpw::eid_t eid,
                                           boost::asio::yield_context yield)
{
    static constexpr size_t singleErrorPageSize = 64;
    static constexpr size_t errorPages = 2;
    return getLogPageResponse(
        wrapper, eid, yield,
        nvmemi::protocol::getlog::LogPage::errorInformation,
        (errorPages * singleErrorPageSize));
}
std::optional<std::string>
    getLogPageSMARTHealth(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                          boost::asio::yield_context yield)
{
    static constexpr size_t responseSize = 512;
    return getLogPageResponse(
        wrapper, eid, yield,
        nvmemi::protocol::getlog::LogPage::smartHealthInformation,
        responseSize);
}
std::optional<std::string>
    getLogPageFirmwareSlotInfo(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                               boost::asio::yield_context yield)
{
    static constexpr size_t responseSize = 512;
    return getLogPageResponse(
        wrapper, eid, yield,
        nvmemi::protocol::getlog::LogPage::firmwareSlotInformation,
        responseSize);
}
std::optional<std::string>
    getLogPageChangedNamespaces(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                                boost::asio::yield_context yield)
{
    static constexpr size_t responseSize = 1024;
    return getLogPageResponse(
        wrapper, eid, yield,
        nvmemi::protocol::getlog::LogPage::changedNamespaceList, responseSize);
}
std::optional<std::string>
    getLogPageCmdSupportedAndEffects(mctpw::MCTPWrapper& wrapper,
                                     mctpw::eid_t eid,
                                     boost::asio::yield_context yield)
{
    static constexpr size_t responseSize = 2048;
    size_t offset = 0;
    auto rsp1 = getLogPageResponse(
        wrapper, eid, yield,
        nvmemi::protocol::getlog::LogPage::commandsSupportedEffects,
        responseSize);
    if (rsp1)
    {
        offset += 2048;
        auto rsp2 = getLogPageResponse(
            wrapper, eid, yield,
            nvmemi::protocol::getlog::LogPage::commandsSupportedEffects,
            responseSize, offset);
        if (rsp2)
        {
            return rsp1.value() + rsp2.value();
        }
    }
    return std::nullopt;
}
std::optional<std::string>
    getLogPageDeviceSelfTest(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                             boost::asio::yield_context yield)
{
    static constexpr size_t responseSize = 564;
    return getLogPageResponse(wrapper, eid, yield,
                              nvmemi::protocol::getlog::LogPage::deviceSelfTest,
                              responseSize);
}
std::optional<std::string>
    getLogPageTelemetryHostInitiated(mctpw::MCTPWrapper& wrapper,
                                     mctpw::eid_t eid,
                                     boost::asio::yield_context yield)
{
    static constexpr size_t responseSize = 2048;
    return getLogPageResponse(
        wrapper, eid, yield,
        nvmemi::protocol::getlog::LogPage::telemetryHostInitiated,
        responseSize);
}
std::optional<std::string>
    getLogPageTelemetryControllerInitiated(mctpw::MCTPWrapper& wrapper,
                                           mctpw::eid_t eid,
                                           boost::asio::yield_context yield)
{
    static constexpr size_t responseSize = 2048;
    return getLogPageResponse(
        wrapper, eid, yield,
        nvmemi::protocol::getlog::LogPage::telemetryControllerInitiated,
        responseSize);
}
std::optional<std::string>
    getLogPageEnduranceGroupInformation(mctpw::MCTPWrapper& wrapper,
                                        mctpw::eid_t eid,
                                        boost::asio::yield_context yield)
{
    static constexpr size_t responseSize = 512;
    return getLogPageResponse(
        wrapper, eid, yield,
        nvmemi::protocol::getlog::LogPage::enduranceGroupInformation,
        responseSize);
}
std::optional<std::string>
    getLogPagePredictableLatencyPerNVMSet(mctpw::MCTPWrapper& wrapper,
                                          mctpw::eid_t eid,
                                          boost::asio::yield_context yield)
{
    static constexpr size_t responseSize = 512;
    return getLogPageResponse(
        wrapper, eid, yield,
        nvmemi::protocol::getlog::LogPage::predictableLatencyPerNVMSet,
        responseSize);
}
std::optional<std::string>
    getLogPagePredictableLatencyEventAggregate(mctpw::MCTPWrapper& wrapper,
                                               mctpw::eid_t eid,
                                               boost::asio::yield_context yield)
{
    static constexpr size_t responseSize = 1024;
    return getLogPageResponse(
        wrapper, eid, yield,
        nvmemi::protocol::getlog::LogPage::predictableLatencyEventAggregate,
        responseSize);
}
std::optional<std::string>
    getLogPageAsymmetricNamespaceAccess(mctpw::MCTPWrapper& wrapper,
                                        mctpw::eid_t eid,
                                        boost::asio::yield_context yield)
{
    static constexpr size_t responseSize = 1024;
    return getLogPageResponse(
        wrapper, eid, yield,
        nvmemi::protocol::getlog::LogPage::asymmetricNamespaceAccess,
        responseSize);
}
std::optional<std::string>
    getLogPagePersistentEventLog(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                                 boost::asio::yield_context yield)
{
    static constexpr size_t responseSize = 1024;
    // TODO Handle Log Specific Field
    return getLogPageResponse(
        wrapper, eid, yield,
        nvmemi::protocol::getlog::LogPage::persistentEventLog, responseSize);
}
std::optional<std::string>
    getLogPageEnduranceGroupEventAggregate(mctpw::MCTPWrapper& wrapper,
                                           mctpw::eid_t eid,
                                           boost::asio::yield_context yield)
{
    static constexpr size_t responseSize = 1024;
    return getLogPageResponse(
        wrapper, eid, yield,
        nvmemi::protocol::getlog::LogPage::enduranceGroupEventAggregate,
        responseSize);
}

std::optional<std::string> getIdentifyResponse(
    mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
    boost::asio::yield_context yield,
    nvmemi::protocol::identify::ControllerNamespaceStruct cns,
    uint32_t expectedBytes, uint32_t namespaceId, uint16_t controllerId = 0,
    uint32_t offset = 0)
{
    try
    {
        using DWord10 = nvmemi::protocol::identify::DWord10;
        using Request = nvmemi::protocol::AdminCommand<uint8_t*>;
        std::vector<uint8_t> requestBuffer(
            Request::minSize + sizeof(Request::CRC32C), 0x00);
        Request msg(requestBuffer);
        msg.setAdminOpCode(nvmemi::protocol::AdminOpCode::identify);
        msg.setContainsLength(true);
        if (offset > 0)
        {
            msg.setContainsOffset(true);
            msg.setOffset(offset);
        }
        msg.setLength(expectedBytes);

        auto dword10Ptr = reinterpret_cast<DWord10*>(msg.getSQDword10());
        dword10Ptr->cns = static_cast<uint8_t>(cns);
        dword10Ptr->controllerId = htole16(controllerId);
        msg->sqdword1 = htole32(namespaceId);
        msg.setCRC();

        phosphor::logging::log<phosphor::logging::level::DEBUG>(
            ("Identify request " +
             getHexString(requestBuffer.begin(), requestBuffer.end()))
                .c_str());

        auto [ec, response] = wrapper.sendReceiveYield(
            yield, eid, requestBuffer, longRespTimeout);
        if (ec)
        {
            throw boost::system::system_error(ec);
        }
        phosphor::logging::log<phosphor::logging::level::DEBUG>(
            ("Identify response " +
             getHexString(response.begin(), response.end()))
                .c_str());

        nvmemi::protocol::AdminCommandResponse adminRsp(response);
        if (adminRsp.getStatus() != 0)
        {
            throw std::runtime_error("Error status set in response message");
        }
        auto [data, len] = adminRsp.getAdminResponseData();
        if (len <= 0)
        {
            throw std::runtime_error("No data in admin response");
        }
        return getHexString(data, data + len);
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::WARNING>(
            "Error getting response for identify page",
            phosphor::logging::entry("MSG=%s", e.what()),
            phosphor::logging::entry("CNS=%d", cns));
        return std::nullopt;
    }
}

std::vector<uint32_t>
    getIdentifyActiveNamespaceIdList(mctpw::MCTPWrapper& wrapper,
                                     mctpw::eid_t eid,
                                     boost::asio::yield_context yield)
{
    static constexpr uint16_t maxNamespacesExpected = 256;
    static constexpr uint16_t bytesExpected =
        maxNamespacesExpected * sizeof(uint32_t);
    std::vector<uint32_t> nsIds;
    auto rsp = getIdentifyResponse(
        wrapper, eid, yield,
        nvmemi::protocol::identify::ControllerNamespaceStruct::activeNamespace,
        bytesExpected, 0);
    // TODO Continue processing if max namespaces returned
    std::stringstream ss;
    if (rsp)
    {
        // rsp will be a hex string like 0x01 0x00 0x00 0x00 0x02 0x00 0x00..
        ss << rsp.value();
        // Read set of 4 integers from rsp in loop and combine them to
        // make namespace id. namespace id = 0 means end of list.
        while (!ss.eof())
        {
            uint32_t b1 = 0, b2 = 0, b3 = 0, b4 = 0;
            ss << std::hex;
            ss >> b1 >> b2 >> b3 >> b4;
            uint32_t nsId = b1 | (b2 << 8) | (b3 << 16) | (b4 << 24);
            if (nsId == 0)
            {
                break;
            }
            nsIds.emplace_back(nsId);
        }
    }
    return nsIds;
}

std::optional<std::string>
    getIdentifyController(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                          boost::asio::yield_context yield,
                          uint16_t controllerId)
{
    static constexpr uint16_t controllerInfoSize = 536;
    return getIdentifyResponse(
        wrapper, eid, yield,
        nvmemi::protocol::identify::ControllerNamespaceStruct::
            controllerIdentify,
        controllerInfoSize, globalNamespaceId, controllerId);
}

std::optional<std::string>
    getIdentifyCommonNamespace(mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
                               boost::asio::yield_context yield)
{
    static constexpr uint16_t namespaceDescriptorSize = 256;
    return getIdentifyResponse(
        wrapper, eid, yield,
        nvmemi::protocol::identify::ControllerNamespaceStruct::
            namespaceCapablities,
        namespaceDescriptorSize, globalNamespaceId);
}

std::optional<std::string> getIdentifyNamespaceIdDescList(
    mctpw::MCTPWrapper& wrapper, mctpw::eid_t eid,
    boost::asio::yield_context yield, uint32_t nsId)
{
    static constexpr uint16_t bytesExpected = 1024;
    // TODO Handle namespace count greater than 256
    return getIdentifyResponse(
        wrapper, eid, yield,
        nvmemi::protocol::identify::ControllerNamespaceStruct::
            namespaceIdDescriptorList,
        bytesExpected, nsId);
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
    if (subsystemInfo)
    {
        nlohmann::json portInfoJson;
        for (uint8_t currentPort = 0;
             currentPort <= subsystemInfo->numberOfPorts; currentPort++)
        {
            auto portInfo = getPortInfo(*this->mctpWrapper, this->mctpEid,
                                        currentPort, yield);
            if (!portInfo)
            {
                continue;
            }
            portInfoJson["Port" + std::to_string(currentPort)] =
                portInfo.value();
        }
        jsonObject["Ports"] = portInfoJson;
    }
    std::optional<std::vector<uint16_t>> controllerIds;
    try
    {
        auto controllerList =
            getControllerList(*this->mctpWrapper, this->mctpEid, yield);
        controllerIds = controllerList;
        jsonObject["Controllers"] = controllerList;
        nlohmann::json controllerInfoJson;
        for (uint16_t controllerId : controllerList)
        {
            auto controllerHexString = getControllerInfo(
                *this->mctpWrapper, this->mctpEid, controllerId, yield);
            if (controllerHexString)
            {
                controllerInfoJson["Controller" +
                                   std::to_string(controllerId)] =
                    controllerHexString.value();
            }
        }
        jsonObject["ControllerInfo"] = controllerInfoJson;
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::WARNING>(
            "Error getting controller list",
            phosphor::logging::entry("MSG=%s", e.what()));
    }
    try
    {
        auto optionalCommands =
            getOptionalCommands(*this->mctpWrapper, this->mctpEid, yield);
        std::vector<nlohmann::json> optionalCommandsJson{};
        for (const auto& [msgType, cmd] : optionalCommands)
        {
            nlohmann::json cmdJson;
            cmdJson["Type"] = msgType;
            cmdJson["OpCode"] = cmd;
            optionalCommandsJson.emplace_back(cmdJson);
        }
        jsonObject["OptionalCommands"] = optionalCommandsJson;
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::WARNING>(
            "Error getting optional commands",
            phosphor::logging::entry("MSG=%s", e.what()));
    }
    try
    {
        auto controllerHS = getControllerHSPollResponse(*this->mctpWrapper,
                                                        this->mctpEid, yield);
        if (controllerHS)
        {
            jsonObject["ControllerHSPoll"] = controllerHS.value();
        }
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::WARNING>(
            "Error getting controller hs poll",
            phosphor::logging::entry("MSG=%s", e.what()));
    }
    try
    {
        auto subsystemHS = getSubsystemHealthStatusPollResponse(
            *this->mctpWrapper, this->mctpEid, yield);
        jsonObject["SubsystemHSPoll"] = subsystemHS;
    }
    catch (const std::exception& e)
    {
        phosphor::logging::log<phosphor::logging::level::WARNING>(
            "Error getting subsystem hs poll",
            phosphor::logging::entry("MSG=%s", e.what()));
    }
    if (subsystemInfo)
    {
        nlohmann::json portInfoJson;
        for (uint8_t currentPort = 0;
             currentPort <= subsystemInfo->numberOfPorts; currentPort++)
        {
            try
            {
                nlohmann::json configGetJson;
                uint8_t i2cFreq = getSMBusI2CFrequency(
                    *this->mctpWrapper, this->mctpEid, yield, currentPort);
                configGetJson["I2C_SMBus_Frequency"] = i2cFreq;
                uint8_t mctpUnitSize = getMCTPTransportUnitSize(
                    *this->mctpWrapper, this->mctpEid, yield, currentPort);
                configGetJson["MCTP_Unit_Size"] = mctpUnitSize;
                portInfoJson["Port" + std::to_string(currentPort)] =
                    configGetJson;
            }
            catch (const std::exception& e)
            {
                phosphor::logging::log<phosphor::logging::level::WARNING>(
                    "Error getting config get response",
                    phosphor::logging::entry("MSG=%s", e.what()));
            }
        }
        jsonObject["ConfigGet"] = portInfoJson;
    }
    nlohmann::json getFeaturesJson;
    auto arbitration =
        getFeatureString<nvmemi::protocol::FeatureID::arbitration>(
            *this->mctpWrapper, this->mctpEid, yield);
    if (arbitration)
    {
        getFeaturesJson["Arbitration"] = arbitration.value();
    }
    auto tempThresholdUpper =
        getFeatureString<nvmemi::protocol::FeatureID::arbitration>(
            *this->mctpWrapper, this->mctpEid, yield);
    if (tempThresholdUpper)
    {
        getFeaturesJson["ThresholdUpper"] = tempThresholdUpper.value();
    }
    auto tempThresholdLower = getFeatureTemperatureThreshold(
        *this->mctpWrapper, this->mctpEid, yield, false);
    if (tempThresholdLower)
    {
        getFeaturesJson["ThresholdLower"] = tempThresholdLower.value();
    }
    auto powerFeature = getFeatureString<nvmemi::protocol::FeatureID::power>(
        *this->mctpWrapper, this->mctpEid, yield);
    if (powerFeature)
    {
        getFeaturesJson["Power"] = powerFeature.value();
    }
    auto errorRecovery =
        getFeatureString<nvmemi::protocol::FeatureID::errorRecovery>(
            *this->mctpWrapper, this->mctpEid, yield);
    if (errorRecovery)
    {
        getFeaturesJson["ErrorRecovery"] = errorRecovery.value();
    }
    auto numberOfQueues =
        getFeatureString<nvmemi::protocol::FeatureID::numberOfQueues>(
            *this->mctpWrapper, this->mctpEid, yield);
    if (numberOfQueues)
    {
        getFeaturesJson["NumberOfQueues"] = numberOfQueues.value();
    }
    auto interruptCoalescing =
        getFeatureString<nvmemi::protocol::FeatureID::interruptCoalescing>(
            *this->mctpWrapper, this->mctpEid, yield);
    if (interruptCoalescing)
    {
        getFeaturesJson["InterruptCoalescing"] = interruptCoalescing.value();
    }
    auto interruptVector = getFeatureString<
        nvmemi::protocol::FeatureID::interruptVectorConfiguration>(
        *this->mctpWrapper, this->mctpEid, yield);
    if (interruptVector)
    {
        getFeaturesJson["InterruptVector"] = interruptVector.value();
    }
    auto writeAtomicity =
        getFeatureString<nvmemi::protocol::FeatureID::writeAtomicityNormal>(
            *this->mctpWrapper, this->mctpEid, yield);
    if (writeAtomicity)
    {
        getFeaturesJson["WriteAtomicity"] = writeAtomicity.value();
    }
    auto asyncEventConfig = getFeatureString<
        nvmemi::protocol::FeatureID::asynchronousEventConfiguration>(
        *this->mctpWrapper, this->mctpEid, yield);
    if (asyncEventConfig)
    {
        getFeaturesJson["AsyncEventConfig"] = asyncEventConfig.value();
    }
    jsonObject["GetFeatures"] = getFeaturesJson;

    nlohmann::json getLogPage;
    auto logErr = getLogPageError(*this->mctpWrapper, this->mctpEid, yield);
    if (logErr)
    {
        getLogPage["Error"] = logErr.value();
    }
    auto logSmartHealth =
        getLogPageSMARTHealth(*this->mctpWrapper, this->mctpEid, yield);
    if (logSmartHealth)
    {
        getLogPage["SMARTHealth"] = logSmartHealth.value();
    }
    auto logFirmwareSlot =
        getLogPageFirmwareSlotInfo(*this->mctpWrapper, this->mctpEid, yield);
    if (logFirmwareSlot)
    {
        getLogPage["FirmwareSlot"] = logFirmwareSlot.value();
    }
    auto logChangedNamespace =
        getLogPageChangedNamespaces(*this->mctpWrapper, this->mctpEid, yield);
    if (logChangedNamespace)
    {
        getLogPage["ChangedNamespaces"] = logChangedNamespace.value();
    }
    auto logCommandSUpported = getLogPageCmdSupportedAndEffects(
        *this->mctpWrapper, this->mctpEid, yield);
    if (logCommandSUpported)
    {
        getLogPage["CommandSupported"] = logCommandSUpported.value();
    }
    auto logDeviceSelfTest =
        getLogPageDeviceSelfTest(*this->mctpWrapper, this->mctpEid, yield);
    if (logDeviceSelfTest)
    {
        getLogPage["DeviceSelfTest"] = logDeviceSelfTest.value();
    }
    auto logTelemetryHostInitiated = getLogPageTelemetryHostInitiated(
        *this->mctpWrapper, this->mctpEid, yield);
    if (logTelemetryHostInitiated)
    {
        getLogPage["TelemetryHostInitiated"] =
            logTelemetryHostInitiated.value();
    }
    auto logTelemetryControllerInitiated =
        getLogPageTelemetryControllerInitiated(*this->mctpWrapper,
                                               this->mctpEid, yield);
    if (logTelemetryControllerInitiated)
    {
        getLogPage["TelemetryControllerInitiated"] =
            logTelemetryControllerInitiated.value();
    }
    auto logEnduranceGroupInformation = getLogPageEnduranceGroupInformation(
        *this->mctpWrapper, this->mctpEid, yield);
    if (logEnduranceGroupInformation)
    {
        getLogPage["EnduranceGroupInformation"] =
            logEnduranceGroupInformation.value();
    }
    auto logPredictableLatencyPerNVMSet = getLogPagePredictableLatencyPerNVMSet(
        *this->mctpWrapper, this->mctpEid, yield);
    if (logPredictableLatencyPerNVMSet)
    {
        getLogPage["PredictableLatencyPerNVMSet"] =
            logPredictableLatencyPerNVMSet.value();
    }
    auto logPredictableLatencyEventAggregate =
        getLogPagePredictableLatencyEventAggregate(*this->mctpWrapper,
                                                   this->mctpEid, yield);
    if (logPredictableLatencyEventAggregate)
    {
        getLogPage["PredictableLatencyEventAggregate"] =
            logPredictableLatencyEventAggregate.value();
    }
    auto logAsymmetricNamespaceAccess = getLogPageAsymmetricNamespaceAccess(
        *this->mctpWrapper, this->mctpEid, yield);
    if (logAsymmetricNamespaceAccess)
    {
        getLogPage["AsymmetricNamespaceAccess"] =
            logAsymmetricNamespaceAccess.value();
    }
    auto logPersistentEventLog =
        getLogPagePersistentEventLog(*this->mctpWrapper, this->mctpEid, yield);
    if (logPersistentEventLog)
    {
        getLogPage["PersistentEventLog"] = logPersistentEventLog.value();
    }
    auto logEnduranceGroupEventAggregate =
        getLogPageEnduranceGroupEventAggregate(*this->mctpWrapper,
                                               this->mctpEid, yield);
    if (logEnduranceGroupEventAggregate)
    {
        getLogPage["EnduranceGroupEventAggregate"] =
            logEnduranceGroupEventAggregate.value();
    }
    jsonObject["GetLogPage"] = getLogPage;

    nlohmann::json identifyJson;
    auto activeNamespaces = getIdentifyActiveNamespaceIdList(
        *this->mctpWrapper, this->mctpEid, yield);
    identifyJson["ActiveNamespaces"] = activeNamespaces;
    nlohmann::json namespaceJson;
    for (auto nsId : activeNamespaces)
    {
        auto rsp = getIdentifyNamespaceIdDescList(*this->mctpWrapper,
                                                  this->mctpEid, yield, nsId);
        if (rsp)
        {
            namespaceJson["Namespace" + std::to_string(nsId)] = rsp.value();
        }
    }
    identifyJson["NamespaceIdDescList"] = namespaceJson;
    nlohmann::json controllerIdentify;
    if (controllerIds.has_value())
    {
        for (auto cntrlId : controllerIds.value())
        {
            auto rsp = getIdentifyController(*this->mctpWrapper, this->mctpEid,
                                             yield, cntrlId);
            if (rsp)
            {
                controllerIdentify["Controller" + std::to_string(cntrlId)] =
                    rsp.value();
            }
        }
        identifyJson["Controllers"] = controllerIdentify;
    }
    auto namespaceCapablity =
        getIdentifyCommonNamespace(*this->mctpWrapper, this->mctpEid, yield);
    if (namespaceCapablity)
    {
        identifyJson["CommonNamespaceCapablity"] = namespaceCapablity.value();
    }
    jsonObject["Identify"] = identifyJson;

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

bool Drive::validateResponse(const std::vector<uint8_t>& response)
{
    nvmemi::protocol::NVMeResponse respMsg(response);
    if (respMsg.getStatus() !=
        static_cast<uint8_t>(nvmemi::protocol::Status::success))
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "NVMe Response error",
            phosphor::logging::entry("STATUSCODE=%d", respMsg.getStatus()));
        return false;
    }
    return true;
}
