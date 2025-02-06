#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference     : enable
#extension GL_EXT_scalar_block_layout  : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "Constants/Converter.glsl"
#include "MegaSet.glsl"
#include "Converter.glsl"

layout(location = 0) in vec3 worldPos;

layout(location = 0) out vec3 outColor;

void main()
{
    vec3 normal = normalize(worldPos);
    vec2 uv     = GetSphericalMapUV(normal);

    outColor = texture(sampler2D(textures[Constants.TextureIndex], samplers[Constants.SamplerIndex]), uv).rgb;
}