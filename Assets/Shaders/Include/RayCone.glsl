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

#ifndef RAY_CONE_GLSL
#define RAY_CONE_GLSL

// Texture Level of Detail Strategies for Real-Time Ray Tracing : Ray Tracing Gems, Chapter 20

struct RayCone
{
    float width;
    float spreadAngle;
};

RayCone PropagateRayCone(RayCone cone, float surfaceSpreadAngle, float hitT)
{
    RayCone newCone;

    newCone.width       = cone.spreadAngle * hitT + cone.width;
    newCone.spreadAngle = cone.spreadAngle + surfaceSpreadAngle;

    return newCone;
}

RayCone ComputeRayConeFromGBuffer(float pixelSpreadAngle, float surfaceSpreadAngle, float gBufferDistance)
{
    RayCone cone;

    cone.width       = 0.0f;
    cone.spreadAngle = pixelSpreadAngle;

    return PropagateRayCone(cone, surfaceSpreadAngle, gBufferDistance);
}

uint PackRayCone(RayCone cone)
{
    return packHalf2x16(vec2(cone.width, cone.spreadAngle));
}

RayCone UnpackRayCone(uint packed)
{
    vec2 unpacked = unpackHalf2x16(packed);

    RayCone cone;

    cone.width       = unpacked.x;
    cone.spreadAngle = unpacked.y;

    return cone;
}

#endif