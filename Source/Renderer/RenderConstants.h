#ifndef RENDER_CONSTANTS_H
#define RENDER_CONSTANTS_H

#include "Externals/GLM.h"

namespace Renderer
{
    // Clear color
    constexpr glm::vec4 CLEAR_COLOR = {0.53f, 0.81f, 0.92f, 1.0f};
    // Aspect ratio
    constexpr f32 ASPECT_RATIO = 16.0f / 9.0f;
}

#endif
