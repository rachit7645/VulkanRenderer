#version 460

// Includes
#include "GammaCorrect.glsl"

// Fragment inputs
layout(location = 0) in vec2 fragTexCoords;

// Color image
layout(binding = 0, set = 0) uniform sampler2D colorOutput;

// Fragment outputs
layout(location = 0) out vec4 outColor;

void main()
{
    // Set
    outColor = vec4(GammaCorrect(texture(colorOutput, fragTexCoords).rgb), 1.0f);
}