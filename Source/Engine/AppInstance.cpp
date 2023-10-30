#include "AppInstance.h"

#include "../Util/Log.h"

namespace Engine
{
    AppInstance::AppInstance()
        : m_window(std::make_shared<Window>()),
          m_renderer(m_window)
    {
        // Log
        LOG_INFO("{}\n", "App instance initialised!");
    }

    void AppInstance::Run()
    {
        while (true)
        {
            // Render
            DrawFrame();
            // Poll events
            if (m_window->PollEvents()) break;
        }

        // Wait for the logical device to finish tasks
        m_renderer.WaitForLogicalDevice();
    }

    void AppInstance::DrawFrame()
    {
        // Begin
        m_renderer.BeginFrame();
        // Render
        m_renderer.Render();
        // End
        m_renderer.EndFrame();
        // Present
        m_renderer.Present();
    }

    AppInstance::~AppInstance()
    {
        // Log
        LOG_INFO("{}\n", "App instance destroyed!");
    }
}