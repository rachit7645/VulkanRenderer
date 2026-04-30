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

#version 460

#extension GL_GOOGLE_include_directive          : enable
#extension GL_EXT_buffer_reference2             : enable
#extension GL_EXT_scalar_block_layout           : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "PBR.glsl"
#include "MegaSet.glsl"
#include "Encoding.glsl"
#include "ShadowMap.glsl"
#include "TiledLighting/Common.h"
#include "Deferred/Lighting.h"

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec3 outColor;

void main()
{
    vec2  viewportSize = vec2(textureSize(Textures[Constants.SceneDepthIndex], 0));
    uvec2 pixelCoord   = uvec2(viewportSize * fragUV);

    uvec2 tileID    = pixelCoord / TILE_SIZE;
    uvec2 tileCount = Constants.MaxTileID + 1;
    uint  tileIndex = tileCount.x * tileID.y + tileID.x;

    TileLightIndices tileLightIndices = Constants.TileLightIndices.indices[tileIndex];

    vec4  gAlbedoIoR  = texture(sampler2D(Textures[Constants.GAlbedoIndex], Samplers[Constants.GBufferSamplerIndex]), fragUV);
    vec3  albedo      = gAlbedoIoR.rgb;
    float reflectance = IoRToReflectance(UnpackIoR(gAlbedoIoR.a));

    vec4 gNormal = texture(sampler2D(Textures[Constants.GNormalIndex], Samplers[Constants.GBufferSamplerIndex]), fragUV);
    vec3 normal  = UnpackNormal(gNormal.rg);

    vec4  gRghMtlHrz = texture(sampler2D(Textures[Constants.GRghMtlIndex], Samplers[Constants.GBufferSamplerIndex]), fragUV);
    float roughness  = gRghMtlHrz.r;
    float metallic   = gRghMtlHrz.g;
    float horizon    = gRghMtlHrz.b;

    float depth         = texture(sampler2D(Textures[Constants.SceneDepthIndex], Samplers[Constants.GBufferSamplerIndex]), fragUV).r;
    vec3  worldPosition = GetWorldPosition(Constants.Scene.currentMatrices, fragUV, depth);

    vec3 toCamera = normalize(Constants.Scene.cameraPosition - worldPosition);

    vec3 Lo = vec3(0.0f);

    // Sun Light
    {
        DirLight  light     = Constants.Scene.Sun.light;
        LightInfo lightInfo = GetLightInfo(light);

        float shadow = texture(sampler2D(Textures[Constants.ShadowMapIndex], Samplers[Constants.GBufferSamplerIndex]), fragUV).r;

        Lo += shadow * CalculateLight
        (
            lightInfo,
            normal,
            toCamera,
            albedo,
            roughness,
            metallic,
            reflectance
        );
    }

    for (uint i = 0; i < tileLightIndices.pointLightCount; ++i)
    {
        uint       index     = tileLightIndices.pointLightIndices[i];
        PointLight light     = Constants.Scene.PointLights.lights[index];
        LightInfo  lightInfo = GetLightInfo(light, worldPosition);

        Lo += CalculateLight
        (
            lightInfo,
            normal,
            toCamera,
            albedo,
            roughness,
            metallic,
            reflectance
        );
    }

    for (uint i = 0; i < tileLightIndices.shadowedPointLightCount; ++i)
    {
        uint               index     = tileLightIndices.shadowedPointLightIndices[i];
        ShadowedPointLight light     = Constants.Scene.ShadowedPointLights.lights[index];
        LightInfo          lightInfo = GetLightInfo(light, worldPosition);

        float shadow = CalculatePointShadow
        (
            index,
            light,
            worldPosition,
            CubemapArrays[Constants.PointShadowMapIndex],
            Samplers[Constants.PointShadowSamplerIndex]
        );

        Lo += shadow * CalculateLight
        (
            lightInfo,
            normal,
            toCamera,
            albedo,
            roughness,
            metallic,
            reflectance
        );
    }

    for (uint i = 0; i < tileLightIndices.spotLightCount; ++i)
    {
        uint      index     = tileLightIndices.spotLightIndices[i];
        SpotLight light     = Constants.Scene.SpotLights.lights[index];
        LightInfo lightInfo = GetLightInfo(light, worldPosition);

        Lo += CalculateLight
        (
            lightInfo,
            normal,
            toCamera,
            albedo,
            roughness,
            metallic,
            reflectance
        );
    }

    for (uint i = 0; i < tileLightIndices.shadowedSpotLightCount; ++i)
    {
        uint              index     = tileLightIndices.shadowedSpotLightIndices[i];
        ShadowedSpotLight light     = Constants.Scene.ShadowedSpotLights.lights[index];
        LightInfo         lightInfo = GetLightInfo(light, worldPosition);

        float shadow = CalculateSpotShadow
        (
            index,
            light,
            worldPosition,
            normal,
            TextureArrays[Constants.SpotShadowMapIndex],
            Samplers[Constants.SpotShadowSamplerIndex]
        );

        Lo += shadow * CalculateLight
        (
            lightInfo,
            normal,
            toCamera,
            albedo,
            roughness,
            metallic,
            reflectance
        );
    }

    vec3 reflected = normalize(reflect(-toCamera, normal));

    uint  maxReflectionLod = textureQueryLevels(Cubemaps[Constants.PreFilterIndex]);
    float prefilterLod     = roughness * float(maxReflectionLod);
    vec3  preFilter        = textureLod(samplerCube(Cubemaps[Constants.PreFilterIndex], Samplers[Constants.IBLSamplerIndex]), reflected, prefilterLod).rgb;

    vec3 irradiance = texture(samplerCube(Cubemaps[Constants.IrradianceIndex], Samplers[Constants.IBLSamplerIndex]), normal).rgb;

    float NdotV = abs(dot(normal, toCamera)) + 1e-5f;
    vec2  brdf  = texture(sampler2D(Textures[Constants.BRDFLUTIndex], Samplers[Constants.IBLSamplerIndex]), vec2(NdotV, roughness)).rg;

    float ao = texture(sampler2D(Textures[Constants.AOIndex], Samplers[Constants.GBufferSamplerIndex]), fragUV).r;

    Lo += ao * CalculateAmbient
    (
        normal,
        toCamera,
        albedo,
        roughness,
        metallic,
        reflectance,
        horizon,
        irradiance,
        preFilter,
        brdf
    );

    vec3 emissive = texture(sampler2D(Textures[Constants.GEmmisiveIndex], Samplers[Constants.GBufferSamplerIndex]), fragUV).rgb;

    Lo += emissive;

    outColor = Lo;
}