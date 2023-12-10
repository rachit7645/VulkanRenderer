// Gamma correction utils

#include "Constants.glsl"

vec3 GammaCorrect(vec3 color)
{
    // Return
    return pow(color, vec3(GAMMA_FACTOR));
}

vec3 ToLinear(vec3 color)
{
    // Return
    return pow(color, vec3(INV_GAMMA_FACTOR));
}