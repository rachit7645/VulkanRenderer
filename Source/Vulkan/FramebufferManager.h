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

namespace Vk
{
    class FramebufferManager;

    using ResizeCallback = std::function<void(const VkExtent2D&, FramebufferManager&)>;

    enum class FramebufferType : u8
    {
        ColorLDR,
        ColorHDR,
        Depth,
        DepthStencil
    };

    enum class ImageType : u8
    {
        Single2D,
        Array2D,
        Cube,
        ArrayCube
    };

    struct FramebufferSize
    {
        u32 width       = 0;
        u32 height      = 0;
        u32 mipLevels   = 1;
        u32 arrayLayers = 1;
    };

    struct FramebufferViewSize
    {
        u32 baseMipLevel   = 0;
        u32 levelCount     = 1;
        u32 baseArrayLayer = 0;
        u32 layerCount     = 1;
    };

    struct FramebufferView
    {
        std::string         framebuffer     = {};
        u32                 descriptorIndex = 0;
        ImageType           type            = ImageType::Single2D;
        FramebufferViewSize size            = {};
        Vk::ImageView       view            = {};
    };

    struct Framebuffer
    {
        FramebufferType type      = FramebufferType::ColorLDR;
        FramebufferSize size      = {};
        ImageType       imageType = ImageType::Single2D;
        Vk::Image       image     = {};
        ResizeCallback  OnResize  = nullptr;
    };

    class FramebufferManager
    {
    public:
        void AddFramebuffer
        (
            const std::string_view name,
            FramebufferType type,
            ImageType imageType,
            const ResizeCallback& resizeCallback = nullptr
        );

        void AddFramebufferView
        (
            const std::string_view framebufferName,
            const std::string_view name,
            ImageType imageType,
            const FramebufferViewSize& size = FramebufferViewSize{}
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

        void DeleteFramebufferViews(const std::string_view framebufferName, VkDevice device);

        void ImGuiDisplay();
        void Destroy(VkDevice device, VmaAllocator allocator);
    private:
        bool IsViewable(FramebufferType type, ImageType imageType);

        std::unordered_map<std::string, Framebuffer> m_framebuffers;
        std::map<std::string, FramebufferView>       m_framebufferViews;
    };
}

#endif
