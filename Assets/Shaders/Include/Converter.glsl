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

#ifndef CONVERTER_GLSL
#define CONVERTER_GLSL

#include "Constants.glsl"

vec4 UnpackRGBA8(uint data)
{
    const float ONE_BY_255 = 1.0f / 255.0f;

    return vec4
    (
        float(bitfieldExtract(data, 0,  8)) * ONE_BY_255,
        float(bitfieldExtract(data, 8,  8)) * ONE_BY_255,
        float(bitfieldExtract(data, 16, 8)) * ONE_BY_255,
        float(bitfieldExtract(data, 24, 8)) * ONE_BY_255
    );
}

vec2 GetSphericalMapUV(vec3 v)
{
    const vec2 INVERSE_CONSTANTS = vec2(INVERSE_TWO_PI, INVERSE_PI);

    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv     *= INVERSE_CONSTANTS;
    uv     += 0.5f;

    uv.y = 1.0f - uv.y;

    return uv;
}

#endif