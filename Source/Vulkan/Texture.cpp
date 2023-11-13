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

#include "Texture.h"
#include "Util/Log.h"
#include "Buffer.h"

namespace Vk
{
    Texture::Texture(const std::shared_ptr<Vk::Context>& context, const std::string_view path)
    {
        // Load image
        auto imageData = STB::STBImage(path, STBI_rgb_alpha);
        // Image size
        VkDeviceSize imageSize = imageData.width * imageData.height * STBI_rgb_alpha;

        // Create staging buffer
        auto stagingBuffer = Vk::Buffer
        (
            context,
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        // Copy data
        stagingBuffer.LoadData
        (
            context->device,
            std::span(static_cast<const stbi_uc*>(imageData.data), imageSize / sizeof(stbi_uc))
        );

        // Create image
        image = Vk::Image
        (
            context,
            imageData.width,
            imageData.height,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        // Transition layout for transfer
        image.TransitionLayout(context, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        // Copy data
        image.CopyFromBuffer(context, stagingBuffer);
        // Transition layout for shader sampling
        image.TransitionLayout(context, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Destroy staging buffer
        stagingBuffer.DeleteBuffer(context->device);

        // Create image view
        imageView = Vk::ImageView
        (
            context->device,
            image,
            VK_IMAGE_VIEW_TYPE_2D,
            image.format,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    }

    void Texture::Destroy(VkDevice device)
    {
        // Destroy image view
        imageView.Destroy(device);
        // Destroy image
        image.Destroy(device);
    }
}