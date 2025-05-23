/*
 * Copyright (c) 2023 - 2025 Rachit
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

#ifndef ENUM_H
#define ENUM_H

#include <type_traits>

// Enum class combiner
template<typename T>
constexpr T operator|(T lhs, T rhs) requires (std::is_enum_v<T>)
{
    return static_cast<T>(
        static_cast<std::underlying_type_t<T>>(lhs) |
        static_cast<std::underlying_type_t<T>>(rhs)
    );
}

// Enum class checker
template<typename T>
constexpr T operator&(T lhs, T rhs) requires (std::is_enum_v<T>)
{
    return static_cast<T>(
        static_cast<std::underlying_type_t<T>>(lhs) &
        static_cast<std::underlying_type_t<T>>(rhs)
    );
}

template<typename T>
constexpr T& operator|=(T& lhs, T rhs) requires (std::is_enum_v<T>)
{
    lhs = lhs | rhs;
    return lhs;
}

template<typename T>
constexpr T& operator&=(T& lhs, T rhs) requires (std::is_enum_v<T>)
{
    lhs = lhs & rhs;
    return lhs;
}

#endif
