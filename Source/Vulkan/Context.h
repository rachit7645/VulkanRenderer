#ifndef VK_CONTEXT_H
#define VK_CONTEXT_H

#include <memory>
#include <array>
#include <vector>
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "ValidationLayers.h"
#include "Extensions.h"
#include "QueueFamilyIndices.h"
#include "SwapChainInfo.h"
#include "Util/Util.h"
#include "Engine/Window.h"

namespace Vk
{
    // Maximum frames in flight at a time
    constexpr usize MAX_FRAMES_IN_FLIGHT = 2;

    class Context
    {
    public:
        // Initialise vulkan context
        explicit Context(SDL_Window* window);
        // Destroy vulkan context
        ~Context();

        // Recreate swap chain
        void RecreateSwapChain(const std::shared_ptr<Engine::Window>& window);

        // Vulkan instance
        VkInstance vkInstance = {};
        // Logical device
        VkDevice device = {};
        // Queue
        VkQueue graphicsQueue = {};

        // Swap chain
        VkSwapchainKHR swapChain = {};
        // Extent
        VkExtent2D swapChainExtent = {};
        // Swap chain framebuffers
        std::vector<VkFramebuffer> swapChainFrameBuffers = {};

        // Render pass
        VkRenderPass renderPass = {};
        // Command buffer
        std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers = {};

        // Semaphores
        std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores = {};
        std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores = {};
        // Fences
        std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences = {};

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

        // Create swap
        void CreateSwapChain(SDL_Window* window);
        // Create image views
        void CreateImageViews();

        // Choose surface format
        [[nodiscard]] VkSurfaceFormatKHR ChooseSurfaceFormat(const Vk::SwapChainInfo& swapChainInfo);
        // Choose surface presentation mode
        [[nodiscard]] VkPresentModeKHR ChoosePresentationMode(const Vk::SwapChainInfo& swapChainInfo);
        // Choose swap extent
        [[nodiscard]] VkExtent2D ChooseSwapExtent(SDL_Window* window, const Vk::SwapChainInfo& swapChainInfo);

        // Creates the default render pass
        void CreateRenderPass();
        // Create swap chain framebuffers
        void CreateFramebuffers();

        // Creates command pool
        void CreateCommandPool();
        // Create graphics command buffer
        void CreateCommandBuffers();
        // Create synchronisation objects
        void CreateSyncObjects();

        // Destroy current swap chain
        void DestroySwapChain();

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

        // Swap chain images
        std::vector<VkImage> m_swapChainImages = {};
        // Image format
        VkFormat m_swapChainImageFormat = {};
        // Swap chain image views
        std::vector<VkImageView> m_swapChainImageViews = {};

        // Command pool
        VkCommandPool m_commandPool = {};
    };
}

#endif