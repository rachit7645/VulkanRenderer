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

#ifndef TEXTUREMANAGER_H
#define TEXTUREMANAGER_H

namespace Vk
{
    class TextureManager
    {
    public:
        TextureManager() = default;
        TextureManager(VkDevice device, const VkPhysicalDeviceLimits& deviceLimits);
        void Destroy(VkDevice device) const;

        VkDescriptorPool      texturePool   = VK_NULL_HANDLE;
        VkDescriptorSet       textureSet    = VK_NULL_HANDLE;
        VkDescriptorSetLayout textureLayout = VK_NULL_HANDLE;
    private:
        void CreatePool(VkDevice device);
        void CreateLayout(VkDevice device, const VkPhysicalDeviceLimits& physicalDeviceLimits);
        void CreateSet(VkDevice device, const VkPhysicalDeviceLimits& physicalDeviceLimits);
    };
}

#endif
