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

#ifndef DESCRIPTOR_ALLOCATOR_H
#define DESCRIPTOR_ALLOCATOR_H

#include <queue>

#include "Util/Types.h"

namespace Vk
{
    using DescriptorID = u32;

    class DescriptorAllocator
    {
    public:
        DescriptorAllocator() = default;
        explicit DescriptorAllocator(u32 maxDescriptorCount);

        [[nodiscard]] Vk::DescriptorID Allocate();
        void Free(Vk::DescriptorID id);

        [[nodiscard]] u32 GetAllocatedCount() const;
        [[nodiscard]] u32 GetFreeSlotCount()  const;
        [[nodiscard]] u32 GetUsedCount()      const;
        [[nodiscard]] u32 GetMaxCount()       const;
    private:
        u32 m_maxDescriptorCount = 0;

        Vk::DescriptorID             m_currentID = 0;
        std::queue<Vk::DescriptorID> m_freeIDs   = {};
    };
}

#endif
