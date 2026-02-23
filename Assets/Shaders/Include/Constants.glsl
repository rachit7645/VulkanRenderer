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

// Constants

#ifndef CONSTANTS_GLSL
#define CONSTANTS_GLSL

// Math constants
const float PI             = 3.1415926535897932384626433832795f;
const float HALF_PI        = 1.5707963267948966192313216916398f;
const float TWO_PI         = 6.2831853071795864769252867665590f;
const float INVERSE_PI     = 0.3183098861837906715377675267450f;
const float INVERSE_TWO_PI = 0.1591549430918953357688837633725f;

// Float limits
const float FLOAT_MIN = 1.175494351e-38;
const float FLOAT_MAX = 3.402823466e+38;

// IBL Constants
const float CONVOLUTION_SAMPLE_DELTA = 0.025f;
const uint  BRDF_LUT_SAMPLE_COUNT    = 1024u;

// Point Shadow Constants
const float POINT_SHADOW_BIAS = 0.15f;

// Spot shadow constants
const float SPOT_SHADOW_MIN_BIAS = 0.0001f;
const float SPOT_SHADOW_MAX_BIAS = 0.001f;

// RT Shadow Constants
const float RT_SHADOW_MIN_BIAS = 0.0005f;
const float RT_SHADOW_MAX_BIAS = 0.005f;

// TAA Constants
const float TAA_DEFAULT_HISTORY_BLEND_RATE = 0.1f;
const float TAA_MIN_HISTORY_BLEND_RATE     = 0.015f;

// VBAO Constants
const uint VBAO_SLICE_COUNT  = 3;
const uint VBAO_SAMPLE_COUNT = 3;
const uint VBAO_SECTOR_COUNT = 32;

const float VBAO_DEPTH_RANGE_SCALE_FACTOR  = 0.75f;
const float VBAO_DEFAULT_RADIUS            = 0.5f;
const float VBAO_DEFAULT_RADIUS_MULTIPLIER = 1.457f;
const float VBAO_DEFAULT_FALLOFF_RANGE     = 0.615f;

const float VBAO_EFFECT_RADIUS = VBAO_DEPTH_RANGE_SCALE_FACTOR * VBAO_DEFAULT_RADIUS * VBAO_DEFAULT_RADIUS_MULTIPLIER;
const float VBAO_FALLOFF_RANGE = VBAO_DEFAULT_FALLOFF_RANGE * VBAO_EFFECT_RADIUS;
const float VBAO_FALLOFF_FROM  = VBAO_EFFECT_RADIUS * (1.0f - VBAO_DEFAULT_FALLOFF_RANGE);

const float VBAO_FALLOFF_MUL = -1.0f / VBAO_FALLOFF_RANGE;
const float VBAO_FALLOFF_ADD = VBAO_FALLOFF_FROM / (VBAO_FALLOFF_RANGE) + 1.0f;

const float VBAO_DEFAULT_DEPTH_MIP_SAMPLING_OFFSET = 3.30f;

// Auto-Exposure Constants
const float HISTOGRAM_MIN_LUMINANCE               = 1.0f / 1024.0f;
const float HISTOGRAM_MIN_LOG_LUMINANCE           = -10.0f;
const float HISTOGRAM_MAX_LOG_LUMINANCE           = 16.0f;
const float HISTOGRAM_LOG_LUMINANCE_RANGE         = -HISTOGRAM_MIN_LOG_LUMINANCE + HISTOGRAM_MAX_LOG_LUMINANCE;
const float HISTOGRAM_INVERSE_LOG_LUMINANCE_RANGE = 1.0f / HISTOGRAM_LOG_LUMINANCE_RANGE;

#endif