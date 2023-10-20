#include "AppInstance.h"

#include "../Util/Log.h"

namespace Engine
{
    AppInstance::AppInstance()
        : m_window(std::make_unique<Window>())
    {
        // Log
        LOG_DEBUG("{}\n", "App instance initialised!");
    }

    AppInstance::~AppInstance()
    {
        // Log
        LOG_DEBUG("{}\n", "App instance destroyed!");
    }

    void AppInstance::Run()
    {
        while (true)
        {
            // Poll events
            if (m_window->PollEvents()) break;
        }
    }
}