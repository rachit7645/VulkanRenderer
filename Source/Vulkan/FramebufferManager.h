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

#include <unordered_set>
#include <unordered_map>
#include <map>

#include "Image.h"
#include "ImageView.h"
#include "FormatHelper.h"
#include "MegaSet.h"
#include "Util/Enum.h"

namespace Vk
{
    class FramebufferManager;

    enum class FramebufferType : u8
    {
        // Special Color Formats
        ColorR_Unorm8,
        ColorRG_SFloat,
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
        Sampled             = 1U << 0,
        Storage             = 1U << 1,
        TransferDestination = 1U << 2
    };

    struct FramebufferSize
    {
        u32 width       = 0;
        u32 height      = 0;
        u32 mipLevels   = 0;
        u32 arrayLayers = 0;

        bool Matches(const Vk::Image& image) const
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
        std::string          framebuffer       = {};
        u32                  sampledImageIndex = 0;
        u32                  storageImageIndex = 0;
        FramebufferImageType type              = FramebufferImageType::Single2D;
        FramebufferViewSize  size              = {};
        Vk::ImageView        view              = {};
    };

    using FramebufferResizeCallback = std::function<FramebufferSize(const VkExtent2D&, FramebufferManager&)>;
    using FramebufferSizeData       = std::variant<std::monostate, FramebufferSize, FramebufferResizeCallback>;

    struct Framebuffer
    {
        FramebufferType      type      = FramebufferType::ColorLDR;
        FramebufferSizeData  sizeData  = {};
        FramebufferImageType imageType = FramebufferImageType::Single2D;
        FramebufferUsage     usage     = FramebufferUsage::None;
        Vk::Image            image     = {};
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
            const FramebufferSizeData& sizeData
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
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Vk::MegaSet& megaSet,
            VkExtent2D swapchainExtent
        );

        [[nodiscard]] Framebuffer& GetFramebuffer(const std::string_view name);
        [[nodiscard]] const Framebuffer& GetFramebuffer(const std::string_view name) const;
        [[nodiscard]] FramebufferView& GetFramebufferView(const std::string_view name);
        [[nodiscard]] const FramebufferView& GetFramebufferView(const std::string_view name) const;

        void DeleteFramebufferViews
        (
            const std::string_view framebufferName,
            VkDevice device,
            Vk::MegaSet& megaSet
        );

        void ImGuiDisplay();
        void Destroy(VkDevice device, VmaAllocator allocator);
    private:
        FramebufferSize GetFramebufferSize(VkExtent2D extent, const FramebufferSizeData& sizeData);

        template<FramebufferUsage FBUsage, VkImageUsageFlags VkUsage>
        void AddUsage(FramebufferUsage framebufferUsage, VkImageUsageFlags& vulkanUsage);

        void AllocateDescriptors
        (
            Vk::MegaSet& megaSet,
            Vk::FramebufferView& framebufferView,
            Vk::FramebufferUsage usage
        );

        void FreeDescriptors
        (
            Vk::MegaSet& megaSet,
            const Vk::FramebufferView& framebufferView,
            Vk::FramebufferUsage usage
        );

        bool IsViewable(FramebufferImageType imageType);

        std::unordered_map<std::string, Framebuffer> m_framebuffers;
        std::map<std::string, FramebufferView>       m_framebufferViews;

        std::unordered_set<std::string> m_fixedFramebuffers;
    };
}

#endif
