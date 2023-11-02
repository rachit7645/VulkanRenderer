#version 460

// Push constant buffer
layout(push_constant) uniform MeshConstantsBuffer
{
    // Model matrix
    mat4 modelMatrix;
} MeshConstants;

// Vertex inputs
layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;

// Vertex outputs
layout(location = 0) out vec3 fragColor;

// Vertex entry point
void main()
{
    // Get position
    gl_Position = MeshConstants.modelMatrix * vec4(position, 0.0f, 1.0f);
    // Get vertex color
    fragColor = color;
}