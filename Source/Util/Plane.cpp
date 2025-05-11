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

#include "Plane.h"

#include <Util/Log.h>

namespace Maths
{
    Frustum::Frustum(const glm::mat4& projectionView)
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
        for (auto& plane : planes)
        {
            const f32 length = glm::length(plane.normal);

            plane.normal   /= length;
            plane.distance /= length;
        }
    }
}
