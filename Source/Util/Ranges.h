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

#ifndef RANGES_H
#define RANGES_H

#include <vector>
#include <array>
#include <ranges>

#include "Types.h"

namespace Util
{
    template <typename T, usize GroupSize>
    auto SplitVector(const std::vector<T>& originalVector) -> std::array<std::vector<T>, GroupSize>
    {
        std::array<std::vector<T>, GroupSize> result;

        const usize groupSize = originalVector.size() / GroupSize;
        const usize remainder = originalVector.size() % GroupSize;

        for (usize i = 0, start = 0, end = 0; i < GroupSize; ++i)
        {
            // Adjust the end index according to the remainder
            end = start + groupSize + (i < remainder ? 1 : 0);

            result[i].insert
            (
                result[i].end(),
                originalVector.begin() + start,
                originalVector.begin() + end
            );

            start = end;
        }

        return result;
    }
}

#endif
