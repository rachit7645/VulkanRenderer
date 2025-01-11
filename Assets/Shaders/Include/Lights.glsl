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
    vec4 position;
    vec4 color;
    vec4 intensity;
};

struct PointLight
{
    vec4 position;
    vec4 color;
    vec4 intensity;
    vec4 attenuation;
};

struct SpotLight
{
    vec4 position;
    vec4 color;
    vec4 intensity;
    vec4 attenuation;
    vec4 direction;
    vec4 cutOff;
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

LightInfo GetDirLightInfo(DirLight light)
{
    LightInfo info;

    info.L        = normalize(-light.position.xyz);
    info.radiance = light.color.rgb * light.intensity.xyz;

    return info;
}

LightInfo GetPointLightInfo(PointLight light, vec3 fragPos)
{
    LightInfo info;

    info.L            = normalize(light.position.xyz - fragPos);
    float attenuation = CalculateAttenuation(light.position.xyz, light.attenuation.xyz, fragPos);
    info.radiance     = light.color.rgb * light.intensity.xyz * attenuation;

    return info;
}

LightInfo GetSpotLightInfo(SpotLight light, vec3 fragPos)
{
    LightInfo info;

    info.L            = normalize(light.position.xyz - fragPos);
    float attenuation = CalculateAttenuation(light.position.xyz, light.attenuation.xyz, fragPos);
    float intensity   = CalculateSpotIntensity(info.L, light.direction.xyz, light.cutOff.xy);
    info.radiance     = light.color.rgb * light.intensity.xyz * attenuation * intensity;

    return info;
}

#endif