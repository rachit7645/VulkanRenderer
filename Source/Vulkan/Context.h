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

#ifndef VK_CONTEXT_H
#define VK_CONTEXT_H

#include <array>
#include <vector>
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>

#include "ValidationLayers.h"
#include "Extensions.h"
#include "QueueFamilyIndices.h"
#include "Constants.h"
#include "Util/Util.h"
#include "Engine/Window.h"
#include "Util/DeletionQueue.h"

namespace Vk
{
    class Context
    {
    public:
        // Initialise vulkan context
        explicit Context(const std::shared_ptr<Engine::Window>& window);
        // Destroy vulkan context
        void Destroy();

        // Allocates command buffer
        std::vector<VkCommandBuffer> AllocateCommandBuffers(u32 count, VkCommandBufferLevel level);
        // Free command buffers
        void FreeCommandBuffers(const std::span<const VkCommandBuffer> cmdBuffers);
        // Allocates descriptors
        std::vector<VkDescriptorSet> AllocateDescriptorSets(const std::span<VkDescriptorSetLayout> descriptorLayouts);

        // Vulkan instance
        VkInstance vkInstance = {};
        // Physical device (GPU)
        VkPhysicalDevice physicalDevice = {};
        // Physical device memory properties
        VkPhysicalDeviceMemoryProperties phyMemProperties = {};
        // Logical device
        VkDevice device = {};
        // Surface
        VkSurfaceKHR surface = {};
        // Queue
        VkQueue graphicsQueue = {};

        // Command buffers
        std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> commandBuffers = {};

        // Semaphores
        std::array<VkSemaphore, FRAMES_IN_FLIGHT> imageAvailableSemaphores = {};
        std::array<VkSemaphore, FRAMES_IN_FLIGHT> renderFinishedSemaphores = {};
        // Fences
        std::array<VkFence, FRAMES_IN_FLIGHT> inFlightFences = {};
    private:
        // Create vulkan instance
        void CreateVKInstance(SDL_Window* window);
        // Create platform dependent surface
        void CreateSurface(SDL_Window* window);

        // Pick a GPU
        void PickPhysicalDevice();
        // Calculate score
        [[nodiscard]] usize CalculateScore(VkPhysicalDevice logicalDevice, VkPhysicalDeviceProperties propertySet);
        // Create a logical device
        void CreateLogicalDevice();

        // Creates command pool
        void CreateCommandPool();
        // Create descriptor pool
        void CreateDescriptorPool();

        // Create graphics command buffer
        void CreateCommandBuffers();
        // Create synchronisation objects
        void CreateSyncObjects();

        // Extensions
        Vk::Extensions m_extensions = {};
        #ifdef ENGINE_DEBUG
        // Vulkan validation layers
        Vk::ValidationLayers m_layers = {};
        #endif

        // Queue families
        Vk::QueueFamilyIndices m_queueFamilies = {};
        // Command pool
        VkCommandPool m_commandPool = {};
        // Descriptor pool
        VkDescriptorPool m_descriptorPool = {};

        // Deletion queue
        Util::DeletionQueue m_deletionQueue = {};
    };
}

#endif