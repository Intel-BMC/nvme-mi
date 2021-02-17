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

#include <cstdint>
#include <stdexcept>
#include <type_traits>

#include "linux/crc32c.h"

namespace nvmemi::protocol
{
constexpr uint8_t mctpMsgTypeNvmeMI = 0x04;

enum class NVMeMessageTye : uint8_t
{
    controlPrimitive = 0,
    miCommand = 1,
    adminCommand = 2,
    pcieCommand = 4,
};

enum class CommandSlot : uint8_t
{
    slot0 = 0,
    slot1 = 1,
};

struct CommonHeader
{
    uint8_t mctpMsgType : 7;
    bool crcEnabled : 1;
    CommandSlot cmdSlot : 1;
    uint8_t reserved : 2;
    NVMeMessageTye nvmeMiMsgType : 4;
    bool isResponse : 1;
    uint8_t reservedBytes[2];
} __attribute__((packed));

template <typename T>
class NVMeMessage;

template <>
class NVMeMessage<const uint8_t*>
{
  public:
    using CRC32C = uint32_t;
    static constexpr size_t minSize = sizeof(CommonHeader);
    NVMeMessage(const uint8_t* data, size_t len) :
        buffer(reinterpret_cast<const CommonHeader*>(data)), size(len)
    {
        if (nullptr == data)
        {
            throw std::invalid_argument(
                "Null pointer received for NVMeMessage");
        }
        if (len < (minSize + sizeof(CRC32C)))
        {
            throw std::length_error("Expected more bytes for NVMe message");
        }
    }
    template <typename T>
    NVMeMessage(T&& arr) : NVMeMessage(arr.data(), arr.size())
    {
    }
    constexpr const CommonHeader* operator->() const noexcept
    {
        return buffer;
    }
    constexpr bool isCrcEnabled() const noexcept
    {
        return buffer->crcEnabled;
    }
    constexpr bool isResponse() const noexcept
    {
        return buffer->isResponse;
    }
    constexpr uint8_t getMctpMsgType() const noexcept
    {
        return buffer->mctpMsgType;
    }
    constexpr NVMeMessageTye getNvmeMiMsgType() const noexcept
    {
        return buffer->nvmeMiMsgType;
    }
    constexpr CommandSlot getCommandSlot() const noexcept
    {
        return buffer->cmdSlot;
    }

  protected:
    size_t size = 0;

  private:
    const CommonHeader* buffer;
};

template <>
class NVMeMessage<uint8_t*> : public virtual NVMeMessage<const uint8_t*>
{
  public:
    NVMeMessage(uint8_t* data, size_t len) :
        NVMeMessage<const uint8_t*>(data, len),
        buffer(reinterpret_cast<CommonHeader*>(data)), base(data)
    {
    }
    template <typename T>
    NVMeMessage(T&& arr) : NVMeMessage(arr.data(), arr.size())
    {
    }
    NVMeMessage(uint8_t* data, size_t len, NVMeMessageTye msgType,
                CommandSlot csi, bool isRequest) :
        NVMeMessage<uint8_t*>(data, len)
    {
        buffer->cmdSlot = csi;
        buffer->crcEnabled = true;
        buffer->isResponse = !isRequest;
        buffer->mctpMsgType = mctpMsgTypeNvmeMI;
        buffer->nvmeMiMsgType = msgType;
        buffer->reserved = 0x00;
        buffer->reservedBytes[0] = 0x00;
        buffer->reservedBytes[1] = 0x00;
    }
    template <typename T>
    NVMeMessage(T&& arr, NVMeMessageTye msgType, CommandSlot csi,
                bool isRequest) :
        NVMeMessage(arr.data(), arr.size(), msgType, csi, isRequest)
    {
    }
    constexpr CommonHeader* operator->() noexcept
    {
        return buffer;
    }
    constexpr void setCrcEnabled(bool crcEnabled) noexcept
    {
        this->buffer->crcEnabled = crcEnabled;
    }
    constexpr void setIsResponse(bool isResponse) noexcept
    {
        this->buffer->isResponse = isResponse;
    }
    constexpr void setMctpMsgType(uint8_t mctpMsgType) noexcept
    {
        this->buffer->mctpMsgType = mctpMsgType;
    }
    constexpr void setNvmeMiMsgType(NVMeMessageTye nvmeMsgType) noexcept
    {
        this->buffer->nvmeMiMsgType = nvmeMsgType;
    }
    constexpr void setCommandSlot(CommandSlot csi) noexcept
    {
        buffer->cmdSlot = csi;
    }
    void setCRC()
    {
        ssize_t dataSize = size - sizeof(CRC32C);
        if (dataSize <= 0)
        {
            throw std::runtime_error("Not enough space for CRC");
        }
        uint32_t crc = crc32c(base, dataSize);
        auto crcLoc = reinterpret_cast<uint32_t*>(base + dataSize);
        *crcLoc = htole32(crc);
    }

  protected:
    uint8_t* base = nullptr;

  private:
    CommonHeader* buffer;
};

NVMeMessage(const uint8_t*, size_t)->NVMeMessage<const uint8_t*>;
NVMeMessage(uint8_t*, size_t)->NVMeMessage<uint8_t*>;
template <typename T>
NVMeMessage(const T& Arr) -> NVMeMessage<const uint8_t*>;
template <typename T>
NVMeMessage(T& Arr) -> NVMeMessage<uint8_t*>;
template <typename T>
NVMeMessage(T& Arr, NVMeMessageTye, CommandSlot, bool) -> NVMeMessage<uint8_t*>;

} // namespace nvmemi::protocol