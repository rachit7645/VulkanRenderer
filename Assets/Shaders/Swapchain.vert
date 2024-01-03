#version 460

// Vertex inputs
layout(location = 0) in vec2 position;

// Vertex outputs
layout(location = 0) out vec2 fragTexCoords;

void main()
{
    // Set position
    gl_Position = vec4(position.xy, 0.0f, 1.0f);
    // Set texture coords
    fragTexCoords = 0.5f * (position + vec2(1.0));
}