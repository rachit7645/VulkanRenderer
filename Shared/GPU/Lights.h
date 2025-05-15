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

#ifndef LIGHTS_GLSL
#define LIGHTS_GLSL

#include "GLSL.h"

GLSL_NAMESPACE_BEGIN(GPU)

#ifdef __cplusplus

constexpr u32 MAX_DIR_LIGHT_COUNT = 1;

constexpr u32 MAX_POINT_LIGHT_COUNT          = 16;
constexpr u32 MAX_SHADOWED_POINT_LIGHT_COUNT = 4;

constexpr u32 MAX_SPOT_LIGHT_COUNT           = 16;
constexpr u32 MAX_SHADOWED_SPOT_LIGHT_COUNT  = 4;

constexpr u32 MAX_TOTAL_POINT_LIGHT_COUNT = MAX_SHADOWED_POINT_LIGHT_COUNT + MAX_POINT_LIGHT_COUNT;
constexpr u32 MAX_TOTAL_SPOT_LIGHT_COUNT  = MAX_SHADOWED_SPOT_LIGHT_COUNT  + MAX_SPOT_LIGHT_COUNT;

constexpr u32 MAX_LIGHT_COUNT = MAX_DIR_LIGHT_COUNT + MAX_TOTAL_POINT_LIGHT_COUNT + MAX_TOTAL_SPOT_LIGHT_COUNT;

constexpr glm::uvec2 POINT_SHADOW_DIMENSIONS   = {512,  512};
constexpr glm::vec2  POINT_LIGHT_SHADOW_PLANES = {1.0f, 25.0f};

constexpr glm::uvec2 SPOT_LIGHT_SHADOW_DIMENSIONS = {1024, 1024};
constexpr glm::vec2  SPOT_LIGHT_SHADOW_PLANES     = {0.1f, 100.0f};

#endif

struct DirLight
{
    vec3 position;
    vec3 color;
    vec3 intensity;
};

struct PointLight
{
    vec3 position;
    vec3 color;
    vec3 intensity;
    vec3 attenuation;
};

struct ShadowedPointLight
{
    #ifdef __cplusplus
    ShadowedPointLight() = default;

    explicit ShadowedPointLight(const PointLight& pointLight)
        : position(pointLight.position),
          color(pointLight.color),
          intensity(pointLight.intensity),
          attenuation(pointLight.attenuation)
    {
        const auto projection = glm::perspectiveRH_ZO
        (
            glm::radians(90.0f),
            static_cast<f32>(GPU::POINT_SHADOW_DIMENSIONS.x) / static_cast<f32>(GPU::POINT_SHADOW_DIMENSIONS.y),
            GPU::POINT_LIGHT_SHADOW_PLANES.x,
            GPU::POINT_LIGHT_SHADOW_PLANES.y
        );

        matrices[0] = projection * glm::lookAtRH(position, position + glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f));
        matrices[1] = projection * glm::lookAtRH(position, position + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f));
        matrices[2] = projection * glm::lookAtRH(position, position + glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f));
        matrices[3] = projection * glm::lookAtRH(position, position + glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f));
        matrices[4] = projection * glm::lookAtRH(position, position + glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f));
        matrices[5] = projection * glm::lookAtRH(position, position + glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f));
    }
    #endif

    vec3 position;
    vec3 color;
    vec3 intensity;
    vec3 attenuation;
    mat4 matrices[6];
};

struct SpotLight
{
    vec3 position;
    vec3 color;
    vec3 intensity;
    vec3 attenuation;
    vec3 direction;
    vec2 cutOff;
};

struct ShadowedSpotLight
{
    #ifdef __cplusplus
    ShadowedSpotLight() = default;

