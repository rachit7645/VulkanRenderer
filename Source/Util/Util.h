#ifndef UTIL_H
#define UTIL_H

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cmath>

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

namespace Util
{
    // Global delta
    inline f32 g_Delta = 1.0f;
}

#endif