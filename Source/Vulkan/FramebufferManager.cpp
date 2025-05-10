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
#include "Util/Log.h"
#include "Util/Visitor.h"

namespace Vk
{
    void FramebufferManager::AddFramebuffer
    (
        const std::string_view name,
        FramebufferType type,
        FramebufferImageType imageType,
        FramebufferUsage usage,
        const FramebufferSizeData& sizeData,
        const FramebufferInitialState& initialState
    )
    {
        if (m_framebuffers.contains(name.data()))
        {
            return;
        }

        m_framebuffers.emplace(name, Framebuffer{
            .type         = type,
            .imageType    = imageType,
            .usage        = usage,
            .sizeData     = sizeData,
            .initialState = initialState,
            .image        = {}
        });
    }

    void FramebufferManager::AddFramebufferView
    (
        const std::string_view framebufferName,
        const std::string_view name,
        FramebufferImageType imageType,
        const FramebufferViewSize& size
    )
    {
        m_framebufferViews.emplace(name, FramebufferView{
            .framebuffer     = framebufferName.data(),
            .sampledImageIndex = std::numeric_limits<u32>::max(),
            .type            = imageType,
            .size            = size,
            .view            = {}
        });
    }

    void FramebufferManager::Update
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        const Vk::FormatHelper& formatHelper,
        const VkExtent2D& extent,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (m_framebuffers.empty())
        {
            return;
        }

