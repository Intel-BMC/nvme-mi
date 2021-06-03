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

#include "nvme_msg.hpp"

#include <sstream>

namespace nvmemi::protocol
{

enum class Status : uint8_t
{
    success = 0,
};

template <typename T>
class NVMeResponse;

using StatusCode = uint8_t;

template <>
class NVMeResponse<const uint8_t*> : public virtual NVMeMessage<const uint8_t*>
{
  public:
    static constexpr size_t minSize =
        NVMeMessage<const uint8_t*>::minSize + sizeof(StatusCode);

    NVMeResponse(const uint8_t* data, size_t len) :
        NVMeMessage<const uint8_t*>(data, len),
        status(reinterpret_cast<const uint8_t*>(
            data + NVMeMessage<const uint8_t*>::minSize)),
        base(data)
    {
        if (len < (minSize + sizeof(CRC32C)))
        {
            throw std::length_error("Expected more bytes for NVMe MI response");
        }
        checkCRC();
    }
    template <typename T>
    NVMeResponse(T&& arr) : NVMeResponse(arr.data(), arr.size())
    {
    }
    constexpr uint8_t getStatus() const noexcept
    {
        return *status;
    }
    bool checkCRC(bool nothrow = false) const
    {
        ssize_t dataSize = size - sizeof(uint32_t);
        if (dataSize <= 0)
        {
            throw std::runtime_error("Not enough space for CRC");
        }
        uint32_t crc = crc32c(base, dataSize);
        auto crcLoc = reinterpret_cast<const uint32_t*>(base + dataSize);
        if (le32toh(*crcLoc) == crc)
        {
            return true;
        }
        if (!nothrow)
        {
            std::stringstream ss;
            ss << "CRC Mismatch . Data CRC ";
            ss << std::hex << le32toh(*crcLoc) << ". Computed ";
            ss << std::hex << crc;
            throw std::runtime_error(ss.str());
        }
        return false;
    }
    uint32_t getCRC() const
    {
        ssize_t dataSize = size - sizeof(uint32_t);
        if (dataSize < 0)
        {
            throw std::runtime_error("Not enough space for CRC");
        }
        auto crcLoc = reinterpret_cast<const uint32_t*>(base + dataSize);
        return le32toh(*crcLoc);
    }

  protected:
    const uint8_t* base = nullptr;

  private:
    // TODO Define error status enum
    const StatusCode* status;
};

template <>
class NVMeResponse<uint8_t*> : public virtual NVMeResponse<const uint8_t*>,
                               public NVMeMessage<uint8_t*>
{
  public:
    NVMeResponse(uint8_t* data, size_t len) :
        NVMeMessage<const uint8_t*>(data, len),
        NVMeResponse<const uint8_t*>(data, len), NVMeMessage<uint8_t*>(data,
                                                                       len),
        status(reinterpret_cast<uint8_t*>(data +
                                          NVMeMessage<const uint8_t*>::minSize))
    {
    }
    template <typename T>
    NVMeResponse(T&& arr) : NVMeResponse(arr.data(), arr.size())
    {
    }
    NVMeResponse(uint8_t* data, size_t len, uint8_t errStatus) :
        NVMeResponse<uint8_t*>(data, len)
    {
        *status = errStatus;
    }
    template <typename T>
    NVMeResponse(T&& arr, uint8_t errStatus) :
        NVMeResponse(arr.data(), arr.size(), errStatus)
    {
    }

  private:
    StatusCode* status;
};

NVMeResponse(const uint8_t*, size_t)->NVMeResponse<const uint8_t*>;
NVMeResponse(uint8_t*, size_t)->NVMeResponse<uint8_t*>;
template <typename T>
NVMeResponse(const T& Arr) -> NVMeResponse<const uint8_t*>;
template <typename T>
NVMeResponse(T& Arr) -> NVMeResponse<uint8_t*>;
template <typename T>
NVMeResponse(T& Arr, uint8_t) -> NVMeResponse<uint8_t*>;

} // namespace nvmemi::protocol
