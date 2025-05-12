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

#ifndef MATH_GLSL
#define MATH_GLSL

// I'm too damn lazy to replace all calls to saturate
// Fuck you Microsoft
#define saturate(x) clamp(x, 0.0f, 1.0f)

float max3(vec3 x)
{
    return max(x.r, max(x.g, x.b));
}

float rcp(float x)
{
    return 1.0f / x;
}

float pow5(float x)
{
    float x2 = x * x;
    float x5 = x * (x2 * x2);

    return x5;
}

float FastTanArcCos(float x)
{
    // tan(acos(x)) = sqrt(1 - x^2) / x

    float numerator   = max(1.0f - (x * x), 0.0f);
    float denominator = max(x, 0.00001f);

    return sqrt(numerator) / denominator;
}

float UnsafeFastTanArcCos(float x)
{
    // tan(acos(x)) = sqrt(1 - x^2) / x

    float numerator   = 1.0f - (x * x);
    float denominator = x;

    return sqrt(numerator) / denominator;
}

// http://h14s.p5r.org/2012/09/0x5f3759df.html, [Drobot2014a] Low Level Optimizations for GCN, https://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf slide 63
float FastSqrt(float x)
{
    return intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(x) >> 1));
}

// https://seblagarde.wordpress.com/2014/12/01/inverse-trigonometric-functions-gpu-optimization-for-amd-gcn-architecture/
// Input:  [-1, 1]
// Output: [0, PI]
float FastArcCos(float inX)
{
    const float FAST_ACOS_PI      = 3.141593f;
    const float FAST_ACOS_HALF_PI = 1.570796f;

    float x   = abs(inX);
    float res = -0.156583f * x + FAST_ACOS_HALF_PI;
    res      *= FastSqrt(1.0f - x);

    return (inX >= 0) ? res : FAST_ACOS_PI - res;
}

#endif