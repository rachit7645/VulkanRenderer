#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2    : enable
#extension GL_EXT_scalar_block_layout  : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "Constants/Convolution.glsl"
#include "Constants.glsl"
#include "MegaSet.glsl"

layout(location = 0) in vec3 worldPos;

layout(location = 0) out vec3 outColor;

void main()
{
    vec3 normal = normalize(worldPos);

    vec3 up    = vec3(0.0f, 1.0f, 0.0f);
    vec3 right = normalize(cross(up, normal));
    up         = normalize(cross(normal, right));

    vec3  irradiance = vec3(0.0f);
    float nrSamples  = 0.0f;

    for(float phi = 0.0; phi < 2.0f * PI; phi += CONVOLUTION_SAMPLE_DELTA)
    {
        float sinPhi = sin(phi);
        float cosPhi = cos(phi);

        for(float theta = 0.0f; theta < 0.5f * PI; theta += CONVOLUTION_SAMPLE_DELTA)
        {
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);

            vec3 tangentSample = vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
            vec3 sampleVec     = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
            irradiance        += texture(samplerCube(cubemaps[Constants.EnvMapIndex], samplers[Constants.SamplerIndex]), sampleVec).rgb * cosTheta * sinTheta;
            nrSamples         += 1;
        }
    }

    outColor = PI * irradiance * (1.0f / float(nrSamples));
}