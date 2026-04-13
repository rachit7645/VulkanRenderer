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

#ifndef FRUSTUM_GLSL
#define FRUSTUM_GLSL

#include "GLSL.h"
#include "AABB.h"

GLSL_NAMESPACE_BEGIN(GPU)

struct Plane
{
    GLSL_VEC3 normal;
    f32       distance;
};

GLSL_SHADER_STORAGE_BUFFER(FrustumBuffer, readonly)
{
    #ifdef __cplusplus
    explicit FrustumBuffer(const glm::mat4& projectionView)
        : planes{},
          aabb{}
    {
        // Left
        planes[0].normal.x = projectionView[0][3] + projectionView[0][0];
        planes[0].normal.y = projectionView[1][3] + projectionView[1][0];
        planes[0].normal.z = projectionView[2][3] + projectionView[2][0];
        planes[0].distance = projectionView[3][3] + projectionView[3][0];

        // Right
        planes[1].normal.x = projectionView[0][3] - projectionView[0][0];
        planes[1].normal.y = projectionView[1][3] - projectionView[1][0];
        planes[1].normal.z = projectionView[2][3] - projectionView[2][0];
        planes[1].distance = projectionView[3][3] - projectionView[3][0];

        // Bottom
        planes[2].normal.x = projectionView[0][3] + projectionView[0][1];
        planes[2].normal.y = projectionView[1][3] + projectionView[1][1];
        planes[2].normal.z = projectionView[2][3] + projectionView[2][1];
        planes[2].distance = projectionView[3][3] + projectionView[3][1];

        // Top
        planes[3].normal.x = projectionView[0][3] - projectionView[0][1];
        planes[3].normal.y = projectionView[1][3] - projectionView[1][1];
        planes[3].normal.z = projectionView[2][3] - projectionView[2][1];
        planes[3].distance = projectionView[3][3] - projectionView[3][1];

        // Near
        planes[4].normal.x = projectionView[0][3] + projectionView[0][2];
        planes[4].normal.y = projectionView[1][3] + projectionView[1][2];
        planes[4].normal.z = projectionView[2][3] + projectionView[2][2];
        planes[4].distance = projectionView[3][3] + projectionView[3][2];

        // Far
        planes[5].normal.x = projectionView[0][3] - projectionView[0][2];
        planes[5].normal.y = projectionView[1][3] - projectionView[1][2];
        planes[5].normal.z = projectionView[2][3] - projectionView[2][2];
        planes[5].distance = projectionView[3][3] - projectionView[3][2];

        // Normalize all planes
        for (auto& [normal, distance] : planes)
        {
            const f32 length = glm::length(normal);

            normal   /= length;
            distance /= length;
        }

        constexpr std::array<glm::vec4, 8> NDC_CORNERS =
        {
            glm::vec4{ -1.0f, -1.0f, 0.0f, 1.0f },  // Near Bottom-Left
            glm::vec4{  1.0f, -1.0f, 0.0f, 1.0f }, // Near Bottom-Right
            glm::vec4{  1.0f,  1.0f, 0.0f, 1.0f }, // Near Top-Right
            glm::vec4{ -1.0f,  1.0f, 0.0f, 1.0f }, // Near Top-Left
            glm::vec4{ -1.0f, -1.0f, 1.0f, 1.0f }, // Far  Bottom-Left
            glm::vec4{  1.0f, -1.0f, 1.0f, 1.0f }, // Far  Bottom-Right
            glm::vec4{  1.0f,  1.0f, 1.0f, 1.0f }, // Far  Top-Right
            glm::vec4{ -1.0f,  1.0f, 1.0f, 1.0f }, // Far  Top-Left
        };

        const glm::mat4 inverseProjectionView = glm::inverse(projectionView);

        aabb.min = glm::vec3(std::numeric_limits<f32>::max());
        aabb.max = glm::vec3(std::numeric_limits<f32>::lowest());

        for (const auto& NDC : NDC_CORNERS)
        {
            const glm::vec4 projectedPosition = inverseProjectionView * NDC;

            const glm::vec3 corner = glm::vec3(projectedPosition) / projectedPosition.w;

            aabb.min = glm::min(aabb.min, corner);
            aabb.max = glm::max(aabb.max, corner);
        }
    }
    #endif

    Plane planes[6];
    AABB  aabb;
};

GLSL_NAMESPACE_END

#endif