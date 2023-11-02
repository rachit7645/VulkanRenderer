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
        T matrix = {};
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
