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

vec4 UnpackRGBA8(uint data)
{
    const float ONE_BY_255 = 1.0f / 255.0f;

    return vec4
    (
        float((data >> 0)  & 0xFF) * ONE_BY_255,
        float((data >> 8)  & 0xFF) * ONE_BY_255,
        float((data >> 16) & 0xFF) * ONE_BY_255,
        float((data >> 24) & 0xFF) * ONE_BY_255
    );
}

vec2 GetSphericalMapUV(vec3 v)
{
    const vec2 INVERSE_ATAN = vec2(0.1591f, 0.3183f);

    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv     *= INVERSE_ATAN;
    uv     += 0.5f;

    return uv;
}

#endif