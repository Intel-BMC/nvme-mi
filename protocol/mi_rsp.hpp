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

#include "nvme_rsp.hpp"

namespace nvmemi::protocol
{

template <typename T>
class ManagementInterfaceResponse;

using NVMeManagementResponse = uint8_t[3];
// TODO Handle response data and crc

template <>
class ManagementInterfaceResponse<const uint8_t*>
    : public virtual NVMeResponse<const uint8_t*>
{
  public:
    static constexpr size_t minSize =
        NVMeResponse<const uint8_t*>::minSize + sizeof(NVMeManagementResponse);
    ManagementInterfaceResponse(const uint8_t* data, size_t len) :
        NVMeMessage<const uint8_t*>(data, len), NVMeResponse<const uint8_t*>(
                                                    data, len)
    {
        if (len < (minSize + sizeof(CRC32C)))
        {
            throw std::length_error(
                "Expected more bytes for ManagementInterface response");
        }
        buffer = reinterpret_cast<const uint8_t*>(data) +
                 NVMeResponse<const uint8_t*>::minSize;
    }
    template <typename T>
    ManagementInterfaceResponse(T&& arr) :
        ManagementInterfaceResponse(arr.data(), arr.size())
    {
    }
    constexpr std::pair<const uint8_t*, size_t>
        getNVMeManagementResponse() const noexcept
    {
        return std::make_pair(buffer, miRspLength);
    }
    constexpr std::pair<const uint8_t*, ssize_t>
        getOptionalResponseData() const noexcept
    {
        return std::make_pair(buffer + miRspLength,
                              size - (minSize + sizeof(CRC32C)));
    }

  protected:
    static constexpr size_t miRspLength = sizeof(NVMeManagementResponse);
    size_t responseDataLength = 0;

  private:
    const uint8_t* buffer = nullptr;
};

template <>
class ManagementInterfaceResponse<uint8_t*>
    : public ManagementInterfaceResponse<const uint8_t*>,
      public virtual NVMeResponse<uint8_t*>
{
  public:
    ManagementInterfaceResponse(uint8_t* data, size_t len) :
        NVMeMessage<const uint8_t*>(data, len),
        NVMeResponse<const uint8_t*>(data, len), NVMeResponse<uint8_t*>(data,
                                                                        len),
        ManagementInterfaceResponse<const uint8_t*>(data, len)
    {
        buffer = reinterpret_cast<uint8_t*>(data) +
                 NVMeResponse<const uint8_t*>::minSize;
    }
    template <typename T>
    ManagementInterfaceResponse(T&& arr) :
        ManagementInterfaceResponse(arr.data(), arr.size())
    {
    }
    constexpr std::pair<uint8_t*, size_t> getNVMeManagementResponse() noexcept
    {
        return std::make_pair(buffer, miRspLength);
    }

  private:
    uint8_t* buffer;
};

ManagementInterfaceResponse(const uint8_t*, size_t)
    ->ManagementInterfaceResponse<const uint8_t*>;
ManagementInterfaceResponse(uint8_t*, size_t)
    ->ManagementInterfaceResponse<uint8_t*>;
template <typename T>
ManagementInterfaceResponse(const T&)
    -> ManagementInterfaceResponse<const uint8_t*>;
template <typename T>
ManagementInterfaceResponse(T&) -> ManagementInterfaceResponse<uint8_t*>;
template <typename T>
ManagementInterfaceResponse(T&) -> ManagementInterfaceResponse<uint8_t*>;

} // namespace nvmemi::protocol