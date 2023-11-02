/*
 * Copyright 2023 Rachit Khandelwal
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
    // Create transformation matrix
    template<typename T, typename U>
    constexpr T CreateModelMatrix(U translation, U rotation, U scale)
    {
        // Matrix
        T matrix = glm::identity<T>();
        // Translate
        matrix = glm::translate(matrix, translation);
        // Rotate in all axis
        matrix = glm::rotate(matrix, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        matrix = glm::rotate(matrix, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        matrix = glm::rotate(matrix, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        // Scale
        matrix = glm::scale(matrix, scale);
        // Return
        return matrix;
    }

    // Common uses
    template glm::mat4 CreateModelMatrix<glm::mat4, glm::vec3>(glm::vec3, glm::vec3, glm::vec3);
}

#endif
