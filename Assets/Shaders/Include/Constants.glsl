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
const float PI = 3.14159265359;

// Gamma correction constants
const float GAMMA_FACTOR     = 2.2f;
const float INV_GAMMA_FACTOR = 1.0f / GAMMA_FACTOR;

// IBL Constants
const float CONVOLUTION_SAMPLE_DELTA = 0.025f;
const uint  PREFILER_SAMPLE_COUNT    = 1024u;
const uint  BRDF_LUT_SAMPLE_COUNT    = 1024u;
const float MAX_REFLECTION_LOD       = 5.0f;

// Shadow Constants
const float SHADOW_MIN_BIAS      = 0.005f;
const float SHADOW_MAX_BIAS      = 0.05f;
const float SHADOW_BIAS_MODIFIER = 0.35f;
const int   SHADOW_PCF_RANGE     = 1; // p = 2 * r + 1 for a p * p PCF filter
const float SHADOW_BLEND_RANGE   = 25.0f;

// Point Shadow Constants
const int   POINT_SHADOW_NUM_SAMPLES = 20;
const float POINT_SHADOW_BIAS        = 0.15f;

#endif