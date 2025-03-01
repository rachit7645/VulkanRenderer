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
        const std::string_view name,
        FramebufferType type,
        ImageType imageType,
        const FramebufferSizeData& sizeData
    )
    {
        if (m_framebuffers.contains(name.data()))
        {
            return;
        }

        m_framebuffers.emplace(name, Framebuffer{
            .type      = type,
            .sizeData  = sizeData,
            .imageType = imageType,
            .image     = {}
        });
    }

    void FramebufferManager::AddFramebufferView
    (
        const std::string_view framebufferName,
        const std::string_view name,
        ImageType imageType,
        const FramebufferViewSize& size
    )
    {
        m_framebufferViews.emplace(name, FramebufferView{
            .framebuffer     = framebufferName.data(),
            .descriptorIndex = std::numeric_limits<u32>::max(),
            .type            = imageType,
            .size            = size,
            .view            = {}
        });
    }

    void FramebufferManager::Update
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::MegaSet& megaSet,
        VkExtent2D swapExtent
    )
    {
        swapchainExtent = swapExtent;

        if (m_framebuffers.empty())
        {
            return;
        }

        std::unordered_set<std::string>    updatedFramebuffers;
        std::vector<VkImageMemoryBarrier2> barriers;

        for (auto& [name, framebuffer] : m_framebuffers)
        {
            const bool isFixedSize = std::holds_alternative<Vk::FramebufferSize>(framebuffer.sizeData);

            if
            (
                framebuffer.image.handle != VK_NULL_HANDLE &&
                isFixedSize &&
                m_fixedFramebuffers.contains(name)
            )
            {
                continue;
            }

            const auto size = GetFramebufferSize(swapchainExtent, framebuffer.sizeData);

            if (size.Matches(framebuffer.image))
            {
                continue;
            }

            framebuffer.image.Destroy(context.allocator);

            VkImageCreateInfo createInfo = {};
            {
                createInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                createInfo.pNext                 = nullptr;
                createInfo.imageType             = VK_IMAGE_TYPE_2D;
                createInfo.extent                = {size.width, size.height, 1};
                createInfo.arrayLayers           = size.arrayLayers;
                createInfo.mipLevels             = size.mipLevels;
                createInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
                createInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
                createInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
                createInfo.queueFamilyIndexCount = 0;
                createInfo.pQueueFamilyIndices   = nullptr;
                createInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
            }

            VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

            switch (framebuffer.type)
            {
            case FramebufferType::ColorR:
                createInfo.format = formatHelper.rFormat;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                break;

            case FramebufferType::ColorLDR:
                createInfo.format = formatHelper.colorAttachmentFormatLDR;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                break;

            case FramebufferType::ColorHDR:
                createInfo.format = formatHelper.colorAttachmentFormatHDR;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                break;

            case FramebufferType::Depth:
                createInfo.format = formatHelper.depthFormat;
                createInfo.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

                aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
                break;

            case FramebufferType::DepthStencil:
                createInfo.format = formatHelper.depthStencilFormat;
                createInfo.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

                aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
                break;
            }

            switch (framebuffer.imageType)
            {
            case ImageType::Single2D:
                createInfo.flags = 0;
                break;

            case ImageType::Array2D:
                createInfo.flags = 0;
                break;

            case ImageType::Cube:
                createInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
                break;

            case ImageType::ArrayCube:
                createInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
                break;
            }

            framebuffer.image = Vk::Image(context.allocator, createInfo, aspect);

            barriers.push_back(VkImageMemoryBarrier2{
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext               = nullptr,
                .srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask       = VK_ACCESS_2_NONE,
                .dstStageMask        = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask       = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = framebuffer.image.handle,
                .subresourceRange    = {
                    .aspectMask     = framebuffer.image.aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = framebuffer.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = framebuffer.image.arrayLayers
                }
            });

            Vk::SetDebugName(context.device, framebuffer.image.handle, name);

            if (isFixedSize)
            {
                m_fixedFramebuffers.insert(name);
            }

            updatedFramebuffers.insert(name);
        }

        for (auto& [name, framebufferView] : m_framebufferViews)
        {
            if (framebufferView.view.handle != VK_NULL_HANDLE && !updatedFramebuffers.contains(framebufferView.framebuffer))
            {
                continue;
            }

            const auto& framebuffer = GetFramebuffer(framebufferView.framebuffer);

            framebufferView.view.Destroy(context.device);

            VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;

            switch (framebufferView.type)
            {
            case ImageType::Single2D:
                viewType = VK_IMAGE_VIEW_TYPE_2D;
                break;
            case ImageType::Array2D:
                viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                break;
            case ImageType::Cube:
                viewType = VK_IMAGE_VIEW_TYPE_CUBE;
                break;
            case ImageType::ArrayCube:
                viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
                break;
            }

            framebufferView.view = Vk::ImageView
            (
                context.device,
                framebuffer.image,
                viewType,
                framebuffer.image.format,
                {
                    framebuffer.image.aspect,
                    framebufferView.size.baseMipLevel,
                    framebufferView.size.levelCount,
                    framebufferView.size.baseArrayLayer,
                    framebufferView.size.layerCount
                }
            );

            framebufferView.descriptorIndex = megaSet.WriteImage(framebufferView.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            Vk::SetDebugName(context.device, framebufferView.view.handle, name);
        }

        if (!barriers.empty())
        {
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
        }

        megaSet.Update(context.device);
    }

    Vk::Framebuffer& FramebufferManager::GetFramebuffer(const std::string_view name)
    {
        const auto iter = m_framebuffers.find(name.data());

        if (iter == m_framebuffers.end())
        {
            Logger::Error("Could not find framebuffer! [Name={}]\n", name);
        }

        return iter->second;
    }

    const Vk::Framebuffer& FramebufferManager::GetFramebuffer(const std::string_view name) const
    {
        const auto iter = m_framebuffers.find(name.data());

        if (iter == m_framebuffers.cend())
        {
            Logger::Error("Could not find framebuffer! [Name={}]\n", name);
        }

        return iter->second;
    }

    Vk::FramebufferView& FramebufferManager::GetFramebufferView(const std::string_view name)
    {
        const auto iter = m_framebufferViews.find(name.data());

        if (iter == m_framebufferViews.end())
        {
            Logger::Error("Could not find framebuffer view! [Name={}]\n", name);
        }

        return iter->second;
    }

    const Vk::FramebufferView& FramebufferManager::GetFramebufferView(const std::string_view name) const
    {
        const auto iter = m_framebufferViews.find(name.data());

        if (iter == m_framebufferViews.cend())
        {
            Logger::Error("Could not find framebuffer view! [Name={}]\n", name);
        }

        return iter->second;
    }

    void FramebufferManager::DeleteFramebufferViews(const std::string_view framebufferName, VkDevice device)
    {
        if (!m_framebuffers.contains(framebufferName.data()))
        {
            Logger::Error("Framebuffer not found! [Name={}]\n", framebufferName);
        }

        std::erase_if(m_framebufferViews, [&] (const auto& pair) -> bool
        {
            if (pair.second.framebuffer == framebufferName)
            {
                pair.second.view.Destroy(device);
                return true;
            }

            return false;
        });
    }

    void FramebufferManager::ImGuiDisplay()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Framebuffer Manager"))
            {
                for (const auto& [name, framebufferView] : m_framebufferViews)
                {
                    const auto& framebuffer = GetFramebuffer(framebufferView.framebuffer);

                    if (!IsViewable(framebufferView.type))
                    {
                        continue;
                    }

                    if (ImGui::TreeNode(name.c_str()))
                    {
                        ImGui::Text("Descriptor Index | %u",        framebufferView.descriptorIndex);
                        ImGui::Text("Width            | %u",        std::max(static_cast<u32>(framebuffer.image.width  * std::pow(0.5f, framebufferView.size.baseMipLevel)), 1u));
                        ImGui::Text("Height           | %u",        std::max(static_cast<u32>(framebuffer.image.height * std::pow(0.5f, framebufferView.size.baseMipLevel)), 1u));
                        ImGui::Text("Mipmap Levels    | [%u - %u]", framebufferView.size.baseMipLevel, framebufferView.size.baseMipLevel + framebufferView.size.levelCount);
                        ImGui::Text("Array Layers     | [%u - %u]", framebufferView.size.baseArrayLayer, framebufferView.size.baseArrayLayer + framebufferView.size.layerCount);
                        ImGui::Text("Format           | %s",        string_VkFormat(framebuffer.image.format));
                        ImGui::Text("Usage            | %s",        string_VkImageUsageFlags(framebuffer.image.usage).c_str());

                        ImGui::Separator();

                        const f32 originalWidth  = static_cast<f32>(framebuffer.image.width);
                        const f32 originalHeight = static_cast<f32>(framebuffer.image.height);

                        constexpr f32 MAX_SIZE = 1024.0f;

                        // Maintain aspect ratio
                        const f32  scale     = std::min(MAX_SIZE / originalWidth, MAX_SIZE / originalHeight);
                        const auto imageSize = ImVec2(originalWidth * scale, originalHeight * scale);

                        ImGui::Image(framebufferView.descriptorIndex, imageSize);

                        ImGui::TreePop();
                    }

                    ImGui::Separator();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    bool FramebufferManager::IsViewable(ImageType imageType)
    {
        return imageType == ImageType::Single2D;
    }

    FramebufferSize FramebufferManager::GetFramebufferSize(VkExtent2D extent, const FramebufferSizeData& sizeData)
    {
        if (std::holds_alternative<FramebufferSize>(sizeData))
        {
            return std::get<FramebufferSize>(sizeData);
        }

        if (std::holds_alternative<FramebufferResizeCallback>(sizeData))
        {
            return std::get<FramebufferResizeCallback>(sizeData)(extent, *this);
        }

        Logger::Error("{}\n", "Invalid framebuffer size!");
    }

    void FramebufferManager::Destroy(VkDevice device, VmaAllocator allocator)
    {
        for (const auto& framebuffer : m_framebuffers | std::views::values)
        {
            framebuffer.image.Destroy(allocator);
        }

        for (const auto& framebufferViews : m_framebufferViews | std::views::values)
        {
            framebufferViews.view.Destroy(device);
        }
    }
}
