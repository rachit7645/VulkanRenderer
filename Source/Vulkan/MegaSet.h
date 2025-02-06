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

#include "DescriptorSet.h"
#include "DescriptorWriter.h"
#include "Sampler.h"
#include "ImageView.h"

namespace Vk
{
    class MegaSet
    {
    public:
        enum DescriptorBinding : u32
        {
            SAMPLER_BINDING          = 0,
            SAMPLED_IMAGES_BINDING   = 1,
            SAMPLED_CUBEMAPS_BINDING = 2,
            BINDINGS_COUNT
        };

        MegaSet(VkDevice device, const VkPhysicalDeviceLimits& deviceLimits);

        [[nodiscard]] u32 WriteSampler(const Vk::Sampler& sampler);
        [[nodiscard]] u32 WriteImage(const Vk::ImageView& imageView, VkImageLayout layout);
        [[nodiscard]] u32 WriteCubemap(const Vk::ImageView& imageView, VkImageLayout layout);

        void Update(VkDevice device);

        void Destroy(VkDevice device);

        Vk::DescriptorSet descriptorSet;
    private:
        VkDescriptorPool     m_descriptorPool = VK_NULL_HANDLE;
        Vk::DescriptorWriter m_writer         = {};

        u32 m_samplerID = 0;
        u32 m_imageID   = 0;
        u32 m_cubemapID = 0;
    };
}

#endif
