// Types of light

#ifndef LIGHTS_GLSL
#define LIGHTS_GLSL

#include "Constants.glsl"

// Directional Light
struct DirLight
{
    vec4 position;
    vec4 color;
    vec4 intensity;
};

// Point Light
struct PointLight
{
    vec4 position;
    vec4 color;
    vec4 intensity;
    vec4 attenuation;
};

// Spot Light
struct SpotLight
{
    vec4 position;
    vec4 color;
    vec4 intensity;
    vec4 attenuation;
    vec4 direction;
    vec4 cutOff;
};

// Common Light Information
struct LightInfo
{
    vec3 L;
    vec3 radiance;
};

// Calculate attenuation
float CalculateAttenuation(vec3 position, vec3 ATT, vec3 fragPos)
{
    // Get distance
    float distance = length(position - fragPos);
    // Calculate attenuation
    float attenuation = ATT.x + (ATT.y * distance) + (ATT.z * distance * distance);
    // Make attenuation multiplicable
    attenuation = 1.0f / max(attenuation, 0.0001f);
    // Return
    return attenuation;
}

// Calculate spot light intensity
float CalculateSpotIntensity(vec3 L, vec3 direction, vec2 cutOff)
{
    // Calculate angle
    float theta = dot(L, normalize(-direction));
    // Calculate cut off epsilon
    float epsilon = cutOff.x - cutOff.y;
    // Calculate intensity
    float intensity = smoothstep(0.0f, 1.0f, (theta - cutOff.y) / epsilon);
    // Return
    return intensity;
}

// Get directional light information
LightInfo GetDirLightInfo(DirLight light)
{
    LightInfo info;
    // Light vector
    info.L = normalize(-light.position.xyz);
    // Radiance
    info.radiance = light.color.rgb * light.intensity.xyz;
    // Return
    return info;
}

// Get point light information
LightInfo GetPointLightInfo(PointLight light, vec3 fragPos)
{
    LightInfo info;
    // Light vector
    info.L = normalize(light.position.xyz - fragPos);
    // Attenuation
    float attenuation = CalculateAttenuation(light.position.xyz, light.attenuation.xyz, fragPos);
    // Radiance
    info.radiance = light.color.rgb * light.intensity.xyz * attenuation;
    // Return
    return info;
}

// Get spot light information
LightInfo GetSpotLightInfo(SpotLight light, vec3 fragPos)
{
    LightInfo info;
    // Light vector
    info.L = normalize(light.position.xyz - fragPos);
    // Attenuation
    float attenuation = CalculateAttenuation(light.position.xyz, light.attenuation.xyz, fragPos);
    // Spot intensity
    float intensity = CalculateSpotIntensity(info.L, light.direction.xyz, light.cutOff.xy);
    // Radiance
    info.radiance = light.color.rgb * light.intensity.xyz * attenuation * intensity;
    // Return
    return info;
}

#endif