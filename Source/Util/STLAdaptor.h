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

#ifndef STL_ADAPTOR_H
#define STL_ADAPTOR_H

#include <limits>
#include <new>
#include <memory>

#include "Util/Scratch.h"
#include "Util/Unused.h"

namespace STL
{
    template <typename T>
    class ScratchAllocator
    {
    public:
        using value_type = T;

        using propagate_on_container_copy_assignment = std::true_type;
        using propagate_on_container_move_assignment = std::true_type;
        using propagate_on_container_swap            = std::true_type;

        using is_always_equal = std::false_type;

        template <typename U>
        friend class ScratchAllocator;

        ScratchAllocator() noexcept = default;

        explicit ScratchAllocator(Scratch::Allocator* allocator)
            : m_allocator{allocator}
        {
        }

        // DO NOT MAKE THIS EXPLICIT!
        template <typename U>
        ScratchAllocator(const ScratchAllocator<U>& other) noexcept
            : m_allocator{other.m_allocator}
        {
        }

        template <typename U>
        bool operator==(const ScratchAllocator<U>& other) const noexcept
        {
            return m_allocator == other.m_allocator;
        }

        [[nodiscard]] T* allocate(usize count)
        {
            if (m_allocator == nullptr)
            {
                throw std::bad_alloc{};
            }

            if (count > std::numeric_limits<usize>::max() / sizeof(T))
            {
                throw std::bad_array_new_length{};
            }

            const usize size = sizeof(T) * count;

            T* pointer = static_cast<T*>(m_allocator->Allocate(size, alignof(T)));

            if (pointer == nullptr)
            {
                throw std::bad_alloc{};
            }

            return pointer;
        }

        void deallocate(ENGINE_UNUSED T* pointer) noexcept
        {
        }

        void deallocate(ENGINE_UNUSED T* pointer, ENGINE_UNUSED usize count) noexcept
        {
        }

        template <typename U, typename... Args>
        void construct(U* pointer, Args&&... args)
        {
            std::uninitialized_construct_using_allocator(pointer, *this, std::forward<Args>(args)...);
        }
    private:
        Scratch::Allocator* m_allocator = nullptr;
    };
}

#endif
