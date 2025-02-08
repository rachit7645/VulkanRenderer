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

// Mega set
layout(set = 0, binding = 0) uniform sampler          samplers[];
layout(set = 0, binding = 1) uniform texture2D        textures[];
layout(set = 0, binding = 2) uniform textureCube      cubemaps[];
layout(set = 0, binding = 3) uniform texture2DArray   textureArrays[];
layout(set = 0, binding = 4) uniform textureCubeArray cubemapArrays[];

#endif