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
const float FLOAT_MAX = 3.402823466e+38;

// GBuffer Constants
const float GBUFFER_HORIZON_FADE = 1.2f;

// IBL Constants
const uint IRRADIANCE_SAMPLE_COUNT = 1024u;

// Point Shadow Constants
const float POINT_SHADOW_BIAS = 0.0001f;

// Spot shadow constants
const float SPOT_SHADOW_MIN_BIAS = 0.0001f;
const float SPOT_SHADOW_MAX_BIAS = 0.001f;

// RT Shadow Constants
const float RT_SHADOW_MIN_BIAS        = 0.0005f;
const float RT_SHADOW_MAX_BIAS        = 0.005f;
const uint  RT_SHADOW_RAY_COUNT       = 1;
const float RT_SUN_ANGULAR_RADIUS     = radians(0.5f);
const float RT_COS_SUN_ANGULAR_RADIUS = cos(RT_SUN_ANGULAR_RADIUS);

// TAA Constants
const float TAA_DEFAULT_HISTORY_BLEND_RATE = 0.1f;
const float TAA_MIN_HISTORY_BLEND_RATE     = 0.015f;

// VBAO Constants
const uint  VBAO_SLICE_COUNT               = 3;
const uint  VBAO_SAMPLE_COUNT              = 3;
const uint  VBAO_SECTOR_COUNT              = 32;
const float VBAO_EFFECT_RADIUS             = 0.55f;
const float VBAO_DEPTH_MIP_SAMPLING_OFFSET = 3.15f;

// Auto-Exposure Constants
const float HISTOGRAM_MIN_LUMINANCE               = 1.0f / 1024.0f;
const float HISTOGRAM_MIN_LOG_LUMINANCE           = -10.0f;
const float HISTOGRAM_MAX_LOG_LUMINANCE           = 20.0f;
const float HISTOGRAM_LOG_LUMINANCE_RANGE         = -HISTOGRAM_MIN_LOG_LUMINANCE + HISTOGRAM_MAX_LOG_LUMINANCE;
const float HISTOGRAM_INVERSE_LOG_LUMINANCE_RANGE = 1.0f / HISTOGRAM_LOG_LUMINANCE_RANGE;
const float HISTOGRAM_LUMINANCE_EPSILON           = 0.005f;

#endif