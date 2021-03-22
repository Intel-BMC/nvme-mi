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

#include "../nvme_msg.hpp"

namespace nvmemi::protocol
{
enum class AdminOpCode : uint8_t
{
    getLogPage = 0x02,
    identify = 0x06,
    getFeatures = 0x0A,
};

struct AdminCommandHeader
{
    struct CommandFlags
    {
        bool containsLength : 1;
        bool containsOffset : 1;
        uint8_t reserved : 6;
    } __attribute__((packed));
    AdminOpCode opCode;
    CommandFlags cmdFlags;
    uint16_t controllerId;
    uint32_t sqdword1;
    uint32_t sqdword2;
    uint32_t sqdword3;
    uint32_t sqdword4;
    uint32_t sqdword5;
    uint32_t offset;
    uint32_t length;
    uint64_t reserved;
    uint32_t sqdword10;
    uint32_t sqdword11;
    uint32_t sqdword12;
    uint32_t sqdword13;
    uint32_t sqdword14;
    uint32_t sqdword15;
} __attribute__((packed));

template <typename T>
class AdminCommand;

template <>
class AdminCommand<const uint8_t*> : public virtual NVMeMessage<const uint8_t*>
{
  public:
    static constexpr size_t minSize =
        NVMeMessage<const uint8_t*>::minSize + sizeof(AdminCommandHeader);
    AdminCommand(const uint8_t* data, size_t len) :
        NVMeMessage<const uint8_t*>(data, len)
    {
        if (len < (minSize + sizeof(CRC32C)))
        {
            throw std::runtime_error(
                "Expected more bytes for AdminCommand message");
        }
        buffer = reinterpret_cast<const AdminCommandHeader*>(
            data + sizeof(CommonHeader));
    }
    template <typename T>
    AdminCommand(T&& arr) : AdminCommand(arr.data(), arr.size())
    {
    }
    const AdminCommandHeader* operator->()
    {
        return buffer;
    }
    inline std::pair<const uint8_t*, ssize_t> getRequestData() const noexcept
    {
        ssize_t len = (size - (minSize + sizeof(CRC32C)));
        return std::make_pair(reinterpret_cast<const uint8_t*>(buffer) +
                                  sizeof(AdminCommandHeader),
                              len);
    }

    constexpr AdminOpCode getAdminOpCode() const noexcept
    {
        return buffer->opCode;
    }
    constexpr bool getContainsLength() const noexcept
    {
        return buffer->cmdFlags.containsLength;
    }
    constexpr bool getContainsOffset() const noexcept
    {
        return buffer->cmdFlags.containsOffset;
    }
    constexpr uint16_t getControllerId() const noexcept
    {
        return buffer->controllerId;
    }
    constexpr uint32_t getOffset() const noexcept
    {
        return buffer->offset;
    }
    constexpr uint32_t getLength() const noexcept
    {
        return buffer->length;
    }

  private:
    const AdminCommandHeader* buffer = nullptr;
};

template <>
class AdminCommand<uint8_t*> : public AdminCommand<const uint8_t*>,
                               public NVMeMessage<uint8_t*>
{
  public:
    AdminCommand(uint8_t* data, size_t len) :
        NVMeMessage<const uint8_t*>(data, len),
        NVMeMessage<uint8_t*>(data, len, NVMeMessageTye::adminCommand,
                              CommandSlot::slot0, true),
        AdminCommand<const uint8_t*>(data, len)
    {
        buffer =
            reinterpret_cast<AdminCommandHeader*>(data + sizeof(CommonHeader));
    }
    template <typename T>
    AdminCommand(T&& arr) : AdminCommand(arr.data(), arr.size())
    {
    }
    AdminCommand(uint8_t* data, size_t len, AdminOpCode opCode) :
        AdminCommand<uint8_t*>(data, len)
    {
        setAdminOpCode(opCode);
    }
    template <typename T>
    AdminCommand(T&& arr, AdminOpCode opCode) :
        AdminCommand(arr.data(), arr.size(), opCode)
    {
    }
    AdminCommandHeader* operator->()
    {
        return buffer;
    }
    void setAdminOpCode(AdminOpCode opCode) noexcept
    {
        buffer->opCode = opCode;
        setCRC();
    }
    inline uint8_t* getSQDword1() noexcept
    {
        return reinterpret_cast<uint8_t*>(&buffer->sqdword1);
    }
    inline uint8_t* getSQDword2() noexcept
    {
        return reinterpret_cast<uint8_t*>(&buffer->sqdword2);
    }
    inline uint8_t* getSQDword3() noexcept
    {
        return reinterpret_cast<uint8_t*>(&buffer->sqdword3);
    }
    inline uint8_t* getSQDword4() noexcept
    {
        return reinterpret_cast<uint8_t*>(&buffer->sqdword4);
    }
    inline uint8_t* getSQDword5() noexcept
    {
        return reinterpret_cast<uint8_t*>(&buffer->sqdword5);
    }
    inline uint8_t* getSQDword10() noexcept
    {
        return reinterpret_cast<uint8_t*>(&buffer->sqdword10);
    }
    inline uint8_t* getSQDword11() noexcept
    {
        return reinterpret_cast<uint8_t*>(&buffer->sqdword11);
    }
    inline uint8_t* getSQDword12() noexcept
    {
        return reinterpret_cast<uint8_t*>(&buffer->sqdword12);
    }
    inline uint8_t* getSQDword13() noexcept
    {
        return reinterpret_cast<uint8_t*>(&buffer->sqdword13);
    }
    inline uint8_t* getSQDword14() noexcept
    {
        return reinterpret_cast<uint8_t*>(&buffer->sqdword14);
    }
    inline uint8_t* getSQDword15() noexcept
    {
        return reinterpret_cast<uint8_t*>(&buffer->sqdword15);
    }
    inline void setLength(uint32_t length) noexcept
    {
        this->buffer->length = htole32(length);
    }
    inline void setOffset(uint32_t offset) noexcept
    {
        this->buffer->offset = htole32(offset);
    }
    inline void setControllerId(uint16_t controllerId) noexcept
    {
        this->buffer->controllerId = htole32(controllerId);
    }
    inline void setContainsOffset(bool value) noexcept
    {
        this->buffer->cmdFlags.containsOffset = value;
    }
    inline void setContainsLength(bool value) noexcept
    {
        this->buffer->cmdFlags.containsLength = value;
    }

  private:
    AdminCommandHeader* buffer;
};

AdminCommand(const uint8_t*, size_t)->AdminCommand<const uint8_t*>;
AdminCommand(uint8_t*, size_t)->AdminCommand<uint8_t*>;
template <typename T>
AdminCommand(const T&) -> AdminCommand<const uint8_t*>;
template <typename T>
AdminCommand(T&) -> AdminCommand<uint8_t*>;
template <typename T>
AdminCommand(T&, AdminOpCode) -> AdminCommand<uint8_t*>;

} // namespace nvmemi::protocol