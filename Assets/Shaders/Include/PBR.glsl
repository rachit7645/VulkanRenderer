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

// PBR functions

#ifndef PBR_GLSL
#define PBR_GLSL

#include "Lights.glsl"

// Trowbridge-Reitz Normal Distribution Function
float DistributionGGX(float NdotH, float a)
{
	float a2     = a * a;
	float NdotH2 = NdotH * NdotH;

	float nom   = a2;
	float denom = NdotH2 * (a2 - 1.0f) + 1.0f;
	denom       = PI * denom * denom;

	return nom / denom;
}

// Schlick's Approximation for IBL
float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0f;

    float nom   = NdotV;
    float denom = NdotV * (1.0f - k) + k;

    return nom / denom;
}

// Smith's Self Shadowing for IBL
float GeometrySmith_IBL(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);

    float ggx1 = GeometrySchlickGGX_IBL(NdotL, roughness);
    float ggx2 = GeometrySchlickGGX_IBL(NdotV, roughness);

    return ggx1 * ggx2;
}

// Smith's Self Shadowing
float GeometrySmith(float NdotL, float NdotV, float a)
{
    float a2 = a * a;

    float ggxV = NdotL * sqrt(NdotV * NdotV * (1.0f - a2) + a2);
    float ggxL = NdotV * sqrt(NdotL * NdotL * (1.0f - a2) + a2);

    // Add epsilon to avoid division by zero
    float ggx = ggxV + ggxL + 1e-5f;

    return 0.5f / ggx;
}

// Fresnel equation
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

// Fresnel equation with injected roughness parameter
vec3 FresnelSchlick_IBL(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

vec3 CalculateLight
(
    LightInfo lightInfo,
    vec3 N,
    vec3 V,
    vec3 F0,
    vec3 albedo,
    float roughness,
    float metallic
)
{
    vec3 L = lightInfo.L;
    vec3 H = normalize(V + L);

    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float NdotH = max(dot(N, H), 0.0f);
    float HdotV = max(dot(H, V), 0.0f);

    // Squared Roughness as proposed by Disney
    float a = roughness * roughness;

	// Cook-Torrance BRDF
	float NDF = DistributionGGX(NdotH, a);
	float G   = GeometrySmith(NdotL, NdotV, a);
	vec3  F   = FresnelSchlick(HdotV, F0);

	// Specular
	vec3 specular = NDF * G * F;

	// Diffuse energy conservation
	vec3 kS = F;
	vec3 kD = vec3(1.0f) - kS;
	kD     *= 1.0f - metallic;

	return (kD * (albedo / PI) + specular) * lightInfo.radiance * NdotL;
}

vec3 CalculateAmbient
(
    vec3 N,
    vec3 V,
    vec3 F0,
    vec3 albedo,
    float roughness,
    float metallic,
    vec3 irradiance,
    vec3 preFilter,
    vec2 brdf
)
{
    vec3 F  = FresnelSchlick_IBL(max(dot(N, V), 0.0f), F0, roughness);

    // Energy conservation
    vec3 kS = F;
    vec3 kD = 1.0f - kS;
    kD     *= 1.0f - metallic;

    vec3 diffuse  = irradiance * albedo;
    vec3 specular = preFilter * (F * brdf.x + brdf.y);

    return kD * diffuse + specular;
}

#endif