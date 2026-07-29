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

#include "Scratch.h"
#include "STLAdaptor.h"

#include "Externals/UnorderedDense.h"
#include "Util/String.h"

namespace Scratch
{
    using String = std::basic_string<char, std::char_traits<char>, STL::ScratchAllocator<char>>;

    template <typename T>
    using Vector = std::vector<T, STL::ScratchAllocator<T>>;

    template <typename T>
    using List = std::list<T, STL::ScratchAllocator<T>>;

    template <typename Key, typename T>
    using Map = ankerl::unordered_dense::map
    <
        Key,
        T,
        ankerl::unordered_dense::hash<Key>,
        std::equal_to<Key>,
        STL::ScratchAllocator<std::pair<Key, T>>
    >;

    template<typename Key>
    struct SetTraits
    {
        using Type = ankerl::unordered_dense::set
        <
            Key,
            ankerl::unordered_dense::hash<Key>,
            std::equal_to<Key>,
            STL::ScratchAllocator<Key>
        >;
    };

    template<>
    struct SetTraits<Scratch::String>
    {
        using Type = ankerl::unordered_dense::set
        <
            Scratch::String,
            Util::StringHash,
            Util::StringEqual,
            STL::ScratchAllocator<Scratch::String>
        >;
    };

    template<typename Key>
    using Set = SetTraits<Key>::Type;

    Scratch::String CreateString(Scratch::Allocator& allocator);

    template <typename T>
    Scratch::Vector<T> CreateVector(Scratch::Allocator& allocator)
    {
        return Scratch::Vector<T>(STL::ScratchAllocator<T>(&allocator));
    }

    template <typename T>
    Scratch::List<T> CreateList(Scratch::Allocator& allocator)
    {
        return Scratch::List<T>(STL::ScratchAllocator<T>(&allocator));
    }

    template <typename Key, typename T>
    Scratch::Map<Key, T> CreateMap(Scratch::Allocator& allocator)
    {
        return Scratch::Map<Key, T>(STL::ScratchAllocator<std::pair<Key, T>>(&allocator));
    }

    template <typename T>
    Scratch::Set<T> CreateSet(Scratch::Allocator& allocator)
    {
        return Scratch::Set<T>(STL::ScratchAllocator<T>(&allocator));
    }
}

#endif
