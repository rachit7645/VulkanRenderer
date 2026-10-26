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

#ifndef UTIL_SPAN_H
#define UTIL_SPAN_H

#include <span>

#include "Util/Types.h"

namespace Util
{
    template <typename>
    struct IsSpanTrait : std::false_type{};

    template <typename T, std::size_t N>
    struct IsSpanTrait<std::span<T, N>> : std::true_type{};

    template <typename T>
    concept IsSpan = IsSpanTrait<std::remove_cvref_t<T>>::value;

    template <typename T>
    concept IsConstructibleToSpan = requires(T& t) { std::span(t); };

    template<typename T, usize N>
    std::span<const u8> ToBytes(const std::span<const T, N> source)
    {
        return std::span(reinterpret_cast<const u8*>(source.data()), source.size_bytes());
    }

    template <typename T>
    requires (!IsSpan<T> && IsConstructibleToSpan<T>)
    std::span<const u8> ToBytes(const T& source)
    {
        return Util::ToBytes(std::span(source));
    }

    template <typename T>
    requires (!IsSpan<T> && !IsConstructibleToSpan<T>)
    std::span<const u8> ToBytes(const T& source)
    {
        return Util::ToBytes(std::span<const T>(std::addressof(source), 1));
    }
}

#endif