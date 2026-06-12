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

#ifndef WIREFRAME_CONE_H
#define WIREFRAME_CONE_H

#include <vector>

#include "Types.h"
#include "Externals/GLM.h"

namespace Maths
{
    struct WireframeCone
    {
        WireframeCone(f32 base, f32 height, usize stacks, usize slices);

        std::vector<glm::vec3> vertices = {};
        std::vector<u32>       indices  = {};
    };
}

#endif
