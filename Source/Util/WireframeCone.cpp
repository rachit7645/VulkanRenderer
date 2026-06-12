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

#include "WireframeCone.h"

#include <numbers>

namespace Maths
{
    WireframeCone::WireframeCone(f32 base, f32 height, usize stacks, usize slices)
    {
        vertices.emplace_back(0.0f, height, 0.0f);
        
        for (usize i = 1; i <= stacks; ++i)
        {
            const f32 t      = static_cast<f32>(i) / static_cast<f32>(stacks);
            const f32 y      = height - t * height;
            const f32 radius = t * base;

            for (usize j = 0; j < slices; ++j)
            {
                const f32 angle = 2.0f * static_cast<f32>(std::numbers::pi) * static_cast<f32>(j) / static_cast<f32>(slices);
                
                vertices.emplace_back
                (
                    radius * glm::cos(angle),
                    y,
                    radius * glm::sin(angle)
                );
            }
        }
        
        vertices.emplace_back(0.0f, 0.0f, 0.0f);

        const u32 baseCenterIndex = static_cast<u32>(vertices.size() - 1);
        
        for (usize j = 0; j < slices; ++j)
        {
            constexpr u32 apexIndex = 0;
            
            const u32 ringVertex = 1 + static_cast<u32>(j);
            
            indices.emplace_back(apexIndex);
            indices.emplace_back(ringVertex);
        }
        
        for (usize i = 0; i < stacks; ++i)
        {
            const u32 ringStart = 1 + static_cast<u32>(i) * static_cast<u32>(slices);

            for (usize j = 0; j < slices; ++j)
            {
                const u32 current = ringStart + static_cast<u32>(j);
                const u32 next    = ringStart + static_cast<u32>((j + 1) % slices);

                indices.emplace_back(current);
                indices.emplace_back(next);
            }

            if (i + 1 < stacks)
            {
                const u32 nextRingStart = ringStart + static_cast<u32>(slices);

                for (usize j = 0; j < slices; ++j)
                {
                    indices.emplace_back(ringStart     + static_cast<u32>(j));
                    indices.emplace_back(nextRingStart + static_cast<u32>(j));
                }
            }
        }

        const u32 baseRingStart = 1 + static_cast<u32>(stacks - 1) * static_cast<u32>(slices);

        for (usize j = 0; j < slices; ++j)
        {
            indices.emplace_back(baseCenterIndex);
            indices.emplace_back(baseRingStart + static_cast<u32>(j));
        }
    }
}
