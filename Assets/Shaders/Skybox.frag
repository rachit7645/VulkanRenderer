#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference     : enable
#extension GL_EXT_scalar_block_layout  : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "Constants/Skybox.glsl"
#include "MegaSet.glsl"

layout(location = 0) in vec3 txCoords;

layout (location = 0) out vec3 outColor;

void main()
{
    outColor = texture(samplerCube(cubemaps[Constants.CubemapIndex], samplers[Constants.SamplerIndex]), txCoords).rgb;
}