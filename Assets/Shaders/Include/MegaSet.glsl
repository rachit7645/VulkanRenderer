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

#ifndef MEGASET_GLSL
#define MEGASET_GLSL

#extension GL_EXT_nonuniform_qualifier : enable

// Mega set
layout(set = 0, binding = 0) uniform sampler          Samplers[];
layout(set = 0, binding = 1) uniform texture2D        Textures[];
layout(set = 0, binding = 1) uniform utexture2D       UTextures[];
layout(set = 0, binding = 1) uniform textureCube      Cubemaps[];
layout(set = 0, binding = 1) uniform texture2DArray   TextureArrays[];
layout(set = 0, binding = 1) uniform textureCubeArray CubemapArrays[];

layout(set = 0, binding = 2) uniform writeonly image2D  Images[];
layout(set = 0, binding = 2) uniform writeonly uimage2D UImages[];

#endif