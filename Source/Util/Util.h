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

#ifndef UTIL_H
#define UTIL_H

#include <cstdint>

// Unused macro
#define UNUSED [[maybe_unused]]

// Vulkan data
#define VULKAN_GLSL_DATA alignas(16)

// Signed integer types
using s8  = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

// Unsigned integer types
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// Floating point types
using f32 = float;
using f64 = double;

// Size types
using ssize = ssize_t;
using usize = std::size_t;

// Enum combiner
template<typename T>
constexpr T operator|(T lhs, T rhs) requires (std::is_enum_v<T>)
{
    // Combine
    return static_cast<T>(
        static_cast<std::underlying_type_t<T>>(lhs) |
        static_cast<std::underlying_type_t<T>>(rhs)
    );
}

// Enum checker
template<typename T>
constexpr T operator&(T lhs, T rhs) requires (std::is_enum_v<T>)
{
    // Combine
    return static_cast<T>(
        static_cast<std::underlying_type_t<T>>(lhs) &
        static_cast<std::underlying_type_t<T>>(rhs)
    );
}

#endif