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

#ifdef __cplusplus

#include <vulkan/vulkan.h>

#include "Externals/GLM.h"
#include "Util/Types.h"

// TODO: HACKY! (And pollutes main namespace)

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

using uvec2 = glm::uvec2;

using mat3 = glm::mat3;
using mat4 = glm::mat4;

#define GLSL_SHADER_STORAGE_BUFFER(BufferName, ReadOnlyOrWriteOnly) struct BufferName

#define GLSL_BUFFER_POINTER(BufferType) VkDeviceAddress

#define GLSL_NAMESPACE_BEGIN(NamespaceName) \
    namespace NamespaceName \
    { \

#define GLSL_NAMESPACE_END \
    }

#define GLSL_PUSH_CONSTANT_BEGIN struct Constants
#define GLSL_PUSH_CONSTANT_END

#else

#extension GL_EXT_shader_explicit_arithmetic_types_int32   : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64   : enable
#extension GL_EXT_shader_explicit_arithmetic_types_float32 : enable

#define u32 uint32_t
#define u64 uint64_t
#define f32 float32_t

#define GLSL_SHADER_STORAGE_BUFFER(BufferName, ReadOnlyOrWriteOnly) layout(buffer_reference, scalar) ReadOnlyOrWriteOnly buffer BufferName

#define GLSL_BUFFER_POINTER(BufferType) BufferType

#define GLSL_NAMESPACE_BEGIN(NamespaceName)
#define GLSL_NAMESPACE_END

#define GLSL_PUSH_CONSTANT_BEGIN layout(push_constant, scalar) uniform ConstantsBuffer
#define GLSL_PUSH_CONSTANT_END Constants

#endif

#endif