    explicit ShadowedSpotLight(const SpotLight& spotLight)
        : position(spotLight.position),
          color(spotLight.color),
          intensity(spotLight.intensity),
          attenuation(spotLight.attenuation),
          direction(spotLight.direction),
          cutOff(spotLight.cutOff)
    {
        const glm::mat4 projection = glm::perspectiveRH_ZO
        (
            glm::radians(90.0f),
            1.0f,
            GPU::SPOT_LIGHT_SHADOW_PLANES.x,
            GPU::SPOT_LIGHT_SHADOW_PLANES.y
        );

        const glm::mat4 view = glm::lookAt
        (
           position,
           glm::vec3(0.0f, 0.0f, 0.0f),
           glm::vec3(0.0f, 1.0f, 0.0f)
        );

        matrix = projection * view;
    }
    #endif

    vec3 position;
    vec3 color;
    vec3 intensity;
    vec3 attenuation;
    vec3 direction;
    vec2 cutOff;
    mat4 matrix;
};

#ifdef __cplusplus

template<typename T>
concept IsLightType = std::is_same_v<T, DirLight          > ||
                      std::is_same_v<T, PointLight        > ||
                      std::is_same_v<T, ShadowedPointLight> ||
                      std::is_same_v<T, SpotLight         > ||
                      std::is_same_v<T, ShadowedSpotLight >  ;

#endif

#ifndef __cplusplus

#include "Constants.glsl"

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer CommonLightBuffer
{
    vec2 pointLightShadowPlanes;
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer DirLightBuffer
{
    uint     count;
    DirLight lights[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer PointLightBuffer
{
    uint       count;
    PointLight lights[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer ShadowedPointLightBuffer
{
    uint               count;
    ShadowedPointLight lights[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer SpotLightBuffer
{
    uint      count;
    SpotLight lights[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer ShadowedSpotLightBuffer
{
    uint              count;
    ShadowedSpotLight lights[];
};

struct LightInfo
{
    vec3 L;
    vec3 radiance;
};

float CalculateAttenuation(vec3 position, vec3 ATT, vec3 fragPosition)
{
    float distance = length(position - fragPosition);

    float attenuation = ATT.x + (ATT.y * distance) + (ATT.z * distance * distance);
          attenuation = 1.0f / max(attenuation, 0.0001f);

    return attenuation;
}

float CalculateSpotIntensity(vec3 L, vec3 direction, vec2 cutOff)
{
    float theta     = dot(L, normalize(-direction));
    vec2  cutOffCos = cos(cutOff);
    float epsilon   = cutOffCos.x - cutOffCos.y;
    float intensity = smoothstep(0.0f, 1.0f, (theta - cutOffCos.y) / epsilon);

    return intensity;
}

LightInfo GetLightInfo(DirLight light)
{
    LightInfo info;

    info.L        = normalize(-light.position);
    info.radiance = light.color * light.intensity;

    return info;
}

LightInfo GetLightInfo(PointLight light, vec3 fragPosition)
{
    LightInfo info;

    info.L            = normalize(light.position - fragPosition);
    float attenuation = CalculateAttenuation(light.position, light.attenuation, fragPosition);
    info.radiance     = light.color.rgb * light.intensity * attenuation;

    return info;
}

LightInfo GetLightInfo(ShadowedPointLight light, vec3 fragPosition)
{
    LightInfo info;

    info.L            = normalize(light.position - fragPosition);
    float attenuation = CalculateAttenuation(light.position, light.attenuation, fragPosition);
    info.radiance     = light.color.rgb * light.intensity * attenuation;

    return info;
}

LightInfo GetLightInfo(SpotLight light, vec3 fragPosition)
{
    LightInfo info;

    info.L            = normalize(light.position - fragPosition);
    float attenuation = CalculateAttenuation(light.position, light.attenuation, fragPosition);
    float intensity   = CalculateSpotIntensity(info.L, light.direction, light.cutOff);
    info.radiance     = light.color * light.intensity * attenuation * intensity;

    return info;
}

LightInfo GetLightInfo(ShadowedSpotLight light, vec3 fragPosition)
{
    LightInfo info;

    info.L            = normalize(light.position - fragPosition);
    float attenuation = CalculateAttenuation(light.position, light.attenuation, fragPosition);
    float intensity   = CalculateSpotIntensity(info.L, light.direction, light.cutOff);
    info.radiance     = light.color * light.intensity * attenuation * intensity;

    return info;
}

#endif

GLSL_NAMESPACE_END

#endif