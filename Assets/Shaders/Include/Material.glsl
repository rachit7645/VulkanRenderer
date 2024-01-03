// Material utils

#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

// Defines
#define ALBEDO(material)     material[0]
#define NORMAL(material)     material[1]
#define AO_RGH_MTL(material) material[2]

// Get normal from normal map
vec3 GetNormalFromMap(vec3 normal, mat3 TBN)
{
    // Convert to correct range
    normal = normal * 2.0f - 1.0f;
    // Return normalised
    return normalize(TBN * normal);
}

#endif