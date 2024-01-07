/*
 *   Copyright 2023 - 2024 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

// Gamma correction utils

#ifndef GAMMA_CORRECT_GLSL
#define GAMMA_CORRECT_GLSL

#include "Constants.glsl"

vec3 GammaCorrect(vec3 color)
{
    return pow(color, vec3(GAMMA_FACTOR));
}

vec3 ToLinear(vec3 color)
{
    return pow(color, vec3(INV_GAMMA_FACTOR));
}

#endif