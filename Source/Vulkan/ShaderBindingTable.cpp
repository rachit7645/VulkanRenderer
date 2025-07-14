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

#include "ShaderBindingTable.h"
#include "DebugUtils.h"
#include "ImmediateSubmit.h"
#include "Util/Align.h"

namespace Vk
{
    ShaderBindingTable::ShaderBindingTable
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        const Vk::Pipeline& pipeline,
        u32 missCount,
        u32 hitCount,
        Util::DeletionQueue& deletionQueue
    )
    {
        Vk::BeginLabel(cmdBuffer, "Shader Binding Table Build", glm::vec4(0.4126f, 0.7488f, 0.5581f, 1.0f));

        const u32 shaderGroupHandleSize      = context.physicalDeviceRayTracingPipelineProperties.shaderGroupHandleSize;
        const u32 shaderGroupHandleAlignment = context.physicalDeviceRayTracingPipelineProperties.shaderGroupHandleAlignment;
        const u32 shaderGroupBaseAlignment   = context.physicalDeviceRayTracingPipelineProperties.shaderGroupBaseAlignment;

        const u32          handleCount = 1 + missCount + hitCount;
        const VkDeviceSize handlesSize = handleCount * shaderGroupHandleSize;

        auto handlesData = std::vector<u8>(handlesSize);

        Vk::CheckResult(vkGetRayTracingShaderGroupHandlesKHR(
            context.device,
            pipeline.handle,
            0,
            handleCount,
            handlesSize,
            handlesData.data()),
            "Failed to get ray tracing shader group handles!"
        );

        const VkDeviceSize handleSizeAligned = Util::Align(shaderGroupHandleSize, shaderGroupHandleAlignment);

        raygenRegion.stride = Util::Align(handleSizeAligned, shaderGroupBaseAlignment);
        raygenRegion.size   = raygenRegion.stride;

        missRegion.stride = handleSizeAligned;
        missRegion.size   = Util::Align(missCount * handleSizeAligned, shaderGroupBaseAlignment);

        hitRegion.stride = handleSizeAligned;
        hitRegion.size   = Util::Align(hitCount * handleSizeAligned, shaderGroupBaseAlignment);

        const VkDeviceSize sbtSize = raygenRegion.size + missRegion.size + hitRegion.size;

        auto stagingBuffer = Vk::Buffer
        (
            context.allocator,
            sbtSize,
            0,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        deletionQueue.PushDeletor([allocator = context.allocator, buffer = stagingBuffer] () mutable
        {
            buffer.Destroy(allocator);
        });

        const auto pMappedData = static_cast<u8*>(stagingBuffer.hostAddress);

        const usize raygenOffset = 0;
        const usize missOffset   = raygenOffset + 1         * shaderGroupHandleSize;
        const usize hitOffset    = missOffset   + missCount * shaderGroupHandleSize;

        // Raygen
        std::memcpy
        (
            pMappedData,
            handlesData.data(),
            shaderGroupHandleSize
        );

        // Miss
        for (u32 i = 0; i < missCount; ++i)
        {
            std::memcpy
            (
                pMappedData + raygenRegion.size + (i * missRegion.stride),     // Base + RayGen + Current Miss (Strided)
                handlesData.data() + missOffset + (i * shaderGroupHandleSize), // Base + Raygen + Current Miss
                shaderGroupHandleSize
            );
        }

        // Hit
        for (u32 i = 0; i < hitCount; ++i)
        {
            std::memcpy
            (
                pMappedData + raygenRegion.size + missRegion.size + (i * hitRegion.stride), // Base + RayGen + Miss + Current Hit (Strided)
                handlesData.data() + hitOffset + (i * shaderGroupHandleSize),               // Base + Raygen + Miss + Current Hit
                shaderGroupHandleSize
            );
        }

        if (!(stagingBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                context.allocator,
                stagingBuffer.allocation,
                0,
                sbtSize),
                "Failed to flush allocation!"
            );
        }

        m_buffer = Vk::Buffer
        (
            context.allocator,
            sbtSize,
            0,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        const VkBufferCopy2 copyRegion =
        {
            .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .pNext     = nullptr,
            .srcOffset = 0,
            .dstOffset = 0,
            .size      = sbtSize,
        };

        const VkCopyBufferInfo2 copyInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext       = nullptr,
            .srcBuffer   = stagingBuffer.handle,
            .dstBuffer   = m_buffer.handle,
            .regionCount = 1,
            .pRegions    = &copyRegion
        };

        vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

        m_buffer.Barrier
        (
            cmdBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                .dstAccessMask  = VK_ACCESS_2_SHADER_BINDING_TABLE_READ_BIT_KHR,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sbtSize
            }
        );

        m_buffer.GetDeviceAddress(context.device);

        raygenRegion.deviceAddress = m_buffer.deviceAddress;
        missRegion.deviceAddress   = m_buffer.deviceAddress + raygenRegion.size;
        hitRegion.deviceAddress    = m_buffer.deviceAddress + raygenRegion.size + missRegion.size;

        Vk::SetDebugName(context.device, m_buffer.handle, "ShaderBindingTable/Buffer");

        Vk::EndLabel(cmdBuffer);
    }

    void ShaderBindingTable::Destroy(VmaAllocator allocator)
    {
        m_buffer.Destroy(allocator);
    }
}