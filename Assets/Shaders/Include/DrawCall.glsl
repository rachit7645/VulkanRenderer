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

#ifndef DRAW_CALL_GLSL
#define DRAW_CALL_GLSL

struct DrawCall
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer DrawCallBuffer
{
    uint     count;
    DrawCall drawCalls[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer MeshIndexBuffer
{
    uint indices[];
};

#endif