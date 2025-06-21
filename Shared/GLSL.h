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

#ifndef GLSL_H
#define GLSL_H

#define GLSL_CONCAT3_INNER(a, b, c) a##b##c
#define GLSL_CONCAT3(a, b, c) GLSL_CONCAT3_INNER(a, b, c)

#ifdef __cplusplus

#include <vulkan/vulkan.h>

#include "Externals/GLM.h"
#include "Util/Types.h"

using GLSL_VEC2 = glm::vec2;
using GLSL_VEC3 = glm::vec3;
using GLSL_VEC4 = glm::vec4;

using GLSL_UVEC2 = glm::uvec2;

using GLSL_MAT3 = glm::mat3;
using GLSL_MAT4 = glm::mat4;

#define GLSL_SHADER_STORAGE_BUFFER(BufferName, ReadOnlyOrWriteOnly) struct BufferName

#define GLSL_BUFFER_POINTER(BufferType) VkDeviceAddress

#define GLSL_NAMESPACE_BEGIN(NamespaceName) \
    namespace NamespaceName \
    { \

#define GLSL_NAMESPACE_END \
    }

#define GLSL_PUSH_CONSTANT_BEGIN struct Constants
#define GLSL_PUSH_CONSTANT_END

#define GLSL_ENUM_CLASS_BEGIN(EnumClassName, UnderlyingType) \
    enum class EnumClassName : UnderlyingType \
    { \

#define GLSL_ENUM_CLASS_ENTRY(EnumClassName, UnderlyingType, EnumClassEntryName, EnumClassEntryValue) \
    EnumClassEntryName = EnumClassEntryValue,

#define GLSL_ENUM_CLASS_END };

#define GLSL_ENUM_CLASS_NAME(EnumClassName, UnderlyingType) EnumClassName

#define GLSL_CONSTANT(ConstantType, ConstantName, ConstantValue) \
    constexpr ConstantType ConstantName = ConstantValue;

#else

#extension GL_EXT_shader_explicit_arithmetic_types_int32   : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64   : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float32 : enable

#define u32 uint32_t
#define u64 uint64_t
#define f32 float32_t

#define GLSL_VEC2 vec2
#define GLSL_VEC3 vec3
#define GLSL_VEC4 vec4

#define GLSL_UVEC2 uvec2

#define GLSL_MAT3 mat3
#define GLSL_MAT4 mat4

#define GLSL_SHADER_STORAGE_BUFFER(BufferName, ReadOnlyOrWriteOnly) \
    layout(buffer_reference, scalar) ReadOnlyOrWriteOnly buffer BufferName

#define GLSL_BUFFER_POINTER(BufferType) BufferType

#define GLSL_NAMESPACE_BEGIN(NamespaceName)
#define GLSL_NAMESPACE_END

#define GLSL_PUSH_CONSTANT_BEGIN layout(push_constant, scalar) uniform ConstantsBuffer
#define GLSL_PUSH_CONSTANT_END Constants

#define GLSL_ENUM_CLASS_BEGIN(EnumClassName, UnderlyingType)

#define GLSL_ENUM_CLASS_ENTRY(EnumClassName, UnderlyingType, EnumClassEntryName, EnumClassEntryValue) \
    const UnderlyingType GLSL_CONCAT3(EnumClassName, _, EnumClassEntryName) = UnderlyingType(EnumClassEntryValue);

#define GLSL_ENUM_CLASS_END

#define GLSL_ENUM_CLASS_NAME(EnumClassName, UnderlyingType) UnderlyingType

#define GLSL_CONSTANT(ConstantType, ConstantName, ConstantValue) \
    const ConstantType ConstantName = ConstantValue;

#endif

#endif