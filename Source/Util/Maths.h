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

#ifndef MATHS_H
#define MATHS_H

#include "Types.h"
#include "Externals/GLM.h"

namespace Maths
{
    glm::mat4 CreateTransformMatrix(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale);
    glm::mat4 CreateProjectionReverseZ(f32 FOV, f32 aspectRatio, f32 nearPlane, f32 farPlane);
    glm::mat4 CreateInfiniteProjectionReverseZ(f32 FOV, f32 aspectRatio, f32 nearPlane);
    glm::mat3 CreateNormalMatrix(const glm::mat4& transform);

    constexpr f32 Halton(usize index, usize base)
    {
        f64 result = 0.0;
        f64 f      = 1.0 / static_cast<f64>(base);

        index += 1;

        while (index > 0)
        {
            result += f * static_cast<f64>(index % base);

            index /= base;
            f     /= static_cast<f64>(base);
        }

        return static_cast<f32>(result);
    }

    template<usize N>
    constexpr auto GenerateHaltonSequence()
    {
        std::array<glm::vec2, N> sequence = {};

        for (usize i = 0; i < N; ++i)
        {
            sequence[i] = {Halton(i, 2), Halton(i, 3)};
        }

        return sequence;
    }

    template<u32 HilbertWidth>
    constexpr u32 HilbertIndex(u32 positionX, u32 positionY)
    {
        u32 index = 0;
        
        for (u32 currentLevel = HilbertWidth / 2; currentLevel > 0; currentLevel /= 2)
        {
            const u32 regionX = (positionX & currentLevel) > 0;
            const u32 regionY = (positionY & currentLevel) > 0;

            index += currentLevel * currentLevel * ((3 * regionX) ^ regionY);

            if (regionY == 0)
            {
                if (regionX == 1)
                {
                    positionX = (HilbertWidth - 1) - positionX;
                    positionY = (HilbertWidth - 1) - positionY;
                }

                const u32 temp = positionX;
                
                positionX = positionY;
                positionY = temp;
            }
        }
        
        return index;
    }

    template<u32 HilbertLevel, typename OutputType = u16, u32 HilbertWidth = 1u << HilbertLevel>
    constexpr auto GenerateHilbertSequence()
    {
        std::array<OutputType, HilbertWidth * HilbertWidth> sequence = {};

        for (u32 i = 0; i < HilbertWidth; ++i)
        {
            for (u32 j = 0; j < HilbertWidth; ++j)
            {
                sequence[i * HilbertWidth + j] = static_cast<OutputType>(HilbertIndex<HilbertWidth>(i, j));
            }
        }

        return sequence;
    }
}

#endif