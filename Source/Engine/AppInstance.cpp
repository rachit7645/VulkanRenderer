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
            // Begin
            m_renderer.BeginFrame();
            m_renderer.BeginRenderPass();
            // Render
            m_renderer.Render();
            // End
            m_renderer.EndRenderPass();
            m_renderer.EndFrame();

            // Poll events
            if (m_window->PollEvents()) break;
        }
    }

    AppInstance::~AppInstance()
    {
        // Log
        LOG_INFO("{}\n", "App instance destroyed!");
    }
}