        // TODO: Figure out a better way
        if (extent.width == m_extent.width && extent.height == m_extent.height)
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "FramebufferManager::Update", {0.6421f, 0.1234f, 0.0316f, 1.0f});

        m_extent = extent;

        std::unordered_set<std::string> updatedFramebuffers = {};

        for (auto& [name, framebuffer] : m_framebuffers)
        {
            const bool isFixedSize = std::holds_alternative<Vk::FramebufferSize>(framebuffer.sizeData);

            if
            (
                framebuffer.image.handle != VK_NULL_HANDLE &&
                isFixedSize &&
                m_fixedSizeFramebuffers.contains(name)
            )
            {
                continue;
            }

            const auto size = GetFramebufferSize(framebuffer.sizeData, deletionQueue);

            if (size.Matches(framebuffer.image))
            {
                continue;
            }

            deletionQueue.PushDeletor([allocator, image = framebuffer.image] () mutable
            {
                image.Destroy(allocator);
            });

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
            case FramebufferType::ColorR_Unorm8:
                createInfo.format = formatHelper.r8UnormFormat;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                break;

            case FramebufferType::ColorR_SFloat32:
                createInfo.format = formatHelper.rSFloat32Format;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                break;

            case FramebufferType::ColorR_Uint32:
                createInfo.format = formatHelper.rUint32Format;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                break;

            case FramebufferType::ColorRG_SFloat16:
                createInfo.format = formatHelper.rgSFloat16Format;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                break;

            case FramebufferType::ColorRGBA_UNorm8:
                createInfo.format = formatHelper.rgba8UnormFormat;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                break;

            case FramebufferType::ColorBGR_SFloat_10_11_11:
                createInfo.format = formatHelper.b10g11r11SFloat;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                break;

            case FramebufferType::ColorLDR:
                createInfo.format = formatHelper.colorAttachmentFormatLDR;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                break;

            case FramebufferType::ColorHDR:
                createInfo.format = formatHelper.colorAttachmentFormatHDR;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                break;

            case FramebufferType::ColorHDR_WithAlpha:
                createInfo.format = formatHelper.colorAttachmentFormatHDRWithAlpha;
                createInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                aspect = VK_IMAGE_ASPECT_COLOR_BIT;
                break;

            case FramebufferType::Depth:
                createInfo.format = formatHelper.depthFormat;
                createInfo.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

                aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
                break;
            }

            switch (framebuffer.imageType)
            {
            case FramebufferImageType::Single2D:
                createInfo.flags = 0;
                break;

            case FramebufferImageType::Array2D:
                createInfo.flags = 0;
                break;

            case FramebufferImageType::Cube:
                createInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
                break;

            case FramebufferImageType::ArrayCube:
                createInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
                break;
            }

            // Add new usages here
            AddUsage<FramebufferUsage::Sampled,             VK_IMAGE_USAGE_SAMPLED_BIT     >(framebuffer.usage, createInfo.usage);
            AddUsage<FramebufferUsage::Storage,             VK_IMAGE_USAGE_STORAGE_BIT     >(framebuffer.usage, createInfo.usage);
            AddUsage<FramebufferUsage::TransferSource,      VK_IMAGE_USAGE_TRANSFER_SRC_BIT>(framebuffer.usage, createInfo.usage);
            AddUsage<FramebufferUsage::TransferDestination, VK_IMAGE_USAGE_TRANSFER_DST_BIT>(framebuffer.usage, createInfo.usage);

            framebuffer.image = Vk::Image(allocator, createInfo, aspect);

            Vk::SetDebugName(device, framebuffer.image.handle, name);

            if (isFixedSize)
            {
                m_fixedSizeFramebuffers.insert(name);
            }

            updatedFramebuffers.insert(name);

            m_barrierWriter.WriteImageBarrier
            (
                framebuffer.image,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                    .srcAccessMask  = VK_ACCESS_2_NONE,
                    .dstStageMask   = framebuffer.initialState.dstStageMask,
                    .dstAccessMask  = framebuffer.initialState.dstAccessMask,
                    .oldLayout      = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout      = framebuffer.initialState.initialLayout,
                    .baseMipLevel   = 0,
                    .levelCount     = framebuffer.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = framebuffer.image.arrayLayers
                }
            );
        }

        for (auto& [name, framebufferView] : m_framebufferViews)
        {
            if (framebufferView.view.handle != VK_NULL_HANDLE && !updatedFramebuffers.contains(framebufferView.framebuffer))
            {
                continue;
            }

            const auto& framebuffer = GetFramebuffer(framebufferView.framebuffer);

            FreeDescriptors
            (
                framebufferView,
                framebuffer.usage,
                megaSet,
                deletionQueue
            );

            deletionQueue.PushDeletor([device, view = framebufferView.view] ()
            {
                view.Destroy(device);
            });

            VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;

            switch (framebufferView.type)
            {
            case FramebufferImageType::Single2D:
                viewType = VK_IMAGE_VIEW_TYPE_2D;
                break;
            case FramebufferImageType::Array2D:
                viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                break;
            case FramebufferImageType::Cube:
                viewType = VK_IMAGE_VIEW_TYPE_CUBE;
                break;
            case FramebufferImageType::ArrayCube:
                viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
                break;
            }

            framebufferView.view = Vk::ImageView
            (
                device,
                framebuffer.image,
                viewType,
                {
                    .aspectMask     = framebuffer.image.aspect,
                    .baseMipLevel   = framebufferView.size.baseMipLevel,
                    .levelCount     = framebufferView.size.levelCount,
                    .baseArrayLayer = framebufferView.size.baseArrayLayer,
                    .layerCount     = framebufferView.size.layerCount
                }
            );

            AllocateDescriptors(megaSet, framebufferView, framebuffer.usage);

            Vk::SetDebugName(device, framebufferView.view.handle, name);
        }

        m_barrierWriter.Execute(cmdBuffer);

        megaSet.Update(device);

        Vk::EndLabel(cmdBuffer);
    }

    bool FramebufferManager::DoesFramebufferExist(const std::string_view name)
    {
        return m_framebuffers.contains(name.data());
    }

    bool FramebufferManager::DoesFramebufferViewExist(const std::string_view name)
    {
        return m_framebufferViews.contains(name.data());
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

    void FramebufferManager::DeleteFramebufferViews
    (
        const std::string_view framebufferName,
        VkDevice device,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (!m_framebuffers.contains(framebufferName.data()))
        {
            Logger::Error("Framebuffer not found! [Name={}]\n", framebufferName);
        }

        const auto& framebuffer = m_framebuffers.at(framebufferName.data());

        std::erase_if(m_framebufferViews, [&] (const auto& pair) -> bool
        {
            const Vk::FramebufferView& framebufferView = pair.second;

            if (framebufferView.framebuffer == framebufferName)
            {
                FreeDescriptors
                (
                    framebufferView,
                    framebuffer.usage,
                    megaSet,
                    deletionQueue
                );

                deletionQueue.PushDeletor([device, view = framebufferView.view] ()
                {
                    view.Destroy(device);
                });

                return true;
            }

            return false;
        });
    }

    FramebufferSize FramebufferManager::GetFramebufferSize(const FramebufferSizeData& sizeData, Util::DeletionQueue& deletionQueue)
    {
        return std::visit(Util::Visitor{
            [] (std::monostate) -> Vk::FramebufferSize
            {
                Logger::Error("{}\n", "Invalid framebuffer size!");
            },
            [] (const FramebufferSize& size) -> Vk::FramebufferSize
            {
                return size;
            },
            [this] (const FramebufferResizeCallbackWithExtent& Callback) -> Vk::FramebufferSize
            {
                return Callback(m_extent);
            },
            [this, &deletionQueue] (const FramebufferResizeCallbackWithExtentAndDeletionQueue& Callback) -> Vk::FramebufferSize
            {
                return Callback(m_extent, deletionQueue);
            }
        }, sizeData);
    }

    template<FramebufferUsage FBUsage, VkImageUsageFlags VkUsage>
    void FramebufferManager::AddUsage(FramebufferUsage framebufferUsage, VkImageUsageFlags& vulkanUsage)
    {
        if ((framebufferUsage & FBUsage) == FBUsage)
        {
            vulkanUsage |= VkUsage;
        }
    }

    void FramebufferManager::AllocateDescriptors
    (
        Vk::MegaSet& megaSet,
        Vk::FramebufferView& framebufferView,
        Vk::FramebufferUsage usage
    )
    {
        if ((usage & FramebufferUsage::Sampled) == FramebufferUsage::Sampled)
        {
            framebufferView.sampledImageIndex = megaSet.WriteSampledImage(framebufferView.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        if ((usage & FramebufferUsage::Storage) == FramebufferUsage::Storage)
        {
            framebufferView.storageImageIndex = megaSet.WriteStorageImage(framebufferView.view);
        }
    }

    void FramebufferManager::FreeDescriptors
    (
        const Vk::FramebufferView& framebufferView,
        Vk::FramebufferUsage usage,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (framebufferView.view.handle != VK_NULL_HANDLE)
        {
            if ((usage & FramebufferUsage::Sampled) == FramebufferUsage::Sampled)
            {
                deletionQueue.PushDeletor([&megaSet, id = framebufferView.sampledImageIndex]
                {
                    megaSet.FreeSampledImage(id);
                });
            }

            if ((usage & FramebufferUsage::Storage) == FramebufferUsage::Storage)
            {
                deletionQueue.PushDeletor([&megaSet, id = framebufferView.storageImageIndex]
                {
                    megaSet.FreeStorageImage(id);
                });
            }
        }
    }

    void FramebufferManager::ImGuiDisplay()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Framebuffer Manager"))
            {
                std::vector<std::pair<std::string, FramebufferView>> sortedFramebufferViews
                (
                    m_framebufferViews.begin(),
                    m_framebufferViews.end()
                );
                
                auto CustomOrderedSort = [] (const std::string& a, const std::string& b)
                {
                    usize i = 0;
                    usize j = 0;

                    while (i < a.length() && j < b.length())
                    {
                        if (std::isdigit(a[i]) && std::isdigit(b[j]))
                        {
                            const usize aNumberStart = i;
                            const usize bNumberStart = j;

                            while (i < a.length() && std::isdigit(a[i]))
                            {
                                ++i;
                            }

                            while (j < b.length() && std::isdigit(b[j]))
                            {
                                ++j;
                            }

                            const usize aAsNumber = std::stoi(a.substr(aNumberStart, i - aNumberStart));
                            const usize bAsNumber = std::stoi(b.substr(bNumberStart, j - bNumberStart));

                            if (aAsNumber != bAsNumber)
                            {
                                return aAsNumber < bAsNumber;
                            }

                            i = aNumberStart;
                            j = bNumberStart;
                        }

                        if (a[i] != b[j])
                        {
                            return a[i] < b[j];
                        }

                        ++i;
                        ++j;
                    }

                    return a.length() < b.length();
                };

                std::ranges::sort(sortedFramebufferViews,[&CustomOrderedSort] (const auto& a, const auto& b)
                {
                    return CustomOrderedSort(a.first, b.first);
                });

                for (const auto& [name, framebufferView] : sortedFramebufferViews)
                {
                    const auto& framebuffer = GetFramebuffer(framebufferView.framebuffer);

                    if (ImGui::TreeNode(name.c_str()))
                    {
                        ImGui::Text("Descriptor Index | %u",        framebufferView.sampledImageIndex);
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

                        ImGui::Image(framebufferView.sampledImageIndex, imageSize);

                        ImGui::TreePop();
                    }

                    ImGui::Separator();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
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
