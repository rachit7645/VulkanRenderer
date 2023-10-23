#ifndef EXTENSION_LOADER_H
#define EXTENSION_LOADER_H

#include <string>
#include <vulkan/vulkan.h>

namespace Vk
{
    class Extensions
    {
    public:
        // Get singleton instance
        static Extensions& GetInstance();

        // Load extensions for SDL window
        std::vector<const char*> LoadExtensions(SDL_Window* window);
        // Load functions
        void LoadFunctions(VkInstance& instance);
        // Destroy
        void Destroy();

        // Function pointers
        PFN_vkGetInstanceProcAddr           p_vkGetInstanceProcAddr           = nullptr;
        PFN_vkCreateDebugUtilsMessengerEXT  p_vkCreateDebugUtilsMessengerEXT  = nullptr;
        PFN_vkDestroyDebugUtilsMessengerEXT p_vkDestroyDebugUtilsMessengerEXT = nullptr;
    private:
        // Constructor
        Extensions() = default;
        // Copy and move operations
        Extensions& operator=(const Extensions&) = default;
        Extensions& operator=(Extensions&&) = default;
    };
}

#endif
