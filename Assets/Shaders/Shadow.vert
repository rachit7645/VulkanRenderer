#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2    : enable
#extension GL_EXT_scalar_block_layout  : enable
#extension GL_EXT_multiview            : enable

#include "Constants/Shadow.glsl"

void main()
{
    Mesh mesh       = Constants.Meshes.meshes[gl_DrawID];
    vec3 position   = Constants.Positions.positions[gl_VertexIndex];
    Cascade cascade = Constants.Cascades.cascades[Constants.Offset + gl_ViewIndex];

    gl_Position = cascade.matrix * mesh.transform * vec4(position, 1.0f);
}