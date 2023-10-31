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
            m_renderer.Render();
            // Poll events
            if (m_window->PollEvents()) break;
        }

        // Wait for the logical device to finish tasks
        m_renderer.WaitForLogicalDevice();
    }

    AppInstance::~AppInstance()
    {
        // Log
        LOG_INFO("{}\n", "App instance destroyed!");
    }
}