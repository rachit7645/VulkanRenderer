//    Copyright 2023 - 2024 Rachit Khandelwal
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include "Texture.h"
#include "DescriptorWriter.h"
#include "Util/Util.h"

namespace Vk
{
    class TextureManager
    {
    public:
        struct TextureInfo
        {
            u32         textureID = 0;
            Vk::Texture texture;
        };

        TextureManager(VkDevice device, const VkPhysicalDeviceLimits& deviceLimits);

        [[nodiscard]] usize AddTexture(const Vk::Context& context, const std::string_view path, Texture::Flags flags);
        [[nodiscard]] usize AddTexture(const Vk::Texture& texture);
        void Update(VkDevice device);

        [[nodiscard]] u32 GetID(usize pathHash) const;
        [[nodiscard]] const Texture& GetTexture(usize pathHash) const;

        void Destroy(VkDevice device, VmaAllocator allocator);

        Vk::DescriptorSet                    textureSet;
        std::unordered_map<usize, TextureInfo> textureMap;
    private:
        void CreatePool(VkDevice device);
        void CreateLayout(VkDevice device, const VkPhysicalDeviceLimits& physicalDeviceLimits);
        void CreateSet(VkDevice device, const VkPhysicalDeviceLimits& physicalDeviceLimits);

        VkDescriptorPool     m_texturePool = VK_NULL_HANDLE;
        u32                  m_lastID      = 0;
        Vk::DescriptorWriter m_writer      = {};
    };
}

#endif
