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

#ifndef RANDOM_H
#define RANDOM_H

#include <random>

namespace Util
{
    /// @brief Deterministic random number between a range
    /// @tparam T Number type
    /// @param min Range minimum
    /// @param max Range maximum
    /// @returns Random number ∈ [min, max)
    template<typename T>
    [[nodiscard]] T RandRange(T min, T max)
    {
        static thread_local std::mt19937_64 generator(777);
        std::uniform_real_distribution<T> distributer(min, max);
        return distributer(generator);
    }

    /// @brief More random number between a range
    /// @tparam T Number type
    /// @param min Range minimum
    /// @param max Range maximum
    /// @returns Random number ∈ [min, max)
    template<typename T>
    [[nodiscard]] T TrueRandRange(T min, T max)
    {
        std::random_device rd;
        std::seed_seq ss{ rd(), rd() };
        static thread_local std::mt19937_64 generator(ss);
        std::uniform_real_distribution<T> distributer(min, max);
        return distributer(generator);
    }
}

#endif