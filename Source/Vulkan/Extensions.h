#ifndef EXTENSIONS_H
#define EXTENSIONS_H

#include <span>
#include <vulkan/vulkan.h>

namespace Vk
{
    class Extensions
    {
    public:
        // Destructor
        ~Extensions();
        // Load extensions for instance
        [[nodiscard]] std::vector<const char*> LoadInstanceExtensions(SDL_Window* window);
        // Load functions
        void LoadInstanceFunctions(VkInstance instance);
        // Get extensions for device
        [[nodiscard]] bool CheckDeviceExtensionSupport(VkPhysicalDevice device, const std::span<const char* const> requiredExtensions);
        // Load device functions
        void LoadDeviceFunctions(VkDevice device);
    private:
        // Load loader
        void LoadProcLoader();
    };
}

#endif
