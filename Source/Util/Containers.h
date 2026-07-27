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

#ifndef CONTAINERS_H
#define CONTAINERS_H

#include "Stack.h"
#include "STLAdaptor.h"

#include "Externals/UnorderedDense.h"

namespace Stack
{
    template <typename T>
    using Vector = std::vector<T, STL::StackAllocator<T>>;

    template <typename Key, typename T>
    using Map = ankerl::unordered_dense::map
    <
        Key,
        T,
        ankerl::unordered_dense::hash<Key>,
        std::equal_to<Key>,
        STL::StackAllocator<std::pair<Key, T>>
    >;

    template <typename T>
    Stack::Vector<T> CreateVector(Stack::Allocator& allocator)
    {
        return Stack::Vector<T>(STL::StackAllocator<T>(&allocator));
    }

    template <typename Key, typename T>
    Stack::Map<Key, T> CreateMap(Stack::Allocator& allocator)
    {
        return Stack::Map<Key, T>(STL::StackAllocator<std::pair<Key, T>>(&allocator));
    }
}

#endif
