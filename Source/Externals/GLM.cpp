#include "GLM.h"

namespace glm
{
    vec2 ai_cast(const aiVector2D& vector)
    {
        // Convert
        return {vector.x, vector.y};
    }

    glm::vec3 ai_cast(const aiVector3D& vector)
    {
        // Convert
        return {vector.x, vector.y, vector.z};
    }
}