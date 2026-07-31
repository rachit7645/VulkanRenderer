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

    Allocator::Allocator(usize size)
        : m_memory{std::make_unique<u8[]>(size)},
          m_size{size}
    {
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

        const usize alignedSize = Util::Align(bytes, alignment);

        const usize offset = m_offset.fetch_add(alignedSize, std::memory_order_relaxed);

        const usize newOffset = offset + alignedSize;

        if (newOffset > m_size)
        {
            Logger::Warning("Failed to allocate! [Bytes={}]\n", bytes);

            return nullptr;
        }

        return m_memory.get() + offset;
    }
}
