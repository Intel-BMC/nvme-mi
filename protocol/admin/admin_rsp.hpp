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

#include "../nvme_rsp.hpp"

namespace nvmemi::protocol
{

template <typename T>
class AdminCommandResponse;

template <>
class AdminCommandResponse<const uint8_t*>
    : public virtual NVMeResponse<const uint8_t*>
{
  protected:
    struct ResponseData
    {
        uint8_t status;
        uint32_t reserved : 24;
        uint32_t cqdword0;
        uint32_t sqdword1;
        uint32_t sqdword3;
    } __attribute__((packed));

  public:
    static constexpr size_t minSize =
        sizeof(CommonHeader) + sizeof(ResponseData);
    AdminCommandResponse(const uint8_t* data, size_t len) :
        NVMeMessage<const uint8_t*>(data, len), NVMeResponse<const uint8_t*>(
                                                    data, len)
    {
        if (len < (minSize + sizeof(CRC32C)))
        {
            throw std::runtime_error(
                "Expected more bytes for AdminCommandResponse response");
        }
        buffer =
            reinterpret_cast<const ResponseData*>(data + sizeof(CommonHeader));
    }
    template <typename T>
    AdminCommandResponse(T&& arr) : AdminCommandResponse(arr.data(), arr.size())
    {
    }
    inline std::pair<const uint8_t*, ssize_t> getResponseData() const noexcept
    {
        return std::make_pair(reinterpret_cast<const uint8_t*>(buffer) +
                                  sizeof(ResponseData),
                              size - (minSize + sizeof(CRC32C)));
    }
    const ResponseData* operator->() const
    {
        return buffer;
    }
    std::pair<const uint8_t*, ssize_t> getAdminResponseData() const noexcept
    {
        return std::make_pair(reinterpret_cast<const uint8_t*>(buffer) +
                                  sizeof(ResponseData),
                              (size - minSize - sizeof(CRC32C)));
    }

  private:
    const ResponseData* buffer = nullptr;
};

template <>
class AdminCommandResponse<uint8_t*>
    : public AdminCommandResponse<const uint8_t*>,
      public virtual NVMeResponse<uint8_t*>
{
  public:
    AdminCommandResponse(uint8_t* data, size_t len) :
        NVMeMessage<const uint8_t*>(data, len), NVMeResponse<const uint8_t*>(
                                                    data, len),
        NVMeResponse<uint8_t*>(data, len), AdminCommandResponse<const uint8_t*>(
                                               data, len)
    {
        buffer = reinterpret_cast<ResponseData*>(data + sizeof(CommonHeader));
    }
    template <typename T>
    AdminCommandResponse(T&& arr) : AdminCommandResponse(arr.data(), arr.size())
    {
    }
    ResponseData* operator->()
    {
        return buffer;
    }

  private:
    ResponseData* buffer;
};

AdminCommandResponse(const uint8_t*, size_t)
    ->AdminCommandResponse<const uint8_t*>;
AdminCommandResponse(uint8_t*, size_t)->AdminCommandResponse<uint8_t*>;
template <typename T>
AdminCommandResponse(const T&) -> AdminCommandResponse<const uint8_t*>;
template <typename T>
AdminCommandResponse(T&) -> AdminCommandResponse<uint8_t*>;

} // namespace nvmemi::protocol