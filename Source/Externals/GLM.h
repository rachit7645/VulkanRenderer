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

#ifndef RACHIT_GLM_H
#define RACHIT_GLM_H

// Use compiler intrinsics
#define GLM_FORCE_INTRINSICS
// Vulkan uses this format
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
// Silence
#define GLM_FORCE_SILENT_WARNINGS

// Standard vector types
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

// Standard matrix types
#include <glm/mat4x4.hpp>

// Integer vector types
#include <glm/ext/vector_int2.hpp>
#include <glm/ext/vector_int4.hpp>

// Unsigned integer vector types
#include <glm/ext/vector_uint2.hpp>
#include <glm/ext/vector_uint3.hpp>
#include <glm/ext/vector_uint4.hpp>

// Core operations
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>
#include <glm/vector_relational.hpp>
#include <glm/gtc/type_ptr.hpp>

// Matrix transformations
#include <glm/gtc/matrix_transform.hpp>

// Fastgltf math types
#include <fastgltf/types.hpp>

namespace glm
{
    // fastgtlf to GLM conversions
    vec3 fastgltf_cast(const fastgltf::math::nvec3& vector);
    vec4 fastgltf_cast(const fastgltf::math::nvec4& vector);
    mat4 fastgltf_cast(const fastgltf::math::fmat4x4& matrix);
    quat fastgltf_cast(const fastgltf::math::fquat& quat);
}

#endif