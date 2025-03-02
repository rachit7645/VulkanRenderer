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

#include "SampleBuffer.h"

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"
#include "Util/Random.h"
#include "Util/Maths.h"

namespace Renderer::SSAO
{
    SampleBuffer::SampleBuffer(const Vk::Context& context)
    {
        constexpr u32          SAMPLE_COUNT = 16;
        constexpr VkDeviceSize BUFFER_SIZE  = sizeof(u32) + SAMPLE_COUNT * sizeof(glm::vec3);

        buffer = Vk::Buffer
        (
            context.allocator,
            BUFFER_SIZE,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO
        );

        auto stagingBuffer = Vk::Buffer
        (
            context.allocator,
            BUFFER_SIZE,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::array<glm::vec3, SAMPLE_COUNT> samples = {};

        for (usize i = 0; i < samples.size(); ++i)
        {
            samples[i] =
            {
                Util::TrueRandRange(0.0f, 1.0f) * 2.0f - 1.0f,
                Util::TrueRandRange(0.0f, 1.0f) * 2.0f - 1.0f,
                Util::TrueRandRange(0.0f, 1.0f)
            };

            samples[i]  = glm::normalize(samples[i]);
            samples[i] *= Util::TrueRandRange(0.0f, 1.0f);

            auto scale  =  static_cast<f32>(i) / static_cast<f32>(samples.size());
            scale       = Maths::Lerp(0.1f, 1.0f, scale * scale);
            samples[i] *= scale;
        }

        std::memcpy
        (
            stagingBuffer.allocationInfo.pMappedData,
            &SAMPLE_COUNT,
            sizeof(u32)
        );

        std::memcpy
        (
            static_cast<u8*>(stagingBuffer.allocationInfo.pMappedData) + sizeof(u32),
            &samples,
            samples.size() * sizeof(glm::vec3)
        );

        if (!(stagingBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                context.allocator,
                stagingBuffer.allocation,
                0,
                BUFFER_SIZE),
                "Failed to flush allocation!"
            );
        }

        Vk::ImmediateSubmit
        (
            context.device,
            context.graphicsQueue,
            context.commandPool,
            [&] (const Vk::CommandBuffer& cmdBuffer)
            {
                const VkBufferCopy2 copyRegion =
                {
                    .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                    .pNext     = nullptr,
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size      = BUFFER_SIZE,
                };

                const VkCopyBufferInfo2 copyInfo =
                {
                    .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                    .pNext       = nullptr,
                    .srcBuffer   = stagingBuffer.handle,
                    .dstBuffer   = buffer.handle,
                    .regionCount = 1,
                    .pRegions    = &copyRegion
                };

                vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

                buffer.Barrier
                (
                    cmdBuffer,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                    0,
                    BUFFER_SIZE
                );
            }
        );

        buffer.GetDeviceAddress(context.device);

        Vk::SetDebugName(context.device, buffer.handle, "SampleBuffer");

        stagingBuffer.Destroy(context.allocator);
    }

    void SampleBuffer::Destroy(VmaAllocator allocator)
    {
        buffer.Destroy(allocator);
    }
}
