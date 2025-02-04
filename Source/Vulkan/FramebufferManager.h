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
    class FramebufferManager
    {
    public:
        enum class FramebufferType : u8
        {
            None,
            ColorLDR,
            ColorHDR,
            Depth,
            DepthStencil,
            BRDF
        };

        struct Framebuffer
        {
            FramebufferType           type            = FramebufferType::None;
            std::optional<VkExtent2D> resolution      = std::nullopt;
            u32                       descriptorIndex = 0;
            Vk::Image                 image           = {};
            Vk::ImageView             imageView       = {};
        };

        void AddFramebuffer
        (
            FramebufferType type,
            const std::string_view name,
            std::optional<VkExtent2D> resolution = std::nullopt
        );

        void Update
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Vk::MegaSet& megaSet,
            VkExtent2D swapchainExtent
        );

        [[nodiscard]] const Framebuffer& GetFramebuffer(const std::string_view name) const;

        void ImGuiDisplay();
        void Destroy(VkDevice device, VmaAllocator allocator);
    private:
        bool IsViewable(FramebufferType type);

        std::unordered_map<std::string, Framebuffer> m_framebuffers;
    };
}

#endif
