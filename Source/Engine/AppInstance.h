#ifndef APP_INSTANCE_H
#define APP_INSTANCE_H

#include <memory>

#include "Window.h"
#include "Vulkan/Context.h"
#include "Renderer/RenderManager.h"

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
        // Draws the frame
        void DrawFrame();

        // Window
        std::shared_ptr<Engine::Window> m_window = nullptr;
        // Renderer
        Renderer::RenderManager m_renderer;
    };
}

#endif
