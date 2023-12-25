#ifndef SPOT_LIGHT_H
#define SPOT_LIGHT_H

#include "Externals/GLM.h"
#include "Util/Util.h"

namespace Renderer
{
    struct VULKAN_GLSL_DATA SpotLight
    {
        // Data
        glm::vec4 position    = {0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 color       = {0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 intensity   = {0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 attenuation = {0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 direction   = {0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 cutOff      = {0.0f, 0.0f, 1.0f, 1.0f};
    };
}

#endif