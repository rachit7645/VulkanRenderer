#ifndef BASIC_SHADER_PUSH_CONSTANTS_H
#define BASIC_SHADER_PUSH_CONSTANTS_H

#include "Externals/GLM.h"
#include "Util/Util.h"

namespace Renderer
{
    struct VULKAN_GLSL_DATA BasicShaderPushConstant
    {
        // Transformation matrix
        glm::mat4 modelMatrix = {};
    };
}

#endif
