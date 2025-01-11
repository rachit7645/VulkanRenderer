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

#ifndef MATHS_H
#define MATHS_H

#include "Util.h"
#include "Externals/GLM.h"

namespace Maths
{
    /// @brief Creates a transformation matrix
    /// @param translation Translation vector
    /// @param rotation Euler Rotation vector (in radians)
    /// @param scale Scaling vector
    /// @returns Transformation matrix
    glm::mat4 CreateTransformMatrix(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale);

    /// @brief Create a projection matrix for reverse-z
    /// @param FOV Field of View (in radians)
    /// @param aspectRatio Aspect ratio
    /// @param nearPlane Near Plane of View Frustum
    /// @param farPlane Far Plane of View Frustum
    /// @returns Projection matrix for reverse-z
    glm::mat4 CreateProjectionReverseZ(f32 FOV, f32 aspectRatio, f32 nearPlane, f32 farPlane);
}

#endif