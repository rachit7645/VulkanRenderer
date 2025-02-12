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
#extension GL_EXT_buffer_reference     : enable
#extension GL_EXT_scalar_block_layout  : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "Constants/Forward.glsl"
#include "Material.glsl"
#include "Color.glsl"
#include "Lights.glsl"
#include "PBR.glsl"
#include "MegaSet.glsl"
#include "CSM.glsl"

layout(location = 0) in      vec3 fragPosition;
layout(location = 1) in      vec3 fragViewPosition;
layout(location = 2) in      vec2 fragTexCoords;
layout(location = 3) in      vec3 fragToCamera;
layout(location = 4) in flat uint fragDrawID;
layout(location = 5) in      mat3 fragTBNMatrix;

layout(location = 0) out vec4 outColor;

void main()
{
    Mesh mesh = Constants.Meshes.meshes[fragDrawID];

    vec4 albedo = texture(sampler2D(textures[MAT_ALBEDO_ID], samplers[Constants.TextureSamplerIndex]), fragTexCoords);
    albedo.rgb  = ToLinear(albedo.rgb);
    albedo     *= mesh.albedoFactor;

    vec3 normal = texture(sampler2D(textures[MAT_NORMAL_ID], samplers[Constants.TextureSamplerIndex]), fragTexCoords).rgb;
    normal      = GetNormalFromMap(normal, fragTBNMatrix);

    vec3 aoRghMtl = texture(sampler2D(textures[MAT_AO_RGH_MTL_ID], samplers[Constants.TextureSamplerIndex]), fragTexCoords).rgb;
    aoRghMtl.g   *= mesh.roughnessFactor;
    aoRghMtl.b   *= mesh.metallicFactor;

    vec3 F0 = mix(vec3(0.04f), albedo.rgb, aoRghMtl.b);

    vec3 Lo = vec3(0.0f);

    if (Constants.Scene.dirLights.count > 0)
    {
        LightInfo sunLightInfo = GetLightInfo(Constants.Scene.dirLights.lights[0]);

        float shadow = CalculateShadow
        (
            fragPosition,
            fragViewPosition,
            normal,
            sunLightInfo.L,
            textureArrays[Constants.ShadowMapIndex],
            samplers[Constants.ShadowSamplerIndex],
            Constants.Cascades
        );

        Lo += CalculateLight
        (
            sunLightInfo,
            normal,
            fragToCamera,
            F0,
            albedo.rgb,
            aoRghMtl.g,
            aoRghMtl.b
        ) * shadow;
    }

    for (uint i = 1; i < Constants.Scene.dirLights.count; ++i)
    {
        LightInfo lightInfo = GetLightInfo(Constants.Scene.dirLights.lights[i]);

        Lo += CalculateLight
        (
            lightInfo,
            normal,
            fragToCamera,
            F0,
            albedo.rgb,
            aoRghMtl.g,
            aoRghMtl.b
        );
    }

    for (uint i = 0; i < Constants.Scene.pointLights.count; ++i)
    {
        LightInfo lightInfo = GetLightInfo(Constants.Scene.pointLights.lights[i], fragPosition);

        Lo += CalculateLight
        (
            lightInfo,
            normal,
            fragToCamera,
            F0,
            albedo.rgb,
            aoRghMtl.g,
            aoRghMtl.b
        );
    }

    for (uint i = 0; i < Constants.Scene.spotLights.count; ++i)
    {
        LightInfo lightInfo = GetLightInfo(Constants.Scene.spotLights.lights[i], fragPosition);

        Lo += CalculateLight
        (
            lightInfo,
            normal,
            fragToCamera,
            F0,
            albedo.rgb,
            aoRghMtl.g,
            aoRghMtl.b
        );
    }

    vec3 R          = reflect(-fragToCamera, normal);
    vec3 irradiance = texture(samplerCube(cubemaps[Constants.IrradianceIndex], samplers[Constants.IBLSamplerIndex]), normal).rgb;
    vec3 preFilter  = textureLod(samplerCube(cubemaps[Constants.PreFilterIndex], samplers[Constants.IBLSamplerIndex]), R, aoRghMtl.g * MAX_REFLECTION_LOD).rgb;
    vec2 brdf       = texture(sampler2D(textures[Constants.BRDFLUTIndex], samplers[Constants.IBLSamplerIndex]), vec2(max(dot(normal, fragToCamera), 0.0f), aoRghMtl.g)).rg;

    Lo += CalculateAmbient
    (
        normal,
        fragToCamera,
        F0,
        albedo.rgb,
        aoRghMtl.g,
        aoRghMtl.b,
        irradiance,
        preFilter,
        brdf
    );

    outColor = vec4(Lo, 1.0f);
}