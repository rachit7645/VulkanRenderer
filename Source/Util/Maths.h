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

#include "Util.h"
#include "Externals/GLM.h"

namespace Maths
{
    glm::mat4 CreateTransformMatrix(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale);
    glm::mat4 CreateProjectionReverseZ(f32 FOV, f32 aspectRatio, f32 nearPlane, f32 farPlane);
    glm::mat3 CreateNormalMatrix(const glm::mat4& transform);

    template<typename T>
    constexpr T Lerp(T a, T b, T factor)
    {
        return a + factor * (b - a);
    }

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
    constexpr std::array<glm::vec2, N> GenerateHaltonSequence()
    {
        std::array<glm::vec2, N> sequence = {};

        for (usize i = 0; i < N; ++i)
        {
            sequence[i] = {Halton(i, 2), Halton(i, 3)};
        }

        return sequence;
    }
}

#endif