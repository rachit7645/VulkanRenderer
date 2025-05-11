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

#ifndef SCENE_GLSL
#define SCENE_GLSL

#include "Lights.glsl"

struct SceneMatrices
{
    mat4 projection;
    mat4 inverseProjection;
    mat4 jitteredProjection;
    mat4 view;
    mat4 inverseView;
};

layout(buffer_reference, scalar) readonly buffer SceneBuffer
{
    SceneMatrices currentMatrices;
    SceneMatrices previousMatrices;
    vec3          cameraPosition;

    float nearPlane;
    float farPlane;

    CommonLightBuffer        commonLight;
    DirLightBuffer           dirLights;
    PointLightBuffer         pointLights;
    ShadowedPointLightBuffer shadowedPointLights;
    SpotLightBuffer          spotLights;
    ShadowedSpotLightBuffer  shadowedSpotLights;
};

vec4 GetClipPosition(SceneMatrices sceneMatrices, vec2 screenUV, float depth)
{
    vec4 clipPosition = vec4(screenUV * 2.0f - 1.0f, depth, 1.0f);

    return clipPosition;
}

vec3 GetViewPosition(SceneMatrices sceneMatrices, vec2 screenUV, float depth)
{
    vec4 projectedPosition = sceneMatrices.inverseProjection * GetClipPosition(sceneMatrices, screenUV, depth);
    vec3 viewPosition      = projectedPosition.xyz / projectedPosition.w;

    return viewPosition;
}

vec3 GetWorldPosition(SceneMatrices sceneMatrices, vec2 screenUV, float depth)
{
    vec3 viewPosition  = GetViewPosition(sceneMatrices, screenUV, depth);
    vec3 worldPosition = vec3(sceneMatrices.inverseView * vec4(viewPosition, 1.0f));

    return worldPosition;
}

#endif