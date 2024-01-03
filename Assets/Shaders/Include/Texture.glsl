// Texture utility functions

#ifndef TEXTURE_GLSL
#define TEXTURE_GLSL

#include "GammaCorrect.glsl"

// Sample texture
vec3 Sample(texture2D tex, sampler texSampler, vec2 txCoords)
{
    // Return
    return texture(sampler2D(tex, texSampler), txCoords).rgb;
}

// Sample SRGB -> Linear
vec3 SampleLinear(texture2D tex, sampler texSampler, vec2 txCoords)
{
    // Return
    return ToLinear(Sample(tex, texSampler, txCoords));
}

#endif