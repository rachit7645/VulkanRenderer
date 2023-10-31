#ifndef EXTENSIONS_H
#define EXTENSIONS_H

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
        void LoadInstanceFunctions(VkInstance instance);
        // Get extensions for device
        [[nodiscard]] bool CheckDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& requiredExtensions);
        // Load device functions
        void LoadDeviceFunctions(VkDevice device);
    private:
        // Load loader
        void LoadProcLoader();
    };
}

#endif
