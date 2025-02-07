#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference     : enable
#extension GL_EXT_scalar_block_layout  : enable
#extension GL_EXT_multiview            : enable

#include "Constants/Prefilter.glsl"

layout(location = 0) out vec3 worldPos;

void main()
{
    worldPos    = Constants.Vertices.positions[gl_VertexIndex];
    gl_Position = Constants.Matrices.matrices[gl_ViewIndex] * vec4(worldPos, 1.0f);

    // Trick to guarantee early depth test
    gl_Position = gl_Position.xyww;
}