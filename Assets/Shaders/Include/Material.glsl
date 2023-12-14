// Material utils

#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

#include "Texture.glsl"

// Defines
#define ALBEDO(material)     material[0]
#define NORMAL(material)     material[1]
#define AO_RGH_MTL(material) material[2]

// Get normal from normal map
vec3 GetNormal(mat3 TBN, texture2D normalMap, sampler texSampler, vec2 txCoords)
{
    // Sample
    vec3 normal = Sample(normalMap, texSampler, txCoords);
    // Convert to correct range
    normal = normal * 2.0f - 1.0f;
    // Return normalised
    return normalize(TBN * normal);
}

#endif