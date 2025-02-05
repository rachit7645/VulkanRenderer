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

#include "FramebufferManager.h"

#include "DebugUtils.h"
#include "Util.h"
#include "Util/Log.h"

namespace Vk
{
    void FramebufferManager::AddFramebuffer
    (
        FramebufferType type,
        const std::string_view name,
        std::optional<VkExtent2D> resolution
    )
    {
        if (m_framebuffers.contains(name.data()))
        {
            return;
        }

        m_framebuffers.emplace(name, Framebuffer{type, resolution, 0, {}, {}});
    }

    void FramebufferManager::Update
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::MegaSet& megaSet,
        VkExtent2D swapchainExtent
    )
    {
        if (m_framebuffers.empty())
        {
            return;
        }

        std::vector<VkImageMemoryBarrier2> barriers = {};
        barriers.reserve(m_framebuffers.size());

        for (auto& [name, framebuffer] : m_framebuffers)
        {
            VkExtent2D resolution = swapchainExtent;

            if (framebuffer.resolution.has_value())
            {
                resolution = framebuffer.resolution.value();
            }

            if (framebuffer.image.width == resolution.width || framebuffer.image.height == resolution.height)
            {
                continue;
            }

            framebuffer.image.Destroy(context.allocator);
            framebuffer.imageView.Destroy(context.device);

            VkImageCreateInfo createInfo = {};
            {
                createInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                createInfo.pNext                 = nullptr;
                createInfo.flags                 = 0;
                createInfo.imageType             = VK_IMAGE_TYPE_2D;
                createInfo.extent                = {resolution.width, resolution.height, 1};
                createInfo.mipLevels             = 1;
                createInfo.arrayLayers           = 1;
                createInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
                createInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
                createInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
                createInfo.queueFamilyIndexCount = 0;
                createInfo.pQueueFamilyIndices   = nullptr;
                createInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
            }

            VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

            VkPipelineStageFlags2 dstStage  = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2        dstAccess = VK_ACCESS_2_NONE;
            VkImageLayout         newLayout = VK_IMAGE_LAYOUT_UNDEFINED;


            switch (framebuffer.type)
            {
            case FramebufferType::None:
                Logger::Error("Invalid framebuffer type! [Name={}]\n", name);
                break;

            case FramebufferType::ColorLDR:
                createInfo.format = formatHelper.colorAttachmentFormatLDR;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;

                dstStage  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                dstAccess = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
                newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                break;

            case FramebufferType::ColorHDR:
                createInfo.format = formatHelper.colorAttachmentFormatHDR;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;

                dstStage  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                dstAccess = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
                newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                break;

            case FramebufferType::Depth:
                createInfo.format = formatHelper.depthFormat;
                createInfo.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

                aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

                dstStage  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
                dstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                break;

            case FramebufferType::DepthStencil:
                createInfo.format = formatHelper.depthStencilFormat;
                createInfo.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

                aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

                dstStage  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
                dstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                break;
            }

            framebuffer.image = Vk::Image(context.allocator, createInfo, aspect);

            framebuffer.imageView = Vk::ImageView
            (
                context.device,
                framebuffer.image,
                VK_IMAGE_VIEW_TYPE_2D,
                framebuffer.image.format,
                {
                    .aspectMask     = framebuffer.image.aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = framebuffer.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            );

            barriers.push_back(VkImageMemoryBarrier2{
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext               = nullptr,
                .srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask       = VK_ACCESS_2_NONE,
                .dstStageMask        = dstStage,
                .dstAccessMask       = dstAccess,
                .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout           = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = framebuffer.image.handle,
                .subresourceRange    = {
                    .aspectMask     = framebuffer.image.aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = framebuffer.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            });

            if (IsViewable(framebuffer.type))
            {
                framebuffer.descriptorIndex = megaSet.WriteImage(framebuffer.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            Vk::SetDebugName(context.device, framebuffer.image.handle,     name);
            Vk::SetDebugName(context.device, framebuffer.imageView.handle, name + "_View");
        }

        Vk::ImmediateSubmit
        (
            context.device,
            context.graphicsQueue,
            context.commandPool,
            [&] (const Vk::CommandBuffer cmdBuffer)
            {
                const VkDependencyInfo dependencyInfo =
                {
                    .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                    .pNext                    = nullptr,
                    .dependencyFlags          = 0,
                    .memoryBarrierCount       = 0,
                    .pMemoryBarriers          = nullptr,
                    .bufferMemoryBarrierCount = 0,
                    .pBufferMemoryBarriers    = nullptr,
                    .imageMemoryBarrierCount  = static_cast<u32>(barriers.size()),
                    .pImageMemoryBarriers     = barriers.data()
                };

                vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencyInfo);
            }
        );

        megaSet.Update(context.device);
    }

    const FramebufferManager::Framebuffer& FramebufferManager::GetFramebuffer(const std::string_view name) const
    {
        const auto iter = m_framebuffers.find(name.data());

        if (iter == m_framebuffers.end())
        {
            Logger::Error("Could not find framebuffer! [Name={}]\n", name);
        }

        return iter->second;
    }

    void FramebufferManager::ImGuiDisplay()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Framebuffer Manager"))
            {
                for (const auto& [name, framebuffer] : m_framebuffers)
                {
                    if (!IsViewable(framebuffer.type))
                    {
                        continue;
                    }

                    if (ImGui::TreeNode(name.c_str()))
                    {
                        ImGui::Text("Descriptor Index | %u", framebuffer.descriptorIndex);
                        ImGui::Text("Width            | %u", framebuffer.image.width);
                        ImGui::Text("Height           | %u", framebuffer.image.height);
                        ImGui::Text("Format           | %s", string_VkFormat(framebuffer.image.format));
                        ImGui::Text("Usage            | %s", string_VkImageUsageFlags(framebuffer.image.usage).c_str());

                        ImGui::Separator();

                        const f32 originalWidth  = static_cast<f32>(framebuffer.image.width);
                        const f32 originalHeight = static_cast<f32>(framebuffer.image.height);

                        constexpr f32 MAX_SIZE = 1024.0f;

                        // Maintain aspect ratio
                        const f32  scale     = std::min(MAX_SIZE / originalWidth, MAX_SIZE / originalHeight);
                        const auto imageSize = ImVec2(originalWidth * scale, originalHeight * scale);

                        ImGui::Image(framebuffer.descriptorIndex, imageSize);

                        ImGui::TreePop();
                    }

                    ImGui::Separator();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    bool FramebufferManager::IsViewable(FramebufferType type)
    {
        switch (type)
        {
        case FramebufferType::ColorLDR:
        case FramebufferType::ColorHDR:
            return true;

        default:
            return false;
        }
    }

    void FramebufferManager::Destroy(VkDevice device, VmaAllocator allocator)
    {
        for (const auto& framebuffer : m_framebuffers | std::views::values)
        {
            framebuffer.image.Destroy(allocator);
            framebuffer.imageView.Destroy(device);
        }
    }
}
