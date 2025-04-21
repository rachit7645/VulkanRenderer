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

#include "GLM.h"

#include "Util/Util.h"

namespace glm
{
    vec3 fastgltf_cast(const fastgltf::math::nvec3& vector)
    {
        vec3 ret = {};
        std::memcpy(value_ptr(ret), vector.data(), vector.size_bytes());
        return ret;
    }

    vec4 fastgltf_cast(const fastgltf::math::nvec4& vector)
    {
        vec4 ret = {};
        std::memcpy(value_ptr(ret), vector.data(), vector.size_bytes());
        return ret;
    }

    mat4 fastgltf_cast(const fastgltf::math::fmat4x4& matrix)
    {
        mat4 ret = {};
        std::memcpy(value_ptr(ret), matrix.data(), matrix.size_bytes());
        return ret;
    }

    quat fastgltf_cast(const fastgltf::math::fquat& quat)
    {
        // glm uses wxyz layout by default
        return {quat.w(), quat.x(), quat.y(), quat.z()};
    }

    VkTransformMatrixKHR vk_cast(const mat4& matrix)
    {
        VkTransformMatrixKHR vkTransform = {};

        for (usize row = 0; row < 3; ++row)
        {
            for (usize col = 0; col < 4; ++col)
            {
                vkTransform.matrix[row][col] = matrix[col][row];
            }
        }

        return vkTransform;
    }
}