/*
 * Copyright (c) 2023 - 2025 Rachit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef POINT_SHADOW_MAP_GLSL
#define POINT_SHADOW_MAP_GLSL

struct PointShadowData
{
    mat4 matrices[6];
    vec2 shadowPlanes;
};

layout(buffer_reference, scalar) readonly buffer PointShadowBuffer
{
    PointShadowData pointShadowData[];
};

float CalculatePointShadow
(
    uint lightIndex,
    vec3 fragPosition,
    vec3 lightPosition,
    vec3 cameraPosition,
    PointShadowData pointShadowData,
    textureCubeArray pointShadowMap,
    sampler pointShadowSampler
)
{
    const vec3 GRID_SAMPLING_DISK[POINT_SHADOW_NUM_SAMPLES] = vec3[]
    (
        vec3(1.0f, 1.0f,  1.0f), vec3( 1.0f, -1.0f,  1.0f), vec3(-1.0f, -1.0f,  1.0f), vec3(-1.0f, 1.0f,  1.0f),
        vec3(1.0f, 1.0f, -1.0f), vec3( 1.0f, -1.0f, -1.0f), vec3(-1.0f, -1.0f, -1.0f), vec3(-1.0f, 1.0f, -1.0f),
        vec3(1.0f, 1.0f,  0.0f), vec3( 1.0f, -1.0f,  0.0f), vec3(-1.0f, -1.0f,  0.0f), vec3(-1.0f, 1.0f,  0.0f),
        vec3(1.0f, 0.0f,  1.0f), vec3(-1.0f,  0.0f,  1.0f), vec3( 1.0f,  0.0f, -1.0f), vec3(-1.0f, 0.0f, -1.0f),
        vec3(0.0f, 1.0f,  1.0f), vec3( 0.0f, -1.0f,  1.0f), vec3( 0.0f, -1.0f, -1.0f), vec3( 0.0f, 1.0f, -1.0f)
    );

    vec3  fragToLight  = fragPosition - lightPosition;
    float currentDepth = length(fragToLight);

    float shadow       = 0.0f;
    float viewDistance = length(cameraPosition - fragPosition);
    float diskRadius   = (1.0f + (viewDistance / pointShadowData.shadowPlanes.y)) / pointShadowData.shadowPlanes.y;

    for(int i = 0; i < POINT_SHADOW_NUM_SAMPLES; ++i)
    {
        float closestDepth = texture(samplerCubeArray(pointShadowMap, pointShadowSampler), vec4(fragToLight + GRID_SAMPLING_DISK[i] * diskRadius, lightIndex)).r;

        // [0, 1] -> [0, FarPlane]
        closestDepth *= pointShadowData.shadowPlanes.y;

        if ((currentDepth - POINT_SHADOW_BIAS) > closestDepth)
        {
            shadow += 1.0f;
        }
    }

    shadow /= float(POINT_SHADOW_NUM_SAMPLES);

    return shadow;
}

#endif