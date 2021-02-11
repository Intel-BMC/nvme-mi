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

#include "binding_context.hpp"

#include <phosphor-logging/log.hpp>

using nvmemi::BindingContext;

BindingContext::BindingContext(
    std::shared_ptr<sdbusplus::asio::connection> connection,
    const mctpw::BindingType binding) :
    busConnection(connection),
    bindingType(binding)
{
    mctpw::MCTPConfiguration config(mctpw::MessageType::nvmeMgmtMsg, binding);
    // TODO Add wrapper callbacks
    mctpWrapper = std::make_shared<mctpw::MCTPWrapper>(busConnection, config);
}

void BindingContext::initialize(boost::asio::yield_context yield)
{
    phosphor::logging::log<phosphor::logging::level::DEBUG>(
        ("Initializing context " +
         std::to_string(static_cast<int>(bindingType)))
            .c_str());
    mctpWrapper->detectMctpEndpoints(yield);
    const auto& endPoints = mctpWrapper->getEndpointMap();
    for (const auto& endPoint : endPoints)
    {
        phosphor::logging::log<phosphor::logging::level::DEBUG>(
            ("EID found " + std::to_string(endPoint.first)).c_str());
    }
    // TODO Create NVMe Device object for each eid
}
