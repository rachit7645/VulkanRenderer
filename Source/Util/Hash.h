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

#ifndef HASH_H
#define HASH_H

#include <utility>

#include "Util/Types.h"

namespace Util
{
    template <typename T>
    [[nodiscard]] constexpr usize HashCombine(usize seed, const T& value)
    {
        seed ^= std::hash<T>{}(value) + 0x9E3779B97F4A7C15ull + (seed << 6) + (seed >> 2);

        return seed;
    }
}

#endif
