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

namespace nvmemi::protocol
{
enum class MiOpCode : uint8_t
{
    readDataStructure = 0,
    subsystemHealthStatusPoll = 1,
    controllerHealthStatusPoll = 2,
    configSet = 3,
    configGet = 4,
    vpdRead = 5,
    vpdWrite = 6,
    reset = 7,
};

struct MiMessageHeader
{
    MiOpCode opCode;
    uint8_t reserved[3];
    uint32_t dword0;
    uint32_t dword1;
} __attribute__((packed));

template <typename T>
class ManagementInterfaceMessage;

template <>
class ManagementInterfaceMessage<const uint8_t*>
    : public virtual NVMeMessage<const uint8_t*>
{
  public:
    static constexpr size_t minSize =
        NVMeMessage<const uint8_t*>::minSize + sizeof(MiMessageHeader);
    ManagementInterfaceMessage(const uint8_t* data, size_t len) :
        NVMeMessage<const uint8_t*>(data, len)
    {
        if (len < (minSize + sizeof(CRC32C)))
        {
            throw std::length_error(
                "Expected more bytes for ManagementInterface message");
        }
        buffer = reinterpret_cast<const MiMessageHeader*>(data +
                                                          sizeof(CommonHeader));
    }
    template <typename T>
    ManagementInterfaceMessage(T&& arr) :
        ManagementInterfaceMessage(arr.data(), arr.size())
    {
    }
    const MiMessageHeader* operator->()
    {
        return buffer;
    }
    constexpr MiOpCode getMiOpCode() const noexcept
    {
        return buffer->opCode;
    }
    inline const uint8_t* getDWord0() const noexcept
    {
        return reinterpret_cast<const uint8_t*>(&buffer->dword0);
    }
    inline const uint8_t* getDWord1() const noexcept
    {
        return reinterpret_cast<const uint8_t*>(&buffer->dword1);
    }

  private:
    const MiMessageHeader* buffer = nullptr;
};

template <>
class ManagementInterfaceMessage<uint8_t*>
    : public ManagementInterfaceMessage<const uint8_t*>,
      public NVMeMessage<uint8_t*>
{
  public:
    ManagementInterfaceMessage(uint8_t* data, size_t len) :
        NVMeMessage<const uint8_t*>(data, len), NVMeMessage<uint8_t*>(
                                                    data, len,
                                                    NVMeMessageTye::miCommand,
                                                    CommandSlot::slot0, true),
        ManagementInterfaceMessage<const uint8_t*>(data, len)
    {
        buffer =
            reinterpret_cast<MiMessageHeader*>(data + sizeof(CommonHeader));
    }
    template <typename T>
    ManagementInterfaceMessage(T&& arr) :
        ManagementInterfaceMessage(arr.data(), arr.size())
    {
    }
    ManagementInterfaceMessage(uint8_t* data, size_t len, MiOpCode opCode) :
        ManagementInterfaceMessage<uint8_t*>(data, len)
    {
        setMiOpCode(opCode);
    }
    template <typename T>
    ManagementInterfaceMessage(T&& arr, MiOpCode opCode) :
        ManagementInterfaceMessage(arr.data(), arr.size(), opCode)
    {
    }
    MiMessageHeader* operator->()
    {
        return buffer;
    }
    void setMiOpCode(MiOpCode opCode) noexcept
    {
        this->buffer->opCode = opCode;
        setCRC();
    }
    inline uint8_t* getDWord0() noexcept
    {
        return reinterpret_cast<uint8_t*>(&buffer->dword0);
    }
    inline uint8_t* getDWord1() noexcept
    {
        return reinterpret_cast<uint8_t*>(&buffer->dword1);
    }
    template <typename It>
    inline void setDWord0(It begin, It end)
    {
        auto out = getDWord0();
        std::copy(begin, end, out);
    }
    inline void setDWord0(uint32_t dword0)
    {
        buffer->dword0 = dword0;
    }
    template <typename It>
    inline void setDWord1(It begin, It end)
    {
        auto out = getDWord1();
        std::copy(begin, end, out);
    }
    inline void setDWord1(uint32_t dword1)
    {
        buffer->dword1 = dword1;
    }

  private:
    MiMessageHeader* buffer;
};

ManagementInterfaceMessage(const uint8_t*, size_t)
    ->ManagementInterfaceMessage<const uint8_t*>;
ManagementInterfaceMessage(uint8_t*, size_t)
    ->ManagementInterfaceMessage<uint8_t*>;
template <typename T>
ManagementInterfaceMessage(const T&)
    -> ManagementInterfaceMessage<const uint8_t*>;
template <typename T>
ManagementInterfaceMessage(T&) -> ManagementInterfaceMessage<uint8_t*>;
template <typename T>
ManagementInterfaceMessage(T&, MiOpCode)
    -> ManagementInterfaceMessage<uint8_t*>;

} // namespace nvmemi::protocol