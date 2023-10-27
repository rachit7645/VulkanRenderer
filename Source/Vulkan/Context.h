#ifndef VK_CONTEXT_H
#define VK_CONTEXT_H

#include <memory>
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "ValidationLayers.h"
#include "Extensions.h"
#include "QueueFamilyIndices.h"
#include "SwapChainInfo.h"
#include "../Util/Util.h"
#include "PipelineBuilder.h"

namespace Vk
{
    class Context
    {
    public:
        // Initialise vulkan context
        explicit Context(SDL_Window* window);
        // Destroy vulkan context
        ~Context();

        // Vulkan instance
        VkInstance vkInstance = {};
    private:
        // Create vulkan instance
        void CreateVKInstance(SDL_Window* window);
        // Create platform dependent surface
        void CreateSurface(SDL_Window* window);

        // Pick a GPU
        void PickPhysicalDevice();
        // Calculate score
        usize CalculateScore(VkPhysicalDevice device, VkPhysicalDeviceProperties propertySet);
        // Create a logical device
        void CreateLogicalDevice();

        // Create swap
        void CreateSwapChain(SDL_Window* window);
        // Create image views
        void CreateImageViews();

        // Choose surface format
        VkSurfaceFormatKHR ChooseSurfaceFormat(const Vk::SwapChainInfo& swapChainInfo);
        // Choose surface presentation mode
        VkPresentModeKHR ChoosePresentationMode(const Vk::SwapChainInfo& swapChainInfo);
        // Choose swap extent
        VkExtent2D ChooseSwapExtent(SDL_Window* window, const Vk::SwapChainInfo& swapChainInfo);

        // Create graphics pipeline
        void CreateGraphicsPipeline();

        // Extensions
        std::unique_ptr<Vk::Extensions> m_extensions = nullptr;
        #ifdef ENGINE_DEBUG
        // Vulkan validation layers
        std::unique_ptr<Vk::ValidationLayers> m_layers = nullptr;
        #endif
        // Surface
        VkSurfaceKHR m_surface = {};

        // Physical device (GPU)
        VkPhysicalDevice m_physicalDevice = {};
        // Queue families
        Vk::QueueFamilyIndices m_queueFamilies = {};
        // Logical device
        VkDevice m_device = {};
        // Queue
        VkQueue m_graphicsQueue = {};

        // Swap chain
        VkSwapchainKHR m_swapChain = {};
        // Swap chain images
        std::vector<VkImage> m_swapChainImages = {};
        // Image format
        VkFormat m_swapChainImageFormat = {};
        // Extent
        VkExtent2D m_swapChainExtent = {};
        // Swap chain image views
        std::vector<VkImageView> m_swapChainImageViews = {};
        // Pipeline object
        VkPipeline m_pipeline = {};
    };
}

#endif