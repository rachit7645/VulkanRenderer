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

#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2    : enable
#extension GL_EXT_scalar_block_layout  : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "Constants/Lighting.glsl"
#include "Color.glsl"
#include "PBR.glsl"
#include "MegaSet.glsl"
#include "Packing.glsl"

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec3 outColor;

void main()
{
    vec4 gAlbedo = texture(sampler2D(Textures[Constants.GAlbedoIndex], Samplers[Constants.GBufferSamplerIndex]), fragUV);
    vec3 albedo  = gAlbedo.rgb;

    vec4  gNormal_Rgh_Mtl = texture(sampler2D(Textures[Constants.GNormalIndex], Samplers[Constants.GBufferSamplerIndex]), fragUV);
    vec3  normal          = UnpackNormal(gNormal_Rgh_Mtl.rg);
    float roughness       = gNormal_Rgh_Mtl.b;
    float metallic        = gNormal_Rgh_Mtl.a;

    vec4  gDepth            = texture(sampler2D(Textures[Constants.SceneDepthIndex], Samplers[Constants.GBufferSamplerIndex]), fragUV);
    float depth             = gDepth.r;
    vec4  projectedPosition = Constants.Scene.inverseProjection * vec4(fragUV * 2.0f - 1.0f, depth, 1.0f);
    vec3  viewPosition      = projectedPosition.xyz / projectedPosition.w;
    vec3  worldPosition     = vec3(Constants.Scene.inverseView * vec4(viewPosition, 1.0f));

    vec3 toCamera = normalize(Constants.Scene.cameraPos - worldPosition);

    vec3 F0 = mix(vec3(0.04f), albedo, metallic);

    vec3 Lo = vec3(0.0f);

    for (uint i = 0; i < Constants.Scene.dirLights.count; ++i)
    {
        DirLight  light     = Constants.Scene.dirLights.lights[i];
        LightInfo lightInfo = GetLightInfo(light);

        float shadow = CalculateShadow
        (
            i,
            worldPosition,
            viewPosition,
            normal,
            lightInfo.L,
            Constants.Cascades,
            TextureArrays[Constants.ShadowMapIndex],
            Samplers[Constants.ShadowSamplerIndex]
        );

        Lo += CalculateLight
        (
            lightInfo,
            normal,
            toCamera,
            F0,
            albedo,
            roughness,
            metallic
        ) * shadow;
    }

    for (uint i = 0; i < Constants.Scene.pointLights.count; ++i)
    {
        PointLight light     = Constants.Scene.pointLights.lights[i];
        LightInfo  lightInfo = GetLightInfo(light, worldPosition);

        float shadow = CalculatePointShadow
        (
            i,
            worldPosition,
            light.position,
            Constants.Scene.cameraPos,
            Constants.PointShadows.pointShadowData[i],
            CubemapArrays[Constants.PointShadowMapIndex],
            Samplers[Constants.ShadowSamplerIndex]
        );

        Lo += CalculateLight
        (
            lightInfo,
            normal,
            toCamera,
            F0,
            albedo,
            roughness,
            metallic
        ) * shadow;
    }

    for (uint i = 0; i < Constants.Scene.spotLights.count; ++i)
    {
        SpotLight light     = Constants.Scene.spotLights.lights[i];
        LightInfo lightInfo = GetLightInfo(light, worldPosition);

        float shadow = CalculateSpotShadow
        (
            i,
            worldPosition,
            normal,
            light.position,
            Constants.SpotShadows.matrices[i],
            TextureArrays[Constants.SpotShadowMapIndex],
            Samplers[Constants.ShadowSamplerIndex]
        );

        Lo += CalculateLight
        (
            lightInfo,
            normal,
            toCamera,
            F0,
            albedo,
            roughness,
            metallic
        ) * (1.0f - shadow);
    }

    vec3  R          = reflect(-toCamera, normal);
    vec3  irradiance = texture(samplerCube(Cubemaps[Constants.IrradianceIndex], Samplers[Constants.IBLSamplerIndex]), normal).rgb;
    vec3  preFilter  = textureLod(samplerCube(Cubemaps[Constants.PreFilterIndex], Samplers[Constants.IBLSamplerIndex]), R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2  brdf       = texture(sampler2D(Textures[Constants.BRDFLUTIndex], Samplers[Constants.IBLSamplerIndex]), vec2(max(dot(normal, toCamera), 0.0f), roughness)).rg;
    float ao         = texture(sampler2D(Textures[Constants.AOIndex], Samplers[Constants.GBufferSamplerIndex]), fragUV).r;

    Lo += CalculateAmbient
    (
        normal,
        toCamera,
        F0,
        albedo,
        roughness,
        metallic,
        irradiance,
        preFilter,
        brdf,
        ao
    );

    outColor = Lo;
}