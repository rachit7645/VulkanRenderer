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

#include "DepthBuffer.h"

#include <vulkan/utility/vk_format_utils.h>

#include "Util.h"
#include "Util/Log.h"

namespace Vk
{
    DepthBuffer::DepthBuffer(const std::shared_ptr<Vk::Context>& context, VkExtent2D swapchainExtent)
    {
        // Get depth format
        auto depthFormat = GetDepthFormat(context->physicalDevice);
        // Check if it has stencil
        bool hasStencil = vkuFormatHasStencil(depthFormat);

        // Create image
        depthImage = Vk::Image
        (
            context,
            swapchainExtent.width,
            swapchainExtent.height,
            1,
            depthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            hasStencil ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        // Create image view
        depthImageView = Vk::ImageView
        (
            context->device,
            depthImage,
            VK_IMAGE_VIEW_TYPE_2D,
            depthImage.format,
            static_cast<VkImageAspectFlagBits>(depthImage.aspect),
            0,
            1,
            0,
            1
        );

        // Transition to depth layout (optional)
        Vk::ImmediateSubmit(context, [&](const Vk::CommandBuffer& cmdBuffer)
        {
            depthImage.TransitionLayout
            (
                cmdBuffer,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            );
        });
    }

    VkFormat DepthBuffer::GetDepthFormat(VkPhysicalDevice physicalDevice)
    {
        // Return
        return Vk::FindSupportedFormat
        (
            physicalDevice,
            std::array<VkFormat, 3>{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    void DepthBuffer::Destroy(VkDevice device, VmaAllocator allocator) const
    {
        // Destroy image view
        depthImageView.Destroy(device);
        // Destroy image
        depthImage.Destroy(allocator);
    }
}