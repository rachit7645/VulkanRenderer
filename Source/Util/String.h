/*
 * Copyright (c) 2023 - 2026 Rachit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef STRING_H
#define STRING_H

#include <string>
#include <string_view>

namespace Util
{
    struct StringHash
    {
        using is_transparent = void;

        [[nodiscard]] std::size_t operator()(const std::string_view string) const
        {
            return std::hash<std::string_view>{}(string);
        }
    };

    struct StringEqual
    {
        using is_transparent = void;

        [[nodiscard]] bool operator()(const std::string_view lhs, const std::string_view rhs) const
        {
            return lhs == rhs;
        }
    };

    [[nodiscard]] std::string ToLower(const std::string_view string);
}

#endif