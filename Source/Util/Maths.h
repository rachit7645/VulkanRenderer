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

#ifndef MATHS_H
#define MATHS_H

#include "Types.h"
#include "Externals/GLM.h"

namespace Maths
{
    [[nodiscard]] glm::mat4 TransformMatrix(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale);
    [[nodiscard]] glm::mat4 ProjectionReverseZ(f32 FOV, f32 aspectRatio, f32 nearPlane, f32 farPlane);
    [[nodiscard]] glm::mat4 InfiniteProjectionReverseZ(f32 FOV, f32 aspectRatio, f32 nearPlane);
    [[nodiscard]] glm::mat3 NormalMatrix(const glm::mat4& transform);
    [[nodiscard]] glm::vec3 SafeCross(const glm::vec3& A, const glm::vec3& B, const glm::vec3& fallback, f32 epsilon);
    [[nodiscard]] glm::vec2 PackOctahedron(const glm::vec3& vector);

    template<typename T>
    [[nodiscard]] constexpr T Max2(const glm::vec<2, T>& vector)
    {
        return glm::max(vector.x, vector.y);
    }

    template<typename T>
    [[nodiscard]] constexpr T ExponentialDecay(T current, T target, f32 rate, f32 dt)
    {
        return target + (current - target) * std::exp(-rate * dt);
    }

    template<>
    [[nodiscard]] constexpr glm::quat ExponentialDecay(glm::quat current, glm::quat target, f32 rate, f32 dt)
    {
        // Ensure we go through the shortest arc
        if (glm::dot(current, target) < 0.0f)
        {
            target = -target;
        }

        return glm::slerp(current, target, 1.0f - std::exp(-rate * dt));
    }

    [[nodiscard]] constexpr f32 Halton(usize index, usize base)
    {
        f32 result = 0.0f;
        f32 f      = 1.0f / static_cast<f32>(base);

        index += 1;

        while (index > 0)
        {
            result += f * static_cast<f32>(index % base);

            index /= base;
            f     /= static_cast<f32>(base);
        }

        return result;
    }

    template<u32 HilbertWidth>
    [[nodiscard]] consteval u32 HilbertIndex(u32 positionX, u32 positionY)
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

    template<u32 HilbertLevel>
    [[nodiscard]] consteval auto GenerateHilbertSequence()
    {
        constexpr u32 HILBERT_WIDTH = 1u << HilbertLevel;

        std::array<u16, static_cast<usize>(HILBERT_WIDTH * HILBERT_WIDTH)> sequence = {};

        for (u32 i = 0; i < HILBERT_WIDTH; ++i)
        {
            for (u32 j = 0; j < HILBERT_WIDTH; ++j)
            {
                sequence[i * HILBERT_WIDTH + j] = static_cast<u16>(HilbertIndex<HILBERT_WIDTH>(i, j));
            }
        }

        return sequence;
    }
}

#endif