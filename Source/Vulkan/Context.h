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

#ifndef VK_CONTEXT_H
#define VK_CONTEXT_H

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

#include "ValidationLayers.h"
#include "QueueFamilyIndices.h"
#include "Util/Util.h"
#include "Util/DeletionQueue.h"
#include "Externals/VMA.h"

namespace Vk
{
    class Context
    {
    public:
        explicit Context(SDL_Window* window);
        void Destroy();

        // Vulkan instance
        VkInstance instance = VK_NULL_HANDLE;

        // Physical device
        VkPhysicalDevice                                physicalDevice               = VK_NULL_HANDLE;
        VkPhysicalDeviceLimits                          physicalDeviceLimits         = {};
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties = {};
        // Logical device
        VkDevice device = VK_NULL_HANDLE;

        // Surface
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        // Queues
        Vk::QueueFamilyIndices queueFamilies;
        VkQueue                graphicsQueue = VK_NULL_HANDLE;

        // Pools
        VkCommandPool commandPool = VK_NULL_HANDLE;

        // Memory allocator
        VmaAllocator allocator = VK_NULL_HANDLE;
    private:
        void CreateInstance();
        void CreateSurface(SDL_Window* window);

        void PickPhysicalDevice();
        void CreateLogicalDevice();

        [[nodiscard]] usize CalculateScore
        (
            VkPhysicalDevice phyDevice,
            const VkPhysicalDeviceProperties2& propertySet,
            const VkPhysicalDeviceFeatures2& featureSet
        );

        void CreateCommandPool();
        void CreateAllocator();

        void AddDebugNames();

        #ifdef ENGINE_ENABLE_VALIDATION
        // Vulkan validation layers
        Vk::ValidationLayers m_layers;
        #endif

        // Deletion queue
        Util::DeletionQueue m_deletionQueue = {};
    };
}

#endif