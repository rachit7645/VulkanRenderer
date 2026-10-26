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

#include "WireframeSphere.h"

namespace Maths
{
    WireframeSphere::WireframeSphere(usize stacks, usize slices)
    {
        const usize vertexCount = stacks * slices;
        const usize indexCount  = 2 * slices * (2 * stacks - 1);

        vertices.resize(vertexCount);
        indices.resize(indexCount);

        for (usize stack = 0; stack < stacks; ++stack)
        {
            const f32 theta = static_cast<f32>(std::numbers::pi) * static_cast<f32>(stack + 1) / static_cast<f32>(stacks + 1);

            const f32 sinTheta = std::sinf(theta);
            const f32 cosTheta = std::cosf(theta);

            for (usize slice = 0; slice < slices; ++slice)
            {
                const f32 phi = 2.0f * static_cast<f32>(std::numbers::pi) * static_cast<f32>(slice) / static_cast<f32>(slices);

                const usize vertexIndex = stack * slices + slice;

                constexpr f32 RADIUS = 1.0f;

                vertices[vertexIndex] = glm::vec3
                (
                    RADIUS * sinTheta * std::cosf(phi),
                    RADIUS * cosTheta,
                    RADIUS * sinTheta * std::sinf(phi)
                );
            }
        }

        auto GetVertexIndex = [slices] (usize stack, usize slice) -> u32
        {
            return static_cast<u32>(stack * slices + (slice % slices));
        };

        usize index = 0;

        for (usize slice = 0; slice < slices; ++slice)
        {
            for (usize stack = 0; stack < stacks - 1; ++stack)
            {
                indices[index++] = GetVertexIndex(stack,     slice);
                indices[index++] = GetVertexIndex(stack + 1, slice);
            }
        }

        for (usize stack = 0; stack < stacks; ++stack)
        {
            for (usize slice = 0; slice < slices; ++slice)
            {
                indices[index++] = GetVertexIndex(stack, slice);
                indices[index++] = GetVertexIndex(stack, slice + 1);
            }
        }
    }
}