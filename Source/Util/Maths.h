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
    glm::mat4 CreateTransformMatrix(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale);
    glm::mat4 CreateProjectionReverseZ(f32 FOV, f32 aspectRatio, f32 nearPlane, f32 farPlane);
    glm::mat3 CreateNormalMatrix(const glm::mat4& transform);
}

#endif