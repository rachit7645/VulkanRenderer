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

#ifndef BLOCK_ALLOCATOR_H
#define BLOCK_ALLOCATOR_H

#include <set>
#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "Util/DeletionQueue.h"

namespace Vk
{
    class BlockAllocator
    {
    public:
        struct Block
        {
            bool operator==(const Block& other) const noexcept;
            bool operator< (const Block& other) const noexcept;

            VkDeviceSize offset = 0;
            VkDeviceSize size   = 0;
        };

        BlockAllocator() = default;

        BlockAllocator
        (
            VkBufferUsageFlags usage,
            VkPipelineStageFlags2 stageMask,
            VkAccessFlags2 accessMask
        );

        Block Allocate(VkDeviceSize size);

        void Update
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue
        );

        void Destroy(VmaAllocator allocator);

        Vk::Buffer buffer = {};
    private:
        void QueueResize(VkDeviceSize currentMaxOffset, VkDeviceSize minRequiredCapacity);

        VkBufferUsageFlags    m_usage      = 0;
        VkPipelineStageFlags2 m_stageMask  = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2        m_accessMask = VK_ACCESS_2_NONE;

        VkDeviceSize m_capacity    = 0;
        VkDeviceSize m_oldCapacity = 0;

        std::optional<VkDeviceSize> m_resizeCopyOffset = std::nullopt;

        std::set<Block> m_usedBlocks = {};
    };
}

#endif
