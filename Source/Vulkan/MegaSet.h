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

#ifndef MEGA_SET_H
#define MEGA_SET_H

#include <vulkan/vulkan.h>

#include "DescriptorWriter.h"
#include "DescriptorAllocator.h"
#include "Sampler.h"
#include "ImageView.h"
#include "Context.h"

namespace Vk
{
    class MegaSet
    {
    public:
        enum DescriptorBinding : u32
        {
            MEGA_SET_SAMPLER_BINDING        = 0,
            MEGA_SET_SAMPLED_IMAGES_BINDING = 1,
            MEGA_SET_STORAGE_IMAGES_BINDING = 2,
            MEGA_SET_BINDINGS_COUNT
        };

        explicit MegaSet(const Vk::Context& context);

        [[nodiscard]] u32 WriteSampler(const Vk::Sampler& sampler);
        [[nodiscard]] u32 WriteSampledImage(const Vk::ImageView& imageView, VkImageLayout layout);
        [[nodiscard]] u32 WriteStorageImage(const Vk::ImageView& imageView);

        void FreeSampler(u32 id);
        void FreeSampledImage(u32 id);
        void FreeStorageImage(u32 id);

        void Update(VkDevice device);

        void ImGuiDisplay();

        void Destroy(VkDevice device);

        VkDescriptorSet       descriptorSet    = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    private:
        VkDescriptorPool     m_descriptorPool = VK_NULL_HANDLE;
        Vk::DescriptorWriter m_writer         = {};

        Vk::DescriptorAllocator m_samplerAllocator      = {};
        Vk::DescriptorAllocator m_sampledImageAllocator = {};
        Vk::DescriptorAllocator m_storageImageAllocator = {};
    };
}

#endif
