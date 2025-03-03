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

#include "RenderPass.h"

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"

namespace Renderer::ShadowRT
{
    RenderPass::RenderPass
    (
        const Vk::Context& context,
        Vk::FramebufferManager& framebufferManager
    )
        : pipeline(context)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context.device,
                context.commandPool,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY
            );

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("ShadowRTPass/FIF{}", i));
        }

        framebufferManager.AddFramebuffer
        (
            "ShadowRT",
            Vk::FramebufferType::ColorR,
            Vk::ImageType::Single2D,
            VK_IMAGE_LAYOUT_GENERAL,
            [] (const VkExtent2D& extent, UNUSED Vk::FramebufferManager& framebufferManager) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = extent.width,
                    .height      = extent.height,
                    .mipLevels   = 1,
                    .arrayLayers = 1
                };
            }
        );

        framebufferManager.AddFramebufferView
        (
            "ShadowRT",
            "ShadowRTView",
            Vk::ImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            }
        );

        Logger::Info("{}\n", "Created shadow pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        VkDevice device,
        VmaAllocator allocator,
        const Vk::FramebufferManager& framebufferManager,
        Vk::AccelerationStructure& accelerationStructure,
        const std::span<const Renderer::RenderObject> renderObjects
    )
    {
        const auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        accelerationStructure.BuildTopLevelAS
        (
            FIF,
            currentCmdBuffer,
            device,
            allocator,
            renderObjects
        );

        currentCmdBuffer.EndRecording();
    }

    void RenderPass::Destroy(VkDevice device, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying shadow pass!");

        Vk::CommandBuffer::Free(device, cmdPool, cmdBuffers);

        pipeline.Destroy(device);
    }
}