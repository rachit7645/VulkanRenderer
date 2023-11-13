/*
 *    Copyright 2023 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef SAMPLER_H
#define SAMPLER_H

#include <utility>
#include <vulkan/vulkan.h>

#include "Util/Util.h"

namespace Vk
{
    class Sampler
    {
    public:
        // Default constructor
        Sampler() = default;
        // Create a sampler
        Sampler
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
        );
        // Destroy sampler
        void Destroy(VkDevice device);
        // Vulkan handle
        VkSampler handle = nullptr;
    };
}

#endif
