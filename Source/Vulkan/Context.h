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

#ifndef VULKAN_CONTEXT_H
#define VULKAN_CONTEXT_H

#include <vulkan/vulkan.h>

#include "DebugCallback.h"
#include "QueueFamilies.h"
#include "Extensions.h"
#include "Properties.h"
#include "Util/DeletionQueue.h"
#include "Externals/VMA.h"
#include "Externals/SDL.h"

namespace Vk
{
    class Context
    {
    public:
        explicit Context(SDL_Window* window);

        void Destroy();

        VkInstance   instance = VK_NULL_HANDLE;
        VkSurfaceKHR surface  = VK_NULL_HANDLE;

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

        VkDevice device = VK_NULL_HANDLE;

        Vk::Properties    properties;
        Vk::QueueFamilies queueFamilies;
        Vk::Extensions    extensions;

        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue computeQueue  = VK_NULL_HANDLE;

        VmaAllocator allocator = VK_NULL_HANDLE;

        std::string physicalDeviceName = "Device/Null";
    private:
        void CreateInstance();
        void CreateSurface(SDL_Window* window);

        void PickPhysicalDevice();
        void CreateLogicalDevice();

        void CreateAllocator();

        void AddDebugNames() const;

        #ifdef ENGINE_DEBUG
        Vk::DebugCallback m_debugCallback;
        #endif

        Util::DeletionQueue m_deletionQueue = {};
    };
}

#endif