#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2    : enable
#extension GL_EXT_scalar_block_layout  : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "Constants/PreFilter.glsl"
#include "Constants.glsl"
#include "MegaSet.glsl"
#include "Sampling.glsl" 
#include "PBR.glsl"

layout(location = 0) in vec3 worldPos;

layout(location = 0) out vec3 outColor;

void main()
{
    vec3 N = normalize(worldPos);

    // Assume that the reflection, view and normal vector are the same
    vec3 R = N;
    vec3 V = R;

    // Tangent to World Space Sample Vector
    vec3 up        = abs(N.z) < 0.999f ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec2 resolution = textureSize(samplerCube(cubemaps[Constants.EnvMapIndex], samplers[Constants.SamplerIndex]), 0);

    vec3  prefilteredColor = vec3(0.0f);
    float totalWeight      = 0.0f;

    for(uint i = 0u; i < PREFILER_SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, PREFILER_SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, tangent, bitangent, Constants.Roughness);
        vec3 L  = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0f);

        // Avoids NaN
        if(NdotL > 0.0f)
        {
            float NdotH = max(dot(N, H), 0.0f);
            float HdotV = max(dot(H, V), 0.0f);
            float a     = Constants.Roughness * Constants.Roughness;

            float D   = DistributionGGX(NdotH, a);
            float pdf = D * NdotH / (4.0f * HdotV) + 0.0001f;

            // Fix for extremely bright spots
            float saSample = 1.0f / (float(PREFILER_SAMPLE_COUNT) * pdf + 0.0001f);
            float saTexel  = 4.0f * PI / (6.0f * resolution.x * resolution.y);
            float mipLevel = Constants.Roughness == 0.0f ? 0.0f : 0.5f * log2(saSample / saTexel);

            prefilteredColor += textureLod(samplerCube(cubemaps[Constants.EnvMapIndex], samplers[Constants.SamplerIndex]), L, mipLevel).rgb * NdotL;
            totalWeight      += NdotL;
        }
    }

    outColor = prefilteredColor / totalWeight;
}