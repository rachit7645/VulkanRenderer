#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout  : enable

#include "MegaSet.glsl"
#include "Constants/UpSample.glsl"

layout(location = 0) in vec2 fragUV;

layout (location = 0) out vec3 upsample;

void main()
{
    #define SRC_TEXTURE sampler2D(textures[Constants.ImageIndex], samplers[Constants.SamplerIndex])
    
    // Radius is in texture coordinate space so that it varies per mip level
    float x = Constants.FilterRadius;
    float y = Constants.FilterRadius;

    // A - B - C
    vec3 a = texture(SRC_TEXTURE, vec2(fragUV.x - x, fragUV.y + y)).rgb;
    vec3 b = texture(SRC_TEXTURE, vec2(fragUV.x,     fragUV.y + y)).rgb;
    vec3 c = texture(SRC_TEXTURE, vec2(fragUV.x + x, fragUV.y + y)).rgb;

    // D - E - F
    vec3 d = texture(SRC_TEXTURE, vec2(fragUV.x - x, fragUV.y)).rgb;
    vec3 e = texture(SRC_TEXTURE, vec2(fragUV.x,     fragUV.y)).rgb;
    vec3 f = texture(SRC_TEXTURE, vec2(fragUV.x + x, fragUV.y)).rgb;

    // G - H - I
    vec3 g = texture(SRC_TEXTURE, vec2(fragUV.x - x, fragUV.y - y)).rgb;
    vec3 h = texture(SRC_TEXTURE, vec2(fragUV.x,     fragUV.y - y)).rgb;
    vec3 i = texture(SRC_TEXTURE, vec2(fragUV.x + x, fragUV.y - y)).rgb;

    // Apply weighted distribution using a 3x3 tent filter
    upsample  = e * 4.0f;
    upsample += (b + d + f + h) * 2.0f;
    upsample += (a + c + g + i);
    upsample *= 1.0f / 16.0f;
}