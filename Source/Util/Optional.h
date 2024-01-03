/*
 *    Copyright 2023 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <optional>

namespace Util
{
    template <typename T>
    constexpr auto GetAddressOrNull(const std::optional<T>& opt) -> const T*
    {
        // Return
        return opt.has_value() ? &(*opt) : nullptr;
    }
}

#endif
