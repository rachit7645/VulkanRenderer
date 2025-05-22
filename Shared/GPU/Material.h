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

#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

#include "GLSL.h"

#ifdef __cplusplus
#include "Util/Enum.h"
#endif

GLSL_NAMESPACE_BEGIN(GPU)

GLSL_ENUM_CLASS_BEGIN(MaterialFlags, u32)
    GLSL_ENUM_CLASS_ENTRY(MaterialFlags, u32, None,        0u << 0u)
    GLSL_ENUM_CLASS_ENTRY(MaterialFlags, u32, DoubleSided, 1u << 1u)
    GLSL_ENUM_CLASS_ENTRY(MaterialFlags, u32, AlphaMasked, 1u << 2u)
GLSL_ENUM_CLASS_END

struct Material
{
    u32 albedo;
    u32 normal;
    u32 aoRghMtl;

    GLSL_VEC4 albedoFactor;
    f32       roughnessFactor;
    f32       metallicFactor;

    f32 alphaCutOff;

    GLSL_ENUM_CLASS_NAME(MaterialFlags, u32) flags;

    #ifdef __cplusplus
    [[nodiscard]] bool IsAlphaMasked() const
    {
        return (flags & MaterialFlags::AlphaMasked) == MaterialFlags::AlphaMasked;
    }

    [[nodiscard]] bool IsDoubleSided() const
    {
        return (flags & MaterialFlags::DoubleSided) == MaterialFlags::DoubleSided;
    }
    #endif
};

#ifndef __cplusplus

bool Material_IsDoubleSided(u32 flags)
{
    return (flags & MaterialFlags_DoubleSided) == MaterialFlags_DoubleSided;
}

bool Material_IsAlphaMasked(u32 flags)
{
    return (flags & MaterialFlags_AlphaMasked) == MaterialFlags_AlphaMasked;
}

bool Material_IsDoubleSided(Material material)
{
    return Material_IsDoubleSided(material.flags);
}

bool Material_IsAlphaMasked(Material material)
{
    return Material_IsAlphaMasked(material.flags);
}

// Safe version, use with bad geometry
vec3 Orthogonalize(vec3 T, vec3 N)
{
    vec3  TPerpendicular       = T - dot(T, N) * N;
    float lengthTPerpendicular = length(TPerpendicular);

    if (lengthTPerpendicular < 1e-5f)
    {
        vec3 reference = abs(N.z) < 0.999f ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
        TPerpendicular = normalize(cross(N, reference));
    }
    else
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

GLSL_NAMESPACE_END

#endif