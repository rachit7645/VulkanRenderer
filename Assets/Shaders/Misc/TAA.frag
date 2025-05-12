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

// References:
// https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail
// http://behindthepixels.io/assets/files/TemporalAA.pdf
// http://leiy.cc/publications/TAA/TAA_EG2020_Talk.pdf
// https://advances.realtimerendering.com/s2014/index.html#_HIGH-QUALITY_TEMPORAL_SUPERSAMPLING

// Implementaion referenced: https://github.com/bevyengine/bevy/blob/main/crates/bevy_core_pipeline/src/taa/taa.wgsl

#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2    : enable
#extension GL_EXT_scalar_block_layout  : enable

#include "Constants/Misc/TAA.glsl"
#include "MegaSet.glsl"
#include "Math.glsl"
#include "Color.glsl"
#include "Constants.glsl"

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec3 resolvedOutput;
layout(location = 1) out vec4 historyOutput;

vec3 ClipTowardsAABBCenter(vec3 historyColor, vec3 currentColor, vec3 min, vec3 max);
vec3 ToneMap(vec3 color);
vec3 ReverseToneMap(vec3 color);
vec3 SampleSceneColor(vec2 fragUV);

void main()
{
    vec2 sceneColorSize = vec2(textureSize(sampler2D(Textures[Constants.CurrentColorIndex], Samplers[Constants.PointSamplerIndex]), 0));
    vec2 texelSize      = 1.0f / sceneColorSize;

    // https://advances.realtimerendering.com/s2014/index.html#_HIGH-QUALITY_TEMPORAL_SUPERSAMPLING, slide 27

    float offset = texelSize.x * 2.0f;

    vec2 depthUVTopLeft     = fragUV + vec2(-offset,  offset);
    vec2 depthUVTopRight    = fragUV + vec2( offset,  offset);
    vec2 depthUVBottomLeft  = fragUV + vec2(-offset, -offset);
    vec2 depthUVBottomRight = fragUV + vec2( offset, -offset);

    vec2 closestUV = fragUV;

    float depthTopLeft  = texture(sampler2D(Textures[Constants.SceneDepthIndex], Samplers[Constants.PointSamplerIndex]), depthUVTopLeft).r;
    float depthTopRight = texture(sampler2D(Textures[Constants.SceneDepthIndex], Samplers[Constants.PointSamplerIndex]), depthUVTopRight).r;

    float closestDepth = texture(sampler2D(Textures[Constants.SceneDepthIndex], Samplers[Constants.PointSamplerIndex]), fragUV).r;

    float depthBottomLeft  = texture(sampler2D(Textures[Constants.SceneDepthIndex], Samplers[Constants.PointSamplerIndex]), depthUVBottomLeft).r;
    float depthBottomRight = texture(sampler2D(Textures[Constants.SceneDepthIndex], Samplers[Constants.PointSamplerIndex]), depthUVBottomRight).r;

    if (depthTopLeft > closestDepth)
    {
        closestUV    = depthUVTopLeft;
        closestDepth = depthTopLeft;
    }

    if (depthTopRight > closestDepth)
    {
        closestUV    = depthUVTopRight;
        closestDepth = depthTopRight;
    }

    if (depthBottomLeft > closestDepth)
    {
        closestUV    = depthUVBottomLeft;
        closestDepth = depthBottomLeft;
    }

    if (depthBottomRight > closestDepth)
    {
        closestUV = depthUVBottomRight;
    }

    vec2 closestMotionVector = texture(sampler2D(Textures[Constants.VelocityIndex], Samplers[Constants.PointSamplerIndex]), closestUV).rg;

    // Catmull-Rom filtering: https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
    // Ignoring corners:      https://www.activision.com/cdn/research/Dynamic_Temporal_Antialiasing_and_Upsampling_in_Call_of_Duty_v4.pdf#page=68

    vec2 historyUV      = fragUV - closestMotionVector;
    vec2 samplePosition = historyUV * sceneColorSize;
    vec2 texelCenter    = floor(samplePosition - 0.5f) + 0.5f;

    vec2 f   = samplePosition - texelCenter;
    vec2 w0  = f * (-0.5f + f * (1.0f - 0.5f * f));
    vec2 w1  = 1.0f + f * f * (-2.5f + 1.5f * f);
    vec2 w2  = f * (0.5f + f * (2.0f - 1.5f * f));
    vec2 w3  = f * f * (-0.5f + 0.5f * f);
    vec2 w12 = w1 + w2;

    vec2 texelPosition0  = (texelCenter - 1.0f)       * texelSize;
    vec2 texelPosition3  = (texelCenter + 2.0f)       * texelSize;
    vec2 texelPosition12 = (texelCenter + (w2 / w12)) * texelSize;

    vec3 historyColor  = texture(sampler2D(Textures[Constants.HistoryBufferIndex], Samplers[Constants.LinearSamplerIndex]), vec2(texelPosition12.x, texelPosition0.y )).rgb * w12.x * w0.y;
         historyColor += texture(sampler2D(Textures[Constants.HistoryBufferIndex], Samplers[Constants.LinearSamplerIndex]), vec2(texelPosition0.x,  texelPosition12.y)).rgb * w0.x  * w12.y;
         historyColor += texture(sampler2D(Textures[Constants.HistoryBufferIndex], Samplers[Constants.LinearSamplerIndex]), vec2(texelPosition12.x, texelPosition12.y)).rgb * w12.x * w12.y;
         historyColor += texture(sampler2D(Textures[Constants.HistoryBufferIndex], Samplers[Constants.LinearSamplerIndex]), vec2(texelPosition3.x,  texelPosition12.y)).rgb * w3.x  * w12.y;
         historyColor += texture(sampler2D(Textures[Constants.HistoryBufferIndex], Samplers[Constants.LinearSamplerIndex]), vec2(texelPosition12.x, texelPosition3.y )).rgb * w12.x * w3.y;

    vec3 currentColor = texture(sampler2D(Textures[Constants.CurrentColorIndex], Samplers[Constants.PointSamplerIndex]), fragUV).rgb;
    currentColor = ToneMap(currentColor);

    // YCoCg: https://advances.realtimerendering.com/s2014/index.html#_HIGH-QUALITY_TEMPORAL_SUPERSAMPLING, slide 33
    // Variance clipping: https://developer.download.nvidia.com/gameworks/events/GDC2016/msalvi_temporal_supersampling.pdf
    vec3 sampleTopLeft      = SampleSceneColor(fragUV + vec2(-texelSize.x,  texelSize.y));
    vec3 sampleTopMiddle    = SampleSceneColor(fragUV + vec2( 0.0f,         texelSize.y));
    vec3 sampleTopRight     = SampleSceneColor(fragUV + vec2( texelSize.x,  texelSize.y));
    vec3 sampleMiddleLeft   = SampleSceneColor(fragUV + vec2(-texelSize.x,  0.0f));
    vec3 sampleMiddleMiddle = RGBToYCoCg(currentColor);
    vec3 sampleMiddleRight  = SampleSceneColor(fragUV + vec2( texelSize.x,  0.0f));
    vec3 sampleBottomLeft   = SampleSceneColor(fragUV + vec2(-texelSize.x, -texelSize.y));
    vec3 sampleBottomMiddle = SampleSceneColor(fragUV + vec2( 0.0f,        -texelSize.y));
    vec3 sampleBottomRight  = SampleSceneColor(fragUV + vec2( texelSize.x, -texelSize.y));

    vec3 moment1 = sampleTopLeft + sampleTopMiddle + sampleTopRight + sampleMiddleLeft + sampleMiddleMiddle + sampleMiddleRight + sampleBottomLeft + sampleBottomMiddle + sampleBottomRight;
    vec3 moment2 = (sampleTopLeft * sampleTopLeft) + (sampleTopMiddle * sampleTopMiddle) + (sampleTopRight * sampleTopRight) + (sampleMiddleLeft * sampleMiddleLeft) + (sampleMiddleMiddle * sampleMiddleMiddle) + (sampleMiddleRight * sampleMiddleRight) + (sampleBottomLeft * sampleBottomLeft) + (sampleBottomMiddle * sampleBottomMiddle) + (sampleBottomRight * sampleBottomRight);

    vec3 mean         = moment1 / 9.0f;
    vec3 variance     = (moment2 / 9.0f) - (mean * mean);
    vec3 stdDeviation = sqrt(max(variance, vec3(0.0f)));

    historyColor = RGBToYCoCg(historyColor);
    historyColor = ClipTowardsAABBCenter(historyColor, sampleMiddleMiddle, mean - stdDeviation, mean + stdDeviation);
    historyColor = YCoCgToRGB(historyColor);

    float historyConfidence = texture(sampler2D(Textures[Constants.HistoryBufferIndex], Samplers[Constants.PointSamplerIndex]), fragUV).a;
    vec2  pixelMotionVector = abs(closestMotionVector) * sceneColorSize;

    if (pixelMotionVector.x < 0.01f && pixelMotionVector.y < 0.01f)
    {
        historyConfidence += 10.0f;
    } else
    {
        historyConfidence = 1.0f;
    }

    // https://hhoppe.com/supersample.pdf, section 4.1
    float currentColorFactor = clamp(1.0f / historyConfidence, TAA_MIN_HISTORY_BLEND_RATE, TAA_DEFAULT_HISTORY_BLEND_RATE);
    vec2  clampedHistoryUV   = saturate(historyUV);

    if (clampedHistoryUV.x != historyUV.x || clampedHistoryUV.y != historyUV.y)
    {
        currentColorFactor = 1.0f;
        historyConfidence  = 1.0f;
    }

    currentColor = mix(historyColor, currentColor, currentColorFactor);

    resolvedOutput = ReverseToneMap(currentColor);
    historyOutput  = vec4(currentColor, historyConfidence);
}

// https://github.com/playdeadgames/temporal/blob/master/Assets/Shaders/TemporalReprojection.shader
vec3 ClipTowardsAABBCenter(vec3 historyColor, vec3 currentColor, vec3 min, vec3 max)
{
    vec3  pClip  = 0.5f * (max + min);
    vec3  eClip  = 0.5f * (max - min) + 0.00000001f;
    vec3  vClip  = historyColor - pClip;
    vec3  vUnit  = vClip / eClip;
    vec3  aUnit  = abs(vUnit);
    float maUnit = max3(aUnit);

    if (maUnit > 1.0f)
    {
        return pClip + (vClip / maUnit);
    }
    else
    {
        return historyColor;
    }
}

vec3 ToneMap(vec3 color)
{
    return color * rcp(max3(color) + 1.0f);
}

vec3 ReverseToneMap(vec3 color)
{
    return color * rcp(1.0f - max3(color));
}

vec3 SampleSceneColor(vec2 fragUV)
{
    vec3 color = texture(sampler2D(Textures[Constants.CurrentColorIndex], Samplers[Constants.PointSamplerIndex]), fragUV).rgb;

    return RGBToYCoCg(ToneMap(color));
}