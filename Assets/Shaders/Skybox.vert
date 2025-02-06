#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference     : enable
#extension GL_EXT_scalar_block_layout  : enable

#include "Constants/Skybox.glsl"

layout(location = 0) out vec3 txCoords;

void main()
{
    // Disable translation
    mat4 rotView = mat4(mat3(Constants.Scene.view));

    txCoords    = Constants.Vertices.positions[gl_VertexIndex];
    gl_Position = Constants.Scene.projection * rotView * vec4(txCoords, 1.0f);
}