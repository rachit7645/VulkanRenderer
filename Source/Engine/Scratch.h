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

#ifndef SCRATCH_ALLOCATOR_H
#define SCRATCH_ALLOCATOR_H

#include <atomic>

#include "Util/Types.h"

namespace Scratch
{
    class Allocator
    {
    public:
        explicit Allocator(usize size);

        ~Allocator();

        Allocator(const Allocator&) = delete;
        Allocator& operator=(const Allocator&) = delete;

        Allocator(Allocator&&) = delete;
        Allocator& operator=(Allocator&&) = delete;

        void Reset();

        [[nodiscard]] void* Allocate(usize bytes, usize alignment);

        usize bytesUsedBeforeReset = 0;
        usize peakMemoryUsage      = 0;
    private:
        u8*              m_begin  = nullptr;
        u8*              m_end    = nullptr;
        std::atomic<u8*> m_offset = nullptr;
    };
}

#endif
