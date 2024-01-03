// PBR functions

#ifndef PBR_GLSL
#define PBR_GLSL

#include "Lights.glsl"

// Trowbridge-Reitz Normal Distribution Function
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    // Square roughness as proposed by Disney
    float a = roughness * roughness;

    // Calculate squares
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    // Calculate numerator and denominator
    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom       = PI * denom * denom;

    // Divide and return
    return nom / denom;
}

// Schlick Geometry Function
float GeometrySchlickGGX(float NdotV, float roughness)
{
    // Calculate various parameters
    float r = (roughness + 1.0f);
    // Square roughness as proposed by Disney
    float k = (r * r) / 8.0f;

    // Calculate numerator and denominator
    float nom   = NdotV;
    float denom = NdotV * (1.0f - k) + k;

    // Divide and return
    return nom / denom;
}

// Smith's method to combine Geometry functions
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    // Take dot products
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);

    // Calculate both geometry schlick outputs
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    // Combine and return
    return ggx1 * ggx2;
}

// Fresnel equation function
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    // Clamp to avoid artifacts
    return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

// Fresnel equation function with injected roughness paramenter for IBL
vec3 FresnelSchlick(float cosTheta, vec3 F0, float roughness)
{
    // Clamp to avoid artifacts
    return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

vec3 CalculateLight(LightInfo lightInfo, vec3 N, vec3 V, vec3 F0, vec3 albedo, float roughness, float metallic)
{
    // Calculate half-way vector
    vec3 H = normalize(V + lightInfo.L);
    // Calculate dots
    float NdotL = max(dot(N, lightInfo.L), 0.0f);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, lightInfo.L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0f), F0);

    // Combine specular
    vec3  numerator   = NDF * G * F;
    float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, lightInfo.L), 0.0f);
    // To prevent division by zero, divide by epsilon
    denominator   = max(denominator, 0.0001f);
    vec3 specular = numerator / denominator;

    // Diffuse energy conservation
    vec3 kS = F;
    vec3 kD = vec3(1.0f) - kS;
    kD     *= 1.0f - metallic;

    // Add everthing up to Lo
    return (kD * albedo / PI + specular) * lightInfo.radiance * NdotL;
}

#endif