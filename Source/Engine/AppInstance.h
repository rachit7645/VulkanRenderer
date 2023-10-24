#ifndef APP_INSTANCE_H
#define APP_INSTANCE_H

#include <memory>

#include "Window.h"
#include "../Vulkan/Context.h"

namespace Engine
{
    class AppInstance
    {
    public:
        // Create an instance of the application
        AppInstance();
        // Destroy application instance
        ~AppInstance();

        // Run application
        void Run();
    private:
        // Window
        std::unique_ptr<Engine::Window> m_window = nullptr;
        // Vulkan context
        std::unique_ptr<Vk::Context> m_vkContext = nullptr;
    };
}

#endif
