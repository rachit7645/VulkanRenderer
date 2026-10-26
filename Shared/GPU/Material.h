/*
 * Copyright (c) 2023 - 2026 Rachit
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

GLSL_ENUM_CLASS_BEGIN(MaterialFlags, u16)
    GLSL_ENUM_CLASS_ENTRY(MaterialFlags, u16, None,        0)
    GLSL_ENUM_CLASS_ENTRY(MaterialFlags, u16, DoubleSided, 1u << 0u)
    GLSL_ENUM_CLASS_ENTRY(MaterialFlags, u16, AlphaMasked, 1u << 1u)
GLSL_ENUM_CLASS_END

struct Material
{
    u32 albedoID;
    u32 normalID;
    u32 aoRghMtlID;
    u32 emissiveID;

    GLSL_VEC4 albedoFactor;
    f32       roughnessFactor;
    f32       metallicFactor;
    GLSL_VEC3 emissiveFactor;
    f32       emissiveStrength;

    f32 alphaCutOff;

    f32 ior;

    u16 packedUVIDs;

    GLSL_ENUM_CLASS_NAME(MaterialFlags, u16) flags;
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

vec3 GetNormalFromMap(vec3 normal, mat3 TBN)
{
    normal = normal * 2.0f - 1.0f;

    return normalize(TBN * normal);
}

uint GetAlbedoUVID(uint packedUVIDs)
{
    return bitfieldExtract(packedUVIDs, 0, 4);
}

uint GetNormalUVID(uint packedUVIDs)
{
    return bitfieldExtract(packedUVIDs, 4, 4);
}

uint GetAORghMtlUVID(uint packedUVIDs)
{
    return bitfieldExtract(packedUVIDs, 8, 4);
}

uint GetEmissiveUVID(uint packedUVIDs)
{
    return bitfieldExtract(packedUVIDs, 12, 4);
}

#endif

GLSL_NAMESPACE_END

#endif