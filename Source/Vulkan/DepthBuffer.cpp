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

#include "Util/Log.h"

namespace Vk
{
    DepthBuffer::DepthBuffer(const std::shared_ptr<Vk::Context>& context, VkExtent2D swapchainExtent)
    {
        // Get depth format
        auto depthFormat = GetDepthFormat(context->physicalDevice);
        // Check if it has stencil
        bool hasStencil = HasStencilComponent(depthFormat);

        // Create image
        depthImage = Vk::Image
        (
            context,
            swapchainExtent.width,
            swapchainExtent.height,
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
            static_cast<VkImageAspectFlagBits>(depthImage.aspect)
        );

        // Transition (optional)
        depthImage.TransitionLayout
        (
            context,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        );
    }

    VkFormat DepthBuffer::FindSupportedFormat
    (
        VkPhysicalDevice physicalDevice,
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    )
    {
        // Loop over formats
        for (auto format : candidates)
        {
            // Get format properties
            VkFormatProperties properties = {};
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);

            // Linear tiling
            bool isValidLinear = tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features;
            // Optimal tiling
            bool isValidOptimal = tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features;

            // Return if valid
            if (isValidLinear || isValidOptimal)
            {
                // Found valid format
                return format;
            }
        }

        // Throw error if no format was suitable
        Logger::Error
        (
            "Failed to find valid depth format! [physicalDevice={}] [tiling={}] [features={}]\n",
            reinterpret_cast<void*>(physicalDevice),
            static_cast<s32>(tiling),
            features
        );
    }

    VkFormat DepthBuffer::GetDepthFormat(VkPhysicalDevice physicalDevice)
    {
        // Return
        return FindSupportedFormat
        (
            physicalDevice,
            {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    bool DepthBuffer::HasStencilComponent(VkFormat format)
    {
        // Return
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    void DepthBuffer::Destroy(VkDevice device)
    {
        // Destroy image view
        depthImageView.Destroy(device);
        // Destroy image
        depthImage.Destroy(device);
    }
}