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

#include "Renderer/RenderConstants.h"
#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"

namespace Renderer::Bloom
{
    RenderPass::RenderPass(const Vk::Context& context, Vk::FramebufferManager& framebufferManager)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context.device,
                context.commandPool,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY
            );

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("BloomPass/FIF{}", i));
        }

        framebufferManager.AddFramebuffer
        (
            "Bloom",
            Vk::FramebufferType::ColorHDR,
            Vk::ImageType::Array2D,
            [device = context.device] (const VkExtent2D& extent, Vk::FramebufferManager& framebufferManager)
            {
                auto& bloomBuffer = framebufferManager.GetFramebuffer("Bloom");

                bloomBuffer.size =
                {
                    .width       = extent.width,
                    .height      = extent.height,
                    .mipLevels   = static_cast<u32>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1,
                    .arrayLayers = 1
                };

                framebufferManager.DeleteFramebufferViews("Bloom", device);

                for (u32 mipLevel = 0; mipLevel < bloomBuffer.size.mipLevels; ++mipLevel)
                {
                    framebufferManager.AddFramebufferView
                    (
                        "Bloom",
                        fmt::format("BloomView/{}", mipLevel),
                        Vk::ImageType::Single2D,
                        {
                            .baseMipLevel   = mipLevel,
                            .levelCount     = 1,
                            .baseArrayLayer = 0,
                            .layerCount     = 1
                        }
                    );
                }
            }
        );

        Logger::Info("{}\n", "Created bloom pass!");
    }

    void RenderPass::Destroy(VkDevice device, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying bloom pass!");

        Vk::CommandBuffer::Free(device, cmdPool, cmdBuffers);
    }
}
