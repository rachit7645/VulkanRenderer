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

#ifndef FRAME_BUFFER_MANAGER_H
#define FRAME_BUFFER_MANAGER_H

#include "Image.h"
#include "ImageView.h"
#include "FormatHelper.h"
#include "MegaSet.h"
#include "BarrierWriter.h"
#include "Util/Enum.h"
#include "Externals/UnorderedDense.h"

namespace Vk
{
    enum class FramebufferType : u8
    {
        // Special Color Formats
        ColorR_Unorm8,
        ColorR_Unorm16,
        ColorR_SFloat16,
        ColorR_SFloat32,
        ColorR_Uint32,
        ColorRG_Unorm8,
        ColorRG_Unorm16,
        ColorRG_SFloat16,
        ColorRGBA_UNorm8,
        ColorBGR_SFloat_10_11_11,
        // Regular Color Formats
        ColorLDR,
        ColorHDR,
        ColorHDR_WithAlpha,
        // Regular Depth Formats
        Depth
    };

    enum class FramebufferImageType : u8
    {
        Single2D,
        Array2D,
        Cube,
        ArrayCube
    };

    enum class FramebufferUsage : u8
    {
        None                = 0,
        Attachment          = 1U << 0,
        Sampled             = 1U << 1,
        Storage             = 1U << 2,
        TransferSource      = 1U << 3,
        TransferDestination = 1U << 4
    };

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
        FramebufferImageType type           = FramebufferImageType::Single2D;
        FramebufferViewSize  size           = {};
        Vk::ImageView        view           = {};
    };

    struct FramebufferInitialState
    {
        VkPipelineStageFlags2 dstStageMask  = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2        dstAccessMask = VK_ACCESS_2_NONE;
        VkImageLayout         initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    using FramebufferResizeCallbackWithExtent                 = std::function<FramebufferSize(const VkExtent2D&)>;
    using FramebufferResizeCallbackWithExtentAndDeletionQueue = std::function<FramebufferSize(const VkExtent2D&, Util::DeletionQueue& deletionQueue)>;

    using FramebufferSizeData = std::variant
    <
        FramebufferSize,
        FramebufferResizeCallbackWithExtent,
        FramebufferResizeCallbackWithExtentAndDeletionQueue
    >;

    struct Framebuffer
    {
        FramebufferType         type         = FramebufferType::ColorLDR;
        FramebufferImageType    imageType    = FramebufferImageType::Single2D;
        FramebufferUsage        usage        = FramebufferUsage::None;
        FramebufferSizeData     sizeData     = {};
        FramebufferInitialState initialState = {};
        Vk::Image               image        = {};
    };

    class FramebufferManager
    {
    public:
        void AddFramebuffer
        (
            const std::string_view name,
            FramebufferType type,
            FramebufferImageType imageType,
            FramebufferUsage usage,
            const FramebufferSizeData& sizeData,
            const FramebufferInitialState& initialState
        );

        void AddFramebufferView
        (
            const std::string_view framebufferName,
            const std::string_view name,
            FramebufferImageType imageType,
            const FramebufferViewSize& size
        );

        void Update
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            const Vk::FormatHelper& formatHelper,
            const VkExtent2D& extent,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        );

        [[nodiscard]] bool DoesFramebufferExist(const std::string_view name) const;
        [[nodiscard]] bool DoesFramebufferViewExist(const std::string_view name) const;

        [[nodiscard]] Framebuffer& GetFramebuffer(const std::string_view name);
        [[nodiscard]] const Framebuffer& GetFramebuffer(const std::string_view name) const;

        [[nodiscard]] FramebufferView& GetFramebufferView(const std::string_view name);
        [[nodiscard]] const FramebufferView& GetFramebufferView(const std::string_view name) const;

        void DeleteFramebufferViews
        (
            const std::string_view framebufferName,
            VkDevice device,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        );

        void ImGuiDisplay();
        void Destroy(VkDevice device, VmaAllocator allocator);
    private:
        FramebufferSize GetFramebufferSize(const FramebufferSizeData& sizeData, Util::DeletionQueue& deletionQueue) const;

        template<FramebufferUsage FBUsage, VkImageUsageFlags VkUsage>
        static void AddUsage(FramebufferUsage framebufferUsage, VkImageUsageFlags& vulkanUsage);

        void AllocateDescriptors
        (
            Vk::MegaSet& megaSet,
            Vk::FramebufferView& framebufferView,
            Vk::FramebufferUsage usage
        );

        void FreeDescriptors
        (
            const Vk::FramebufferView& framebufferView,
            Vk::FramebufferUsage usage,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        );

        ankerl::unordered_dense::map<std::string, Framebuffer>     m_framebuffers;
        ankerl::unordered_dense::map<std::string, FramebufferView> m_framebufferViews;

        ankerl::unordered_dense::set<std::string> m_fixedSizeFramebuffers;

        VkExtent2D m_extent = {};

        Vk::BarrierWriter m_barrierWriter = {};
    };
}

#endif
