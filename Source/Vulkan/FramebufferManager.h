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

#ifndef FRAME_BUFFER_MANAGER_H
#define FRAME_BUFFER_MANAGER_H

#include <variant>

#include "Image.h"
#include "ImageView.h"
#include "FormatHelper.h"
#include "MegaSet.h"
#include "BarrierWriter.h"
#include "Util/Enum.h"
#include "Externals/UnorderedDense.h"
#include "Renderer/RenderConfig.h"
#include "Util/String.h"
#include "Vulkan/Swapchain.h"

namespace Vk
{
    enum class FramebufferCustomFormat : u8
    {
        // Regular Color Formats
        ColorLDR,
        ColorHDR,
        // Regular Depth Formats
        Depth
    };

    using FramebufferFormat = std::variant<VkFormat, FramebufferCustomFormat>;

    struct FramebufferSize
    {
        u32 width       = 0;
        u32 height      = 0;
        u32 mipLevels   = 0;
        u32 arrayLayers = 0;

        [[nodiscard]] bool Matches(const Vk::Image& image) const;
    };

    struct FramebufferViewSize
    {
        u32 baseMipLevel   = 0;
        u32 levelCount     = 0;
        u32 baseArrayLayer = 0;
        u32 layerCount     = 0;
    };

    struct FramebufferView
    {
        std::string          framebuffer    = {};
        u32                  sampledImageID = 0;
        u32                  storageImageID = 0;
        VkImageViewType      type           = VK_IMAGE_VIEW_TYPE_2D;
        FramebufferViewSize  size           = {};
        Vk::ImageView        view           = {};
    };

    struct FramebufferInitialState
    {
        VkPipelineStageFlags2 stageMask  = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2        accessMask = VK_ACCESS_2_NONE;
        VkImageLayout         layout     = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    using FramebufferResizeCallbackWithExtent                 = std::function<FramebufferSize(const VkExtent2D&, const VkExtent2D&)>;
    using FramebufferResizeCallbackWithExtentAndDeletionQueue = std::function<FramebufferSize(const VkExtent2D&, const VkExtent2D&, Engine::DeletionQueue& deletionQueue)>;

    using FramebufferSizeData = std::variant
    <
        FramebufferSize,                                    // Static
        FramebufferResizeCallbackWithExtent,                // Dynamic
        FramebufferResizeCallbackWithExtentAndDeletionQueue // Dynamic+
    >;

    struct Framebuffer
    {
        FramebufferFormat       format        = FramebufferCustomFormat::ColorLDR;
        VkImageViewType         imageViewType = VK_IMAGE_VIEW_TYPE_2D;
        VkImageUsageFlags       imageUsage    = 0;
        FramebufferSizeData     sizeData      = {};
        FramebufferInitialState initialState  = {};
        Vk::Image               image         = {};
    };

    class FramebufferManager
    {
    public:
        void AddFramebuffer
        (
            const std::string_view name,
            const FramebufferFormat& format,
            VkImageViewType imageViewType,
            VkImageUsageFlags imageUsage,
            const FramebufferSizeData& sizeData,
            const FramebufferInitialState& initialState
        );

        void AddFramebufferView
        (
            const std::string_view framebufferName,
            const std::string_view name,
            VkImageViewType imageType,
            const FramebufferViewSize& size
        );

        void Update
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            const Vk::FormatHelper& formatHelper,
            const Vk::Swapchain& swapchain,
            Renderer::RenderConfig& renderConfig,
            Vk::MegaSet& megaSet,
            Scratch::Allocator& scratchAllocator,
            Engine::DeletionQueue& deletionQueue
        );

        [[nodiscard]] bool DoesFramebufferExist(const std::string_view name) const;
        [[nodiscard]] bool DoesFramebufferViewExist(const std::string_view name) const;

        [[nodiscard]] Framebuffer& GetFramebuffer(const std::string_view name);
        [[nodiscard]] const Framebuffer& GetFramebuffer(const std::string_view name) const;

        [[nodiscard]] FramebufferView& GetFramebufferView(const std::string_view name);
        [[nodiscard]] const FramebufferView& GetFramebufferView(const std::string_view name) const;

        void DeleteFramebuffer
        (
            const std::string_view framebufferName,
            VkDevice device,
            VmaAllocator allocator,
            Vk::MegaSet& megaSet,
            Engine::DeletionQueue& deletionQueue
        );

        void DeleteFramebufferViews
        (
            const std::string_view framebufferName,
            VkDevice device,
            Vk::MegaSet& megaSet,
            Engine::DeletionQueue& deletionQueue
        );

        void ImGuiDisplay();
        void Destroy(VkDevice device, VmaAllocator allocator);

        VkExtent2D renderExtent  = {};
        VkExtent2D displayExtent = {};
    private:
        FramebufferSize GetFramebufferSize(const FramebufferSizeData& sizeData, Engine::DeletionQueue& deletionQueue) const;

        void AllocateDescriptors
        (
            Vk::MegaSet& megaSet,
            Vk::FramebufferView& framebufferView,
            VkImageUsageFlags imageUsage
        );

        void FreeDescriptors
        (
            const Vk::FramebufferView& framebufferView,
            VkImageUsageFlags imageUsage,
            Vk::MegaSet& megaSet,
            Engine::DeletionQueue& deletionQueue
        );

        ankerl::unordered_dense::map<std::string, Framebuffer,     Util::StringHash, std::equal_to<>> m_framebuffers;
        ankerl::unordered_dense::map<std::string, FramebufferView, Util::StringHash, std::equal_to<>> m_framebufferViews;

        ankerl::unordered_dense::set<std::string, Util::StringHash, std::equal_to<>> m_fixedSizeFramebuffers;
    };
}

#endif
