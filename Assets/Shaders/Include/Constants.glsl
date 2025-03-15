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

// Config
#define CSM_ENABLE_SMOOTH_TRANSITION 1
#define CSM_ENABLE_PCF               1

// Math constants
const float PI = 3.1415926535897932384626433832795;

// Float bounds
const float FLOAT_MIN = 1.175494351e-38;
const float FLOAT_MAX = 3.402823466e+38;

// Gamma correction constants
const float GAMMA_FACTOR     = 2.2f;
const float INV_GAMMA_FACTOR = 1.0f / GAMMA_FACTOR;

// IBL Constants
const float CONVOLUTION_SAMPLE_DELTA = 0.025f;
const uint  BRDF_LUT_SAMPLE_COUNT    = 512u;
const float MAX_REFLECTION_LOD       = 5.0f;

// Shadow Constants
const float SHADOW_MIN_BIAS      = 0.005f;
const float SHADOW_MAX_BIAS      = 0.05f;
const float SHADOW_BIAS_MODIFIER = 0.35f;
const int   SHADOW_PCF_RANGE     = 1; // p = 2 * r + 1 for a p * p PCF filter
const float SHADOW_BLEND_RANGE   = 25.0f;

// Point Shadow Constants
const uint  POINT_SHADOW_NUM_SAMPLES = 20;
const float POINT_SHADOW_BIAS        = 0.15f;

// Spot shadow constants
const float MIN_SPOT_SHADOW_BIAS = 0.000005f;
const float MAX_SPOT_SHADOW_BIAS = 0.00025f;

// RT Shadow Constants
const float RT_SHADOW_MIN_BIAS = 0.0005f;
const float RT_SHADOW_MAX_BIAS = 0.005f;

// TAA Constants
const float TAA_DEFAULT_HISTORY_BLEND_RATE = 0.1f;
const float TAA_MIN_HISTORY_BLEND_RATE     = 0.015f;

// Gaussian blur constants
const int   GAUSSIAN_FILTER_SIZE                           = 2; // p = 2 * r + 1
const float GAUSSIAN_WEIGHTS[2 * GAUSSIAN_FILTER_SIZE + 1] = float[5](0.06136, 0.24477, 0.38774, 0.24477, 0.06136);

// XeGTAO Constants
const float XE_GTAO_OCCLUSION_TERM_SCALE = 1.5f;
const float XE_GTAO_LEAK_THRESHOLD       = 2.5f;
const float XE_GTAO_LEAK_STRENGTH        = 0.5f;
#endif