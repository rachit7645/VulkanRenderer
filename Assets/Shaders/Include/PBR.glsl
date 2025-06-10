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

#include "Lights.h"
#include "Math.glsl"

float IoRToReflectance(float ior)
{
    float r = (ior - 1.0f) / (ior + 1.0f);

    return r * r;
}

vec3 CalculateF0(vec3 albedo, float metallic, float reflectance)
{
    return mix(vec3(reflectance), albedo, metallic);
}

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

// Smith's Self Shadowing
// V = G / (4 · NoL · NoV)
float VisibilitySmithGGX(float NdotL, float NdotV, float a)
{
    float a2 = a * a;

    float ggxV = NdotL * sqrt((NdotV - a2 * NdotV) * NdotV + a2);
    float ggxL = NdotV * sqrt((NdotL - a2 * NdotL) * NdotL + a2);

    return 0.5f / max(ggxV + ggxL, 1e-5f);
}

// Schlick's Fresnel Approximation
vec3 FresnelSchlick(vec3 F0, vec3 F90, float cosTheta)
{
    return F0 + (F90 - F0) * pow5(saturate(1.0f - cosTheta));
}

// Schlick's Fresnel Approximation, F90 = vec3(1.0f)
vec3 FresnelSchlick(vec3 F0, float cosTheta)
{
    return FresnelSchlick(F0, vec3(1.0f), cosTheta);
}

// Schlick's Fresnel Approximation
float FresnelSchlick(float F0, float F90, float cosTheta)
{
    return F0 + (F90 - F0) * pow5(saturate(1.0f - cosTheta));
}

vec3 Lambert(vec3 albedo)
{
    return INVERSE_PI * albedo;
}

// Burley 2012, "Physically-Based Shading at Disney"
vec3 Burley(vec3 albedo, float roughness, float NdotV, float NdotL, float LdotH)
{
    float F90 = 0.5f + 2.0f * roughness * LdotH * LdotH;

    float lightScatter = FresnelSchlick(1.0f, F90, NdotL);
    float viewScatter  = FresnelSchlick(1.0f, F90, NdotV);

    float factor = lightScatter * viewScatter * INVERSE_PI;

    return factor * albedo;
}

vec3 DiffuseEnergyFactor(vec3 F, float metallic)
{
    vec3 kS = F;
    vec3 kD = vec3(1.0f) - kS;
    kD     *= 1.0f - metallic;

    return kD;
}

vec3 CalculateLight
(
    LightInfo lightInfo,
    vec3 N,
    vec3 V,
    vec3 albedo,
    float roughness,
    float metallic,
    float reflectance
)
{
    vec3 F0 = CalculateF0(albedo, metallic, reflectance);

    vec3 L        = lightInfo.L;
    vec3 radiance = lightInfo.radiance;

    vec3 H = normalize(V + L);

    float NdotV = abs(dot(N, V)) + 1e-5f;
    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    // Brent Burley (2012)
    float r2 = roughness * roughness;

	// Cook-Torrance BRDF
	float D = DistributionGGX(NdotH, r2);
	float G = VisibilitySmithGGX(NdotL, NdotV, r2);
	vec3  F = FresnelSchlick(F0, VdotH);

    vec3 diffuse  = Lambert(albedo);
    vec3 specular = D * G * F;

	vec3 kD = DiffuseEnergyFactor(F, metallic);

	return (kD * diffuse + specular) * radiance * NdotL;
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

// Schlick's Fresnel approximation with injected roughness parameter
vec3 FresnelSchlick_IBL(vec3 F0, float roughness, float cosTheta)
{
    return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow5(saturate(1.0f - cosTheta));
}

vec3 CalculateAmbient
(
    vec3 N,
    vec3 V,
    vec3 albedo,
    float roughness,
    float metallic,
    float reflectance,
    vec3 irradiance,
    vec3 preFilter,
    vec2 brdf
)
{
    vec3 F0 = CalculateF0(albedo, metallic, reflectance);

    float NdotV = abs(dot(N, V)) + 1e-5f;

    vec3 F = FresnelSchlick_IBL(F0, roughness, NdotV);

    vec3 kD = DiffuseEnergyFactor(F, metallic);

    vec3 diffuse  = irradiance * albedo;
    vec3 specular = preFilter * (F * brdf.x + brdf.y);

    return kD * diffuse + specular;
}

#endif