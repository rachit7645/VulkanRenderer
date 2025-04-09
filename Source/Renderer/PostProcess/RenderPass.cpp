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

#include "Util/Log.h"
#include "Util/Ranges.h"
#include "Renderer/RenderConstants.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::PostProcess
{
    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::Swapchain& swapchain,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : pipeline(context, megaSet, textureManager, swapchain.imageFormat)
    {
        Logger::Info("{}\n", "Created post process pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::Swapchain& swapchain
    )
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Bloom"))
            {
                ImGui::DragFloat("Strength", &m_bloomStrength, 0.00125f, 0.0f, 1.0f, "%.4f");

                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        const auto& currentImageView = swapchain.imageViews[swapchain.imageIndex];
        const auto& currentImage     = swapchain.images[swapchain.imageIndex];

        Vk::BeginLabel(cmdBuffer, fmt::format("PostProcessPass/FIF{}", FIF), glm::vec4(0.0705f, 0.8588f, 0.2157f, 1.0f));

        currentImage.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = currentImage.aspect,
                .baseMipLevel   = 0,
                .levelCount     = currentImage.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = currentImageView.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {}
        };

        const VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {0, 0},
                .extent = swapchain.extent
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        pipeline.Bind(cmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(swapchain.extent.width),
            .height   = static_cast<f32>(swapchain.extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = swapchain.extent
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        pipeline.pushConstant =
        {
            .samplerIndex  = pipeline.samplerIndex,
            .imageIndex    = framebufferManager.GetFramebufferView("ResolvedSceneColorView").sampledImageIndex,
            .bloomIndex    = framebufferManager.GetFramebufferView("BloomView/0").sampledImageIndex,
            .bloomStrength = m_bloomStrength
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PostProcess::PushConstant),
            reinterpret_cast<void*>(&pipeline.pushConstant)
        );

        // Mega set
        const std::array descriptorSets = {megaSet.descriptorSet};
        pipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        vkCmdDraw
        (
            cmdBuffer.handle,
            3,
            1,
            0,
            0
        );

        vkCmdEndRendering(cmdBuffer.handle);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Destroy(VkDevice device)
    {
        Logger::Debug("{}\n", "Destroying swapchain pass!");

        pipeline.Destroy(device);
    }
}