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

#include "Scratch.h"

#include "Util/Memory.h"
#include "Util/Log.h"

namespace Scratch
{
    static_assert(std::atomic<usize>::is_always_lock_free, "Really?");

    constexpr usize CACHE_LINE_SIZE = std::hardware_destructive_interference_size;

    Allocator::Allocator(usize size)
        : m_memory{static_cast<u8*>(Util::AlignedAlloc(size, CACHE_LINE_SIZE))},
          m_size{size}
    {
    }

    Allocator::~Allocator()
    {
        Util::AlignedFree(m_memory);
    }

    void Allocator::Reset()
    {
        bytesUsedBeforeReset = m_offset.load(std::memory_order_relaxed);

        peakMemoryUsage = std::max(peakMemoryUsage, bytesUsedBeforeReset);

        m_offset.store(0ull, std::memory_order_relaxed);
    }

    void* Allocator::Allocate(usize bytes, usize alignment)
    {
        if (!Util::IsPowerOfTwo(alignment))
        {
            return nullptr;
        }

        usize offset = m_offset.load(std::memory_order_relaxed);

        while (true)
        {
            const usize alignedOffset = Util::Align(offset, alignment);
            const usize newOffset     = alignedOffset + bytes;

            if (newOffset > m_size)
            {
                return nullptr;
            }

            const bool success = m_offset.compare_exchange_weak
            (
                offset,
                newOffset,
                std::memory_order_relaxed,
                std::memory_order_relaxed
            );

            if (success)
            {
                return m_memory + alignedOffset;
            }
        }
    }
}
