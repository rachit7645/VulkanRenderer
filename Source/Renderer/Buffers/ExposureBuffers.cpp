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

#include "ExposureBuffers.h"

#include "Exposure/Common.h"
#include "Vulkan/DebugUtils.h"
#include "Vulkan/Constants.h"

namespace Renderer::Buffers
{
    ExposureBuffers::ExposureBuffers(VkDevice device, VmaAllocator allocator)
    {
        histogramBuffer = Vk::Buffer
        (
            device,
            allocator,
            Exposure::HISTOGRAM_SIZE * sizeof(u32),
            0,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        luminanceBuffer = Vk::Buffer
        (
            device,
            allocator,
            Vk::FRAMES_IN_FLIGHT * sizeof(f32),
            0,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        Vk::SetDebugName(device, histogramBuffer.handle, "Exposure/HistogramBuffer");
        Vk::SetDebugName(device, luminanceBuffer.handle, "Exposure/LuminanceBuffer");
    }

    void ExposureBuffers::Destroy(VmaAllocator allocator)
    {
        histogramBuffer.Destroy(allocator);
        luminanceBuffer.Destroy(allocator);
    }
}
