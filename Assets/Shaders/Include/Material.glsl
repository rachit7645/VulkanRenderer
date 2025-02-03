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

// Material utils

#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

#define MAT_ALBEDO_ID     mesh.textureIDs.x
#define MAT_NORMAL_ID     mesh.textureIDs.y
#define MAT_AO_RGH_MTL_ID mesh.textureIDs.z

// Safe version, use if geometry with bad geometry
vec3 Orthogonalize(vec3 T, vec3 N)
{
    vec3  TPerpendicular       = T - dot(T, N) * N;
    float lengthTPerpendicular = length(TPerpendicular);

    if (lengthTPerpendicular < 1e-5)
    {
        vec3 reference = abs(N.z) < 0.999f ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
        TPerpendicular = normalize(cross(N, reference));
    } else
    {
        TPerpendicular /= lengthTPerpendicular;
    }

    return TPerpendicular;
}

vec3 GetNormalFromMap(vec3 normal, mat3 TBN)
{
    normal = normal * 2.0f - 1.0f;
    return normalize(TBN * normal);
}

#endif