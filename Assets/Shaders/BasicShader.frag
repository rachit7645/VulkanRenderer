#version 460

// Fragment inputs
layout (location = 0) in vec3 fragColor;

// Fragment outputs
layout (location = 0) out vec4 outColor;

// Fragment entry point
void main()
{
    // Output color
    outColor = vec4(fragColor, 1.0f);
}