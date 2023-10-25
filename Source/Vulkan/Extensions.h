#ifndef EXTENSION_LOADER_H
#define EXTENSION_LOADER_H

#include <string>
#include <vulkan/vulkan.h>

namespace Vk
{
    class Extensions
    {
    public:
        // Destructor
        ~Extensions();
        // Load extensions for instance
        std::vector<const char*> LoadInstanceExtensions(SDL_Window* window);
        // Load functions
        void LoadFunctions(VkInstance& instance);
    };
}

#endif
