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
const float MIN_SPOT_SHADOW_BIAS = 0.000005f;
const float MAX_SPOT_SHADOW_BIAS = 0.00025f;

// RT Shadow Constants
const float RT_SHADOW_MIN_BIAS = 0.0005f;
const float RT_SHADOW_MAX_BIAS = 0.005f;

// TAA Constants
const float TAA_DEFAULT_HISTORY_BLEND_RATE = 0.1f;
const float TAA_MIN_HISTORY_BLEND_RATE     = 0.015f;

// GTAO Constants

// |---------|--------|---------|
// | Setting | Slices | Samples |
// |---------|--------|---------|
// | Low     |   1    |    2    |
// | Medium  |   2    |    2    |
// | High    |   3    |    3    |
// | Ultra   |   9    |    3    |
// |---------|--------|---------|
const uint GTAO_SLICE_COUNT  = 2;
const uint GTAO_SAMPLE_COUNT = 2;

const float GTAO_DEPTH_RANGE_SCALE_FACTOR  = 0.75f;
const float GTAO_DEFAULT_RADIUS            = 0.5f;
const float GTAO_DEFAULT_RADIUS_MULTIPLIER = 1.457f;
const float GTAO_DEFAULT_FALLOFF_RANGE     = 0.615f;

const float GTAO_EFFECT_RADIUS = GTAO_DEPTH_RANGE_SCALE_FACTOR * GTAO_DEFAULT_RADIUS * GTAO_DEFAULT_RADIUS_MULTIPLIER;
const float GTAO_FALLOFF_RANGE = GTAO_DEFAULT_FALLOFF_RANGE * GTAO_EFFECT_RADIUS;
const float GTAO_FALLOFF_FROM  = GTAO_EFFECT_RADIUS * (1.0f - GTAO_DEFAULT_FALLOFF_RANGE);

const float GTAO_FALLOFF_MUL = -1.0f / GTAO_FALLOFF_RANGE;
const float GTAO_FALLOFF_ADD = GTAO_FALLOFF_FROM / (GTAO_FALLOFF_RANGE) + 1.0f;

const float GTAO_DEFAULT_DEPTH_MIP_SAMPLING_OFFSET = 3.30f;

const uint GTAO_HILBERT_LEVEL = 6;
const uint GTAO_HILBERT_WIDTH = 1u << GTAO_HILBERT_LEVEL;

#endif