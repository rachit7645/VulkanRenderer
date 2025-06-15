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

#ifndef SAMPLER_H
#define SAMPLER_H

#include <vulkan/vulkan.h>

#include "DescriptorAllocator.h"
#include "Util/Hash.h"

namespace Vk
{
    struct Sampler
    {
        void Destroy(VkDevice device) const;

        VkSampler        handle       = VK_NULL_HANDLE;
        Vk::DescriptorID descriptorID = 0;
    };
}

template <>
struct std::hash<VkSamplerCreateInfo>
{
    std::size_t operator()(const VkSamplerCreateInfo& sci) const noexcept
    {
        std::size_t hash = 0;

        hash = Util::HashCombine(hash, sci.pNext);
        hash = Util::HashCombine(hash, sci.flags);
        hash = Util::HashCombine(hash, sci.magFilter);
        hash = Util::HashCombine(hash, sci.minFilter);
        hash = Util::HashCombine(hash, sci.mipmapMode);
        hash = Util::HashCombine(hash, sci.addressModeU);
        hash = Util::HashCombine(hash, sci.addressModeV);
        hash = Util::HashCombine(hash, sci.addressModeW);
        hash = Util::HashCombine(hash, sci.mipLodBias);
        hash = Util::HashCombine(hash, sci.anisotropyEnable);
        hash = Util::HashCombine(hash, sci.maxAnisotropy);
        hash = Util::HashCombine(hash, sci.compareEnable);
        hash = Util::HashCombine(hash, sci.compareOp);
        hash = Util::HashCombine(hash, sci.minLod);
        hash = Util::HashCombine(hash, sci.maxLod);
        hash = Util::HashCombine(hash, sci.borderColor);
        hash = Util::HashCombine(hash, sci.unnormalizedCoordinates);

        return hash;
    }
};

#endif