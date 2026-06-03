/*
 * Copyright (c) 2023 - 2026 Rachit
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
#include "AABB.h"

#ifdef __cplusplus
#include "Renderer/RenderConstants.h"
#include "Util/Maths.h"
#endif

GLSL_NAMESPACE_BEGIN(GPU)

GLSL_CONSTEXPR u32 MAX_POINT_LIGHT_COUNT          = 16;
GLSL_CONSTEXPR u32 MAX_SHADOWED_POINT_LIGHT_COUNT =  4;
GLSL_CONSTEXPR u32 MAX_SPOT_LIGHT_COUNT           = 16;
GLSL_CONSTEXPR u32 MAX_SHADOWED_SPOT_LIGHT_COUNT  =  4;

#ifdef __cplusplus

constexpr glm::uvec2 POINT_SHADOW_DIMENSIONS = {512,  512};
constexpr glm::uvec2 SPOT_SHADOW_DIMENSIONS  = {1024, 1024};

#endif

struct DirLight
{
    GLSL_VEC3 direction;
    GLSL_VEC3 color;
    f32       intensity; // In Lux
};

struct PointLight
{
    GLSL_VEC3 position;
    GLSL_VEC3 color;
    f32       intensity; // In Candela
    f32       range;     // In Meters
};

struct ShadowedPointLight
{
    #ifdef __cplusplus
    ShadowedPointLight() = default;

    explicit ShadowedPointLight(const PointLight& pointLight)
        : position(pointLight.position),
          color(pointLight.color),
          intensity(pointLight.intensity),
          range(pointLight.range),
          matrices()
    {
        constexpr f32 ASPECT_RATIO = static_cast<f32>(GPU::POINT_SHADOW_DIMENSIONS.x) /
                                     static_cast<f32>(GPU::POINT_SHADOW_DIMENSIONS.y);

        constexpr f32 FAR_PLANE = 512.0f;

        auto projection = Maths::ProjectionReverseZ
        (
            glm::radians(90.0f),
            ASPECT_RATIO,
            Renderer::NEAR_PLANE,
            FAR_PLANE
        );

        projection[1][1] *= -1;

        matrices[0] = projection * glm::lookAtRH(position, position + glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f));
        matrices[1] = projection * glm::lookAtRH(position, position + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f));
        matrices[2] = projection * glm::lookAtRH(position, position + glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f));
        matrices[3] = projection * glm::lookAtRH(position, position + glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f));
        matrices[4] = projection * glm::lookAtRH(position, position + glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f));
        matrices[5] = projection * glm::lookAtRH(position, position + glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f));
    }
    #endif

    GLSL_VEC3 position;
    GLSL_VEC3 color;
    f32       intensity;   // In Candela
    f32       range;       // In Meters
    GLSL_MAT4 matrices[6];
};

struct SpotLight
{
    GLSL_VEC3 position;
    GLSL_VEC3 color;
    f32       intensity; // In Candela
    GLSL_VEC3 direction;
    GLSL_VEC2 cutOff;
    f32       range;     // In Meters
};

struct ShadowedSpotLight
{
    #ifdef __cplusplus
    ShadowedSpotLight() = default;

    explicit ShadowedSpotLight(const SpotLight& spotLight)
        : position(spotLight.position),
          color(spotLight.color),
          intensity(spotLight.intensity),
          direction(spotLight.direction),
          cutOff(spotLight.cutOff),
          range(spotLight.range),
          matrix(glm::identity<glm::mat4>())
    {
        constexpr f32 ASPECT_RATIO = static_cast<f32>(GPU::SPOT_SHADOW_DIMENSIONS.x) /
                                     static_cast<f32>(GPU::SPOT_SHADOW_DIMENSIONS.y);

        auto projection = Maths::InfiniteProjectionReverseZ
        (
            2.0f * cutOff.y,
            ASPECT_RATIO,
            Renderer::NEAR_PLANE
        );

        projection[1][1] *= -1;

        const auto view = glm::lookAtRH
        (
            position,
            position + glm::normalize(direction),
            Renderer::WORLD_UP
        );

        matrix = projection * view;
    }
    #endif

    GLSL_VEC3 position;
    GLSL_VEC3 color;
    f32       intensity; // In Candela
    GLSL_VEC3 direction;
    GLSL_VEC2 cutOff;
    f32       range;     // In Meters
    GLSL_MAT4 matrix;
};

#ifdef __cplusplus

template<typename T>
concept IsLightType = std::is_same_v<T, DirLight> ||
                      std::is_same_v<T, PointLight> ||
                      std::is_same_v<T, ShadowedPointLight> ||
                      std::is_same_v<T, SpotLight> ||
                      std::is_same_v<T, ShadowedSpotLight>;

struct LightsBuffer
{
    GPU::DirLight           sun                                                      = {};
    u32                     pointLightCount                                          = 0;
    GPU::PointLight         pointLights[GPU::MAX_POINT_LIGHT_COUNT]                  = {};
    u32                     shadowedPointLightCount                                  = 0;
    GPU::ShadowedPointLight shadowedPointLights[GPU::MAX_SHADOWED_POINT_LIGHT_COUNT] = {};
    u32                     spotLightCount                                           = 0;
    GPU::SpotLight          spotLights[GPU::MAX_SPOT_LIGHT_COUNT]                    = {};
    u32                     shadowedSpotLightCount                                   = 0;
    GPU::ShadowedSpotLight  shadowedSpotLights[GPU::MAX_SHADOWED_SPOT_LIGHT_COUNT]   = {};
    
    static constexpr VkDeviceSize GetSunOffset()
    {
        return offsetof(GPU::LightsBuffer, sun);
    }

    static constexpr VkDeviceSize GetPointLightOffset()
    {
        return offsetof(GPU::LightsBuffer, pointLightCount);
    }

    static constexpr VkDeviceSize GetShadowedPointLightOffset()
    {
        return offsetof(GPU::LightsBuffer, shadowedPointLightCount);
    }

    static constexpr VkDeviceSize GetSpotLightOffset()
    {
        return offsetof(GPU::LightsBuffer, spotLightCount);
    }

    static constexpr VkDeviceSize GetShadowedSpotLightOffset()
    {
        return offsetof(GPU::LightsBuffer, shadowedSpotLightCount);
    }
};

#endif

#ifndef __cplusplus

#include "Constants.glsl"
#include "Math.glsl"

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer SunBuffer
{
    DirLight light;
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

float CalculateAttenuation(vec3 position, float range, vec3 fragPosition)
{
    float distance = length(position - fragPosition);

    float N = saturate(1.0f - pow4(distance / max(range, 1e-5f)));
    float D = max(distance * distance, 1e-5f);

    float attenuation = N / D;

    return attenuation;
}

float CalculateSpotIntensity(vec3 L, vec3 direction, vec2 cutOff)
{
    float cosTheta = dot(L, normalize(-direction));
    float cosInner = cos(cutOff.x);
    float cosOuter = cos(cutOff.y);

    float epsilon   = max(cosInner - cosOuter, 1e-5f);
    float intensity = saturate((cosTheta - cosOuter) / epsilon);

    return intensity * intensity;
}

LightInfo GetLightInfo(DirLight light)
{
    LightInfo info;

    info.L        = normalize(-light.direction);
    info.radiance = light.color * light.intensity;

    return info;
}

LightInfo GetLightInfo(PointLight light, vec3 fragPosition)
{
    LightInfo info;

    info.L            = normalize(light.position - fragPosition);
    float attenuation = CalculateAttenuation(light.position, light.range, fragPosition);
    info.radiance     = light.color.rgb * light.intensity * attenuation;

    return info;
}

LightInfo GetLightInfo(ShadowedPointLight light, vec3 fragPosition)
{
    LightInfo info;

    info.L            = normalize(light.position - fragPosition);
    float attenuation = CalculateAttenuation(light.position, light.range, fragPosition);
    info.radiance     = light.color.rgb * light.intensity * attenuation;

    return info;
}

LightInfo GetLightInfo(SpotLight light, vec3 fragPosition)
{
    LightInfo info;

    info.L            = normalize(light.position - fragPosition);
    float attenuation = CalculateAttenuation(light.position, light.range, fragPosition);
    float intensity   = CalculateSpotIntensity(info.L, light.direction, light.cutOff);
    info.radiance     = light.color * light.intensity * attenuation * intensity;

    return info;
}

LightInfo GetLightInfo(ShadowedSpotLight light, vec3 fragPosition)
{
    LightInfo info;

    info.L            = normalize(light.position - fragPosition);
    float attenuation = CalculateAttenuation(light.position, light.range, fragPosition);
    float intensity   = CalculateSpotIntensity(info.L, light.direction, light.cutOff);
    info.radiance     = light.color * light.intensity * attenuation * intensity;

    return info;
}

#endif

GLSL_NAMESPACE_END

#endif