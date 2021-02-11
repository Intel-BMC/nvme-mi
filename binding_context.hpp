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

#include <mctp_wrapper.hpp>

namespace nvmemi
{
/**
 * @brief Class to manage all NVMe devices coming under an MCTP binding type
 *
 */
class BindingContext
{
  public:
    BindingContext(std::shared_ptr<sdbusplus::asio::connection> connection,
                   const mctpw::BindingType binding);
    /**
     * @brief Perform initialization steps. Finds all available NVMe devices and
     * initializes each
     *
     * @param yield yield_context to be used in DBus calls
     */
    void initialize(boost::asio::yield_context yield);

  private:
    std::shared_ptr<sdbusplus::asio::connection> busConnection{};
    const mctpw::BindingType bindingType;
    std::shared_ptr<mctpw::MCTPWrapper> mctpWrapper{};
};
} // namespace nvmemi