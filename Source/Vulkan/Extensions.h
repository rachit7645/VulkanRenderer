#ifndef EXTENSION_LOADER_H
#define EXTENSION_LOADER_H

#include <vector>
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
        void LoadFunctions(VkInstance& instance);
        // Get extensions for device
        [[nodiscard]] bool CheckDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& requiredExtensions);
    };
}

#endif
