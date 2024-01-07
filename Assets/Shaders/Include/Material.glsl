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

// Material utils

#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

// Defines
#define ALBEDO(material)     material[0]
#define NORMAL(material)     material[1]
#define AO_RGH_MTL(material) material[2]

vec3 GetNormalFromMap(vec3 normal, mat3 TBN)
{
    normal = normal * 2.0f - 1.0f;
    return normalize(TBN * normal);
}

#endif