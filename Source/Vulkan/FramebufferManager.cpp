/*
 * Copyright (c) 2023 - 2026 Rachit
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

#include <ranges>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/utility/vk_format_utils.h>

#include "DebugUtils.h"
#include "Util/Log.h"
#include "Util/Visitor.h"
#include "Externals/ImGui.h"

namespace Vk
{
    void FramebufferManager::AddFramebuffer
    (
        const std::string_view name,
        const FramebufferFormat& format,
        VkImageViewType imageViewType,
        VkImageUsageFlags imageUsage,
        const FramebufferSizeData& sizeData,
        const FramebufferInitialState& initialState
    )
    {
        if (m_framebuffers.contains(name))
        {
            return;
        }

        m_framebuffers.emplace(name, Vk::Framebuffer{
            .format        = format,
            .imageViewType = imageViewType,
            .imageUsage    = imageUsage,
            .sizeData      = sizeData,
            .initialState  = initialState,
            .image         = {}
        });
    }

    void FramebufferManager::AddFramebufferView
    (
        const std::string_view framebufferName,
        const std::string_view name,
        VkImageViewType imageViewType,
        const FramebufferViewSize& size
    )
    {
        m_framebufferViews.emplace(name, FramebufferView{
            .framebuffer     = framebufferName.data(),
            .sampledImageID  = std::numeric_limits<u32>::max(),
            .type            = imageViewType,
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
        const Vk::Swapchain& swapchain,
        ENGINE_UNUSED Renderer::RenderConfig& renderConfig,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (m_framebuffers.empty())
        {
            return;
        }

        #ifdef ENGINE_DLSS
        if (renderConfig.antiAliasingMode == Renderer::RenderConfig::AntiAliasingMode::DLSS)
        {
            const auto DLSSExtent = glm::vk_cast(renderConfig.DLSSConfig.GetInternalResolution(glm::vk_cast(swapchain.extent)));

            if (DLSSExtent.width == renderExtent.width && DLSSExtent.height == renderExtent.height)
            {
                return;
            }

            renderExtent  = DLSSExtent;
            displayExtent = swapchain.extent;
        }
        else
        #endif
        {
            VkExtent2D scaledRenderExtent = glm::vk_cast(glm::uvec2(glm::ceil(renderConfig.resolutionScale * glm::vec2(glm::vk_cast(swapchain.extent)))));

            if (scaledRenderExtent.width == 0 || scaledRenderExtent.height == 0)
            {
                scaledRenderExtent = swapchain.extent;
            }

            const bool didScalingChange = scaledRenderExtent.width != renderExtent.width  || scaledRenderExtent.height != renderExtent.height;
            const bool didDisplayChange = swapchain.extent.width   != displayExtent.width || swapchain.extent.height   != displayExtent.height;

            if (!didScalingChange && !didDisplayChange)
            {
                return;
            }

            renderExtent  = scaledRenderExtent;
            displayExtent = swapchain.extent;
        }

        Vk::BeginLabel(cmdBuffer, "FramebufferManager::Update", {0.6421f, 0.1234f, 0.0316f, 1.0f});

        ankerl::unordered_dense::set<std::string> updatedFramebuffers = {};

        Vk::BarrierWriter barrierWriter = {};

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

            deletionQueue.Push([allocator, image = framebuffer.image] () mutable
            {
                image.Destroy(allocator);
            });

            VkImageCreateInfo createInfo = {};
            {
                createInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                createInfo.pNext                 = nullptr;
                createInfo.imageType             = VK_IMAGE_TYPE_2D;
                createInfo.extent                = {.width = size.width, .height = size.height, .depth = 1};
                createInfo.arrayLayers           = size.arrayLayers;
                createInfo.mipLevels             = size.mipLevels;
                createInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
                createInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
                createInfo.usage                 = framebuffer.imageUsage;
                createInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
                createInfo.queueFamilyIndexCount = 0;
                createInfo.pQueueFamilyIndices   = nullptr;
                createInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
            }

            createInfo.format = std::visit(Util::Visitor{
                [] (VkFormat format) -> VkFormat
                {
                    return format;
                },
                [&formatHelper] (Vk::FramebufferCustomFormat format) -> VkFormat
                {
                    switch (format)
                    {
                    case FramebufferCustomFormat::ColorLDR:
                        return formatHelper.colorAttachmentFormatLDR;

                    case FramebufferCustomFormat::ColorHDR:
                        return formatHelper.colorAttachmentFormatHDR;

                    case FramebufferCustomFormat::Depth:
                        return formatHelper.depthFormat;

                    default:
                        return VK_FORMAT_UNDEFINED;
                    }
                }
            }, framebuffer.format);

            const VkImageAspectFlags aspect = vkuFormatHasDepth(createInfo.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

            switch (framebuffer.imageViewType)
            {
            case VK_IMAGE_VIEW_TYPE_2D:
            case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
                createInfo.flags = 0;
                break;

            case VK_IMAGE_VIEW_TYPE_CUBE:
            case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
                createInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
                break;

            default:
                createInfo.flags = 0;
            }

            framebuffer.image = Vk::Image(allocator, createInfo, aspect);

            Vk::SetDebugName(device, framebuffer.image.handle, name);

            if (isFixedSize)
            {
                m_fixedSizeFramebuffers.insert(name);
            }

            updatedFramebuffers.insert(name);

            barrierWriter.WriteImageBarrier
            (
                framebuffer.image,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                    .srcAccessMask  = VK_ACCESS_2_NONE,
                    .dstStageMask   = framebuffer.initialState.stageMask,
                    .dstAccessMask  = framebuffer.initialState.accessMask,
                    .oldLayout      = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout      = framebuffer.initialState.layout,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
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
                framebuffer.imageUsage,
                megaSet,
                deletionQueue
            );

            deletionQueue.Push([device, view = framebufferView.view] ()
            {
                view.Destroy(device);
            });

            framebufferView.view = Vk::ImageView
            (
                device,
                framebuffer.image,
                framebufferView.type,
                {
                    .aspectMask     = framebuffer.image.aspect,
                    .baseMipLevel   = framebufferView.size.baseMipLevel,
                    .levelCount     = framebufferView.size.levelCount,
                    .baseArrayLayer = framebufferView.size.baseArrayLayer,
                    .layerCount     = framebufferView.size.layerCount
                }
            );

            AllocateDescriptors(megaSet, framebufferView, framebuffer.imageUsage);

            Vk::SetDebugName(device, framebufferView.view.handle, name);
        }

        barrierWriter.Execute(cmdBuffer);

        megaSet.Update(device);

        Vk::EndLabel(cmdBuffer);
    }

    bool FramebufferManager::DoesFramebufferExist(const std::string_view name) const
    {
        return m_framebuffers.contains(name);
    }

    bool FramebufferManager::DoesFramebufferViewExist(const std::string_view name) const
    {
        return m_framebufferViews.contains(name);
    }

    Vk::Framebuffer& FramebufferManager::GetFramebuffer(const std::string_view name)
    {
        const auto iter = m_framebuffers.find(name);

        if (iter == m_framebuffers.end())
        {
            Logger::Error("Could not find framebuffer! [Name={}]\n", name);
        }

        return iter->second;
    }

    const Vk::Framebuffer& FramebufferManager::GetFramebuffer(const std::string_view name) const
    {
        const auto iter = m_framebuffers.find(name);

        if (iter == m_framebuffers.cend())
        {
            Logger::Error("Could not find framebuffer! [Name={}]\n", name);
        }

        return iter->second;
    }

    Vk::FramebufferView& FramebufferManager::GetFramebufferView(const std::string_view name)
    {
        const auto iter = m_framebufferViews.find(name);

        if (iter == m_framebufferViews.end())
        {
            Logger::Error("Could not find framebuffer view! [Name={}]\n", name);
        }

        return iter->second;
    }

    const Vk::FramebufferView& FramebufferManager::GetFramebufferView(const std::string_view name) const
    {
        const auto iter = m_framebufferViews.find(name);

        if (iter == m_framebufferViews.cend())
        {
            Logger::Error("Could not find framebuffer view! [Name={}]\n", name);
        }

        return iter->second;
    }

    void FramebufferManager::DeleteFramebuffer
    (
        const std::string_view framebufferName,
        VkDevice device,
        VmaAllocator allocator,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        const auto iter = m_framebuffers.find(framebufferName);

        if (iter == m_framebuffers.end())
        {
            Logger::Error("Could not find framebuffer! [Name={}]\n", framebufferName);
        }

        const auto& framebuffer = iter->second;

        std::erase_if(m_framebufferViews, [&] (const auto& pair) -> bool
        {
            const Vk::FramebufferView& framebufferView = pair.second;

            if (framebufferView.framebuffer != framebufferName)
            {
                return false;
            }

            FreeDescriptors
            (
                framebufferView,
                framebuffer.imageUsage,
                megaSet,
                deletionQueue
            );

            deletionQueue.Push([device, view = framebufferView.view] ()
            {
                view.Destroy(device);
            });

            return true;
        });

        framebuffer.image.Destroy(allocator);

        m_framebuffers.erase(iter);
    }

    void FramebufferManager::DeleteFramebufferViews
    (
        const std::string_view framebufferName,
        VkDevice device,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        const auto& framebuffer = GetFramebuffer(framebufferName);

        std::erase_if(m_framebufferViews, [&] (const auto& pair) -> bool
        {
            const Vk::FramebufferView& framebufferView = pair.second;

            if (framebufferView.framebuffer != framebufferName)
            {
                return false;
            }

            FreeDescriptors
            (
                framebufferView,
                framebuffer.imageUsage,
                megaSet,
                deletionQueue
            );

            deletionQueue.Push([device, view = framebufferView.view] ()
            {
                view.Destroy(device);
            });

            return true;
        });
    }

    FramebufferSize FramebufferManager::GetFramebufferSize(const FramebufferSizeData& sizeData, Util::DeletionQueue& deletionQueue) const
    {
        return std::visit(Util::Visitor{
            [] (const FramebufferSize& size) -> Vk::FramebufferSize
            {
                return size;
            },
            [this] (const FramebufferResizeCallbackWithExtent& Callback) -> Vk::FramebufferSize
            {
                return Callback(renderExtent, displayExtent);
            },
            [this, &deletionQueue] (const FramebufferResizeCallbackWithExtentAndDeletionQueue& Callback) -> Vk::FramebufferSize
            {
                return Callback(renderExtent, displayExtent, deletionQueue);
            }
        }, sizeData);
    }

    void FramebufferManager::AllocateDescriptors
    (
        Vk::MegaSet& megaSet,
        Vk::FramebufferView& framebufferView,
        VkImageUsageFlags imageUsage
    )
    {
        if (imageUsage & VK_IMAGE_USAGE_SAMPLED_BIT)
        {
            framebufferView.sampledImageID = megaSet.WriteSampledImage(framebufferView.view);
        }

        if (imageUsage & VK_IMAGE_USAGE_STORAGE_BIT)
        {
            framebufferView.storageImageID = megaSet.WriteStorageImage(framebufferView.view);
        }
    }

    void FramebufferManager::FreeDescriptors
    (
        const Vk::FramebufferView& framebufferView,
        VkImageUsageFlags imageUsage,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (framebufferView.view.handle == VK_NULL_HANDLE)
        {
            return;
        }

        if (imageUsage & VK_IMAGE_USAGE_SAMPLED_BIT)
        {
            deletionQueue.Push([&megaSet, id = framebufferView.sampledImageID]
            {
                megaSet.FreeSampledImage(id);
            });
        }

        if (imageUsage & VK_IMAGE_USAGE_STORAGE_BIT)
        {
            deletionQueue.Push([&megaSet, id = framebufferView.storageImageID]
            {
                megaSet.FreeStorageImage(id);
            });
        }
    }

    void FramebufferManager::ImGuiDisplay()
    {
        if (ImGui::CollapsingHeader("Framebuffers"))
        {
            VkDeviceSize totalMemoryUsed = 0;

            for (const auto& [name, framebuffer] : m_framebuffers)
            {
                totalMemoryUsed += framebuffer.image.size;

                if (ImGui::TreeNode(name.c_str()))
                {
                    ImGui::Text("Handle          | %p", reinterpret_cast<void*>(framebuffer.image.handle));
                    ImGui::Text("Allocation      | %p", reinterpret_cast<void*>(framebuffer.image.allocation));

                    ImGui::Separator();

                    ImGui::Text("Width           | %u", framebuffer.image.width);
                    ImGui::Text("Height          | %u", framebuffer.image.height);
                    ImGui::Text("Mipmap Levels   | %u", framebuffer.image.mipLevels);
                    ImGui::Text("Array Layers    | %u", framebuffer.image.arrayLayers);
                    ImGui::Text("Format          | %s", string_VkFormat(framebuffer.image.format));
                    ImGui::Text("Aspect          | %s", string_VkImageAspectFlags(framebuffer.image.aspect).c_str());

                    ImGui::Separator();

                    const auto sizeDataType = std::visit(Util::Visitor{
                        [] (ENGINE_UNUSED const FramebufferSize& size) -> std::string_view
                        {
                            return "Static";
                        },
                        [] (ENGINE_UNUSED const FramebufferResizeCallbackWithExtent& Callback) -> std::string_view
                        {
                            return "Dynamic";
                        },
                        [] (ENGINE_UNUSED const FramebufferResizeCallbackWithExtentAndDeletionQueue& Callback) -> std::string_view
                        {
                            return "Dynamic+";
                        }
                    }, framebuffer.sizeData);

                    ImGui::Text("Image View Type | %s", string_VkImageViewType(framebuffer.imageViewType));
                    ImGui::Text("Image Usage     | %s", string_VkImageUsageFlags(framebuffer.imageUsage).c_str());
                    ImGui::Text("Size Data Type  | %s", sizeDataType.data());

                    ImGui::Separator();

                    ImGui::Text("Initial Stage   | %s", string_VkPipelineStageFlags2(framebuffer.initialState.stageMask).c_str());
                    ImGui::Text("Initial Access  | %s", string_VkAccessFlags2(framebuffer.initialState.accessMask).c_str());
                    ImGui::Text("Initial Layout  | %s", string_VkImageLayout(framebuffer.initialState.layout));

                    for (const auto& [viewName, framebufferView] : m_framebufferViews)
                    {
                        if (framebufferView.framebuffer != name)
                        {
                            continue;
                        }

                        ImGui::Separator();

                        if (ImGui::TreeNode(viewName.c_str()))
                        {
                            ImGui::Text("Handle                   | %p", reinterpret_cast<void*>(framebufferView.view.handle));

                            ImGui::Separator();

                            if (framebuffer.imageUsage & VK_IMAGE_USAGE_SAMPLED_BIT)
                            {
                                ImGui::Text("Sampled Image Descriptor | %u", framebufferView.sampledImageID);
                            }

                            if (framebuffer.imageUsage & VK_IMAGE_USAGE_STORAGE_BIT)
                            {
                                ImGui::Text("Storage Image Descriptor | %u", framebufferView.storageImageID);
                            }

                            ImGui::Text("Image View Type          | %s", string_VkImageViewType(framebufferView.type));

                            ImGui::Separator();

                            ImGui::Text("Base Mipmap Level        | %u", framebufferView.size.baseMipLevel);
                            ImGui::Text("Mipmap Level Count       | %u", framebufferView.size.levelCount);
                            ImGui::Text("Base Array Layer         | %u", framebufferView.size.baseArrayLayer);
                            ImGui::Text("Array Layer Count        | %u", framebufferView.size.layerCount);

                            if (framebuffer.imageUsage & VK_IMAGE_USAGE_SAMPLED_BIT)
                            {
                                ImGui::Separator();

                                const f32 originalWidth  = static_cast<f32>(framebuffer.image.width);
                                const f32 originalHeight = static_cast<f32>(framebuffer.image.height);

                                constexpr f32 MAX_SIZE = 1024.0f;

                                // Maintain aspect ratio
                                const f32  scale     = std::min(MAX_SIZE / originalWidth, MAX_SIZE / originalHeight);
                                const auto imageSize = ImVec2(originalWidth * scale, originalHeight * scale);

                                ImGui::Image(framebufferView.sampledImageID, imageSize);
                            }

                            ImGui::TreePop();
                        }
                    }

                    ImGui::TreePop();
                }

                ImGui::Separator();
            }

            ImGui::Text("Total Framebuffer Memory Usage | %llu Bytes", totalMemoryUsed);
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

    bool FramebufferSize::Matches(const Vk::Image& image) const
    {
        if (image.handle == VK_NULL_HANDLE)
        {
            return false;
        }

        return width == image.width &&
               height == image.height &&
               mipLevels == image.mipLevels &&
               arrayLayers == image.arrayLayers;
    }
}
