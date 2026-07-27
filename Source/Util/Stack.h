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

#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <atomic>

#include "Util/Types.h"

namespace Stack
{
    class Allocator
    {
    public:
        explicit Allocator(usize size);

        ~Allocator() = default;

        Allocator(const Allocator&) = delete;
        Allocator& operator=(const Allocator&) = delete;

        Allocator(Allocator&&) = delete;
        Allocator& operator=(Allocator&&) = delete;

        void Reset();

        [[nodiscard]] void* Allocate(usize bytes, usize alignment);
    private:
        std::unique_ptr<u8[]> m_memory = nullptr;
        usize                 m_size   = 0;
        std::atomic<usize>    m_offset = 0;
    };
}

#endif
