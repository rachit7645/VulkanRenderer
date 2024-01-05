/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef MATHS_H
#define MATHS_H

#include "Externals/GLM.h"

namespace Maths
{
    /// @brief Creates a transformation matrix
    /// @param translation Translation vector
    /// @param rotation Euler rotation vector (In radians)
    /// @param scale Scaling vector
    /// @returns Transformation matrix
    glm::mat4 CreateTransformationMatrix(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale);
}

#endif