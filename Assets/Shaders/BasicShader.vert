#version 460

// Position data
const vec2 POSITIONS[3] = vec2[]
(
    vec2( 0.0f, -0.5f),
    vec2( 0.5f,  0.5f),
    vec2(-0.5f,  0.5f)
);

// Vertex color data
const vec3 COLORS[3] = vec3[]
(
    vec3(1.0f, 0.0f, 0.0f),
    vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 0.0f, 1.0f)
);

// Vertex outputs
layout (location = 0) out vec3 fragColor;

// Vertex entry point
void main()
{
    // Get position
    gl_Position = vec4(POSITIONS[gl_VertexIndex], 0.0f, 1.0f);
    // Get vertex color
    fragColor = COLORS[gl_VertexIndex];
}