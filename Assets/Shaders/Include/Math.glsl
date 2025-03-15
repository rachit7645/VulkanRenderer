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

float TanArcCos(float x)
{
    // tan(acos(x)) = sqrt(1 - x^2) / x

    float numerator   = max(1.0f - (x * x), 0.0f);
    float denominator = max(x, 0.00001f);

    return sqrt(numerator) / denominator;
}

float max3(vec3 x)
{
    return max(x.r, max(x.g, x.b));
}

float rcp(float x)
{
    return 1.0f / x;
}

#endif