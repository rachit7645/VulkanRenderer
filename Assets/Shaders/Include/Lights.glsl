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

// Types of light

#ifndef LIGHTS_GLSL
#define LIGHTS_GLSL

#include "Constants.glsl"

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

struct SpotLight
{
    vec3 position;
    vec3 color;
    vec3 intensity;
    vec3 attenuation;
    vec3 direction;
    vec2 cutOff;
};

layout(buffer_reference, scalar) readonly buffer DirLightBuffer
{
    uint     count;
    DirLight lights[];
};

layout(buffer_reference, scalar) readonly buffer PointLightBuffer
{
    uint       count;
    PointLight lights[];
};

layout(buffer_reference, scalar) readonly buffer SpotLightBuffer
{
    uint      count;
    SpotLight lights[];
};

struct LightInfo
{
    vec3 L;
    vec3 radiance;
};

float CalculateAttenuation(vec3 position, vec3 ATT, vec3 fragPos)
{
    float distance = length(position - fragPos);

    float attenuation = ATT.x + (ATT.y * distance) + (ATT.z * distance * distance);
          attenuation = 1.0f / max(attenuation, 0.0001f);

    return attenuation;
}

float CalculateSpotIntensity(vec3 L, vec3 direction, vec2 cutOff)
{
    float theta     = dot(L, normalize(-direction));
    float epsilon   = cutOff.x - cutOff.y;
    float intensity = smoothstep(0.0f, 1.0f, (theta - cutOff.y) / epsilon);

    return intensity;
}

LightInfo GetLightInfo(DirLight light)
{
    LightInfo info;

    info.L        = normalize(-light.position);
    info.radiance = light.color * light.intensity;

    return info;
}

LightInfo GetLightInfo(PointLight light, vec3 fragPos)
{
    LightInfo info;

    info.L            = normalize(light.position - fragPos);
    float attenuation = CalculateAttenuation(light.position, light.attenuation, fragPos);
    info.radiance     = light.color.rgb * light.intensity * attenuation;

    return info;
}

LightInfo GetLightInfo(SpotLight light, vec3 fragPos)
{
    LightInfo info;

    info.L            = normalize(light.position - fragPos);
    float attenuation = CalculateAttenuation(light.position, light.attenuation, fragPos);
    float intensity   = CalculateSpotIntensity(info.L, light.direction, light.cutOff);
    info.radiance     = light.color * light.intensity * attenuation * intensity;

    return info;
}

#endif