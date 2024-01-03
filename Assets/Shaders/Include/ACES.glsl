// ACES Tonemap Operator

#ifndef ACES_GLSL
#define ACES_GLSL

// ACES tone map operator
vec3 ACESToneMap(vec3 color)
{
    // Return
    return clamp((color * (2.51f * color + 0.03f)) / (color * (2.43f * color + 0.59f) + 0.14f), 0.0f, 1.0f);
}

#endif