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

#include "DescriptorAllocator.h"

#include "Util/Log.h"

namespace Vk
{
    DescriptorAllocator::DescriptorAllocator(u32 maxDescriptorCount)
        : m_maxDescriptorCount(maxDescriptorCount)
    {
    }

    u32 DescriptorAllocator::Allocate()
    {
        u32 id = 0;

        if (!m_freeIDs.empty())
        {
            id = m_freeIDs.front();
            m_freeIDs.pop();
        }
        else
        {
            id = m_currentID++;
        }

        if (id >= m_maxDescriptorCount)
        {
            Logger::Error("{}\n", "Failed to allocate descriptor!");
        }

        return id;
    }

    void DescriptorAllocator::Free(u32 id)
    {
        m_freeIDs.push(id);
    }

    u32 DescriptorAllocator::GetAllocatedCount() const
    {
        return m_currentID;
    }

    u32 DescriptorAllocator::GetFreeSlotCount() const
    {
        return m_freeIDs.size();
    }

    u32 DescriptorAllocator::GetUsedCount() const
    {
        return GetAllocatedCount() - GetFreeSlotCount();
    }

    u32 DescriptorAllocator::GetMaxCount() const
    {
        return m_maxDescriptorCount;
    }
}
