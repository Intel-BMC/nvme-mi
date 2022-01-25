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

#include "collectlog_resp_success.hpp"
#include "test_info.hpp"

#include <mctp_wrapper.hpp>

extern TestInfo gTestInfo;

namespace mctpw
{
class MCTPImpl
{
};
} // namespace mctpw

using namespace mctpw;

MCTPConfiguration::MCTPConfiguration(MessageType msgType, BindingType binding) :
    type(msgType), bindingType(binding)
{
}

MCTPConfiguration::MCTPConfiguration(MessageType msgType, BindingType binding,
                                     uint16_t vid, uint16_t vendorMsgType,
                                     uint16_t vendorMsgTypeMask) :
    type(msgType),
    bindingType(binding)
{
    if (MessageType::vdpci != msgType)
    {
        throw std::invalid_argument("MsgType expected VDPCI");
    }
    setVendorDefinedValues(vid, vendorMsgType, vendorMsgTypeMask);
}

MCTPWrapper::MCTPWrapper(boost::asio::io_context& ioContext,
                         const MCTPConfiguration& configIn,
                         const ReconfigurationCallback& networkChangeCb,
                         const ReceiveMessageCallback& rxCb) :
    config(configIn),
    pimpl()
{
}

MCTPWrapper::MCTPWrapper(std::shared_ptr<sdbusplus::asio::connection> conn,
                         const MCTPConfiguration& configIn,
                         const ReconfigurationCallback& networkChangeCb,
                         const ReceiveMessageCallback& rxCb) :
    config(configIn),
    pimpl()
{
}

MCTPWrapper::~MCTPWrapper() noexcept = default;

void MCTPWrapper::detectMctpEndpointsAsync(StatusCallback&& registerCB)
{
    throw std::runtime_error(
        "Not expected to be called from test code. Use yield method");
}

boost::system::error_code
    MCTPWrapper::detectMctpEndpoints(boost::asio::yield_context yield)
{
    return boost::system::error_code();
}

void MCTPWrapper::sendReceiveAsync(ReceiveCallback callback, eid_t dstEId,
                                   const ByteArray& request,
                                   std::chrono::milliseconds timeout)
{
    switch (gTestInfo.testId)
    {
        case TestID::createDrive: {
            std::vector<uint8_t> expected = {
                0x84, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD2, 0xD4, 0x77, 0x36};
            gTestInfo.status =
                std::equal(request.begin(), request.end(), expected.begin());
            ByteArray response = {132, 136, 0,  0, 0, 0, 0,   0,  56, 255,
                                  59,  0,   33, 1, 0, 0, 194, 38, 58, 37};
            callback(boost::system::errc::make_error_code(
                         boost::system::errc::success),
                     response);
        }
        break;
        default:
            throw std::runtime_error("Unknown test case");
    }
}

std::pair<boost::system::error_code, ByteArray>
    MCTPWrapper::sendReceiveYield(boost::asio::yield_context yield,
                                  eid_t dstEId, const ByteArray& request,
                                  std::chrono::milliseconds timeout)
{
    switch (gTestInfo.testId)
    {
        case TestID::createDrive: {
            std::vector<uint8_t> expected = {
                0x84, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD2, 0xD4, 0x77, 0x36};
            gTestInfo.status =
                std::equal(request.begin(), request.end(), expected.begin());
            ByteArray response = {132, 136, 0,  0, 0, 0, 0,   0,  56, 255,
                                  59,  0,   33, 1, 0, 0, 194, 38, 58, 37};
            return std::make_pair(boost::system::error_code(), response);
        }
        break;
        case TestID::highThresholdTest: {
            ByteArray response = {132, 136, 0,    0,    0,    0,   0,
                                  0,   56,  255,  120,  0,    33,  1,
                                  0,   0,   0xF3, 0xB6, 0x82, 0x1F};
            return std::make_pair(boost::system::error_code(), response);
        }
        break;
        case TestID::collectLog: {
            ByteArray response =
                getDummyResponse(request.begin(), request.end());
            return std::make_pair(boost::system::error_code(), response);
        }
        break;
        default:
            throw std::runtime_error("Unknown test case");
    }
    return std::make_pair(
        boost::system::errc::make_error_code(boost::system::errc::timed_out),
        ByteArray());
}

void MCTPWrapper::sendAsync(const SendCallback& callback, const eid_t dstEId,
                            const uint8_t msgTag, const bool tagOwner,
                            const ByteArray& request)
{
    switch (gTestInfo.testId)
    {
        case TestID::createDrive: {
            callback(boost::system::errc::make_error_code(
                         boost::system::errc::success),
                     0);
        }
        break;
        default:
            throw std::runtime_error("Unknown test case");
    }
}

std::pair<boost::system::error_code, int>
    MCTPWrapper::sendYield(boost::asio::yield_context& yield,
                           const eid_t dstEId, const uint8_t msgTag,
                           const bool tagOwner, const ByteArray& request)
{
    return std::pair<boost::system::error_code, int>();
}

const MCTPWrapper::EndpointMap& MCTPWrapper::getEndpointMap()
{
    static MCTPWrapper::EndpointMap map;
    return map;
}
