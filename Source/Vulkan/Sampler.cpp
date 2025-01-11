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

#include "Sampler.h"
#include "Util/Log.h"
#include "Util.h"

namespace Vk
{
    Sampler::Sampler
    (
        VkDevice device,
        std::array<VkFilter, 2> filters,
        VkSamplerMipmapMode mipmapMode,
        std::array<VkSamplerAddressMode, 3> addressModes,
        f32 mipLodBias,
        std::pair<VkBool32, f32> anisotropy,
        std::pair<VkBool32, VkCompareOp> compare,
        std::array<f32, 2> lod,
        VkBorderColor borderColor,
        VkBool32 unnormalizedCoordinates
    )
    {
        VkSamplerCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .magFilter               = filters[1],
            .minFilter               = filters[0],
            .mipmapMode              = mipmapMode,
            .addressModeU            = addressModes[0],
            .addressModeV            = addressModes[1],
            .addressModeW            = addressModes[2],
            .mipLodBias              = mipLodBias,
            .anisotropyEnable        = anisotropy.first,
            .maxAnisotropy           = anisotropy.second,
            .compareEnable           = compare.first,
            .compareOp               = compare.second,
            .minLod                  = lod[0],
            .maxLod                  = lod[1],
            .borderColor             = borderColor,
            .unnormalizedCoordinates = unnormalizedCoordinates
        };

        Vk::CheckResult(vkCreateSampler(
            device,
            &createInfo,
            nullptr,
            &handle),
            "Failed to create sampler!"
        );

        Logger::Debug("Created sampler! [handle={}]\n", std::bit_cast<void*>(handle));
    }

    void Sampler::Destroy(VkDevice device) const
    {
        Logger::Debug("Destroying sampler! [handle={}]\n", std::bit_cast<void*>(handle));
        vkDestroySampler(device, handle, nullptr);
    }
}