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

#include "Maths.h"

namespace Maths
{
    glm::mat4 CreateTransformMatrix(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
    {
        auto matrix = glm::identity<glm::mat4>();

        matrix = glm::translate(matrix, translation);

        matrix = glm::rotate(matrix, rotation.x, glm::vec3(1, 0, 0));
        matrix = glm::rotate(matrix, rotation.y, glm::vec3(0, 1, 0));
        matrix = glm::rotate(matrix, rotation.z, glm::vec3(0, 0, 1));

        matrix = glm::scale(matrix, scale);

        return matrix;
    }

    glm::mat4 CreateProjectionReverseZ(f32 FOV, f32 aspectRatio, f32 nearPlane, f32 farPlane)
    {
        // Since we force a depth range of [0, 1], we don't need to convert to it
        auto projection = glm::perspectiveRH_ZO(FOV, aspectRatio, farPlane, nearPlane);

        // Flip Y axis for vulkan
        projection[1][1] *= -1;

        return projection;
    }

    glm::mat4 CreateInfiniteProjectionReverseZ(f32 FOV, f32 aspectRatio, f32 nearPlane)
    {
        const auto tanHalfFovY = std::tan(FOV / 2.0f);

        auto projection = glm::zero<glm::mat4>();

        projection[0][0] =  1.0f / (aspectRatio * tanHalfFovY);
        projection[1][1] = -1.0f / tanHalfFovY; // Flip Y axis for vulkan
        projection[2][2] =  0.0f;
        projection[2][3] = -1.0f;
        projection[3][2] =  nearPlane;

        return projection;
    }

    glm::mat3 CreateNormalMatrix(const glm::mat4& transform)
    {
        // https://www.shadertoy.com/view/3s33zj
        return
        {
            glm::cross(glm::vec3(transform[1]), glm::vec3(transform[2])),
            glm::cross(glm::vec3(transform[2]), glm::vec3(transform[0])),
            glm::cross(glm::vec3(transform[0]), glm::vec3(transform[1]))
        };
    }
}