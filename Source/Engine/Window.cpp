#include "Window.h"

#include "../Util/Log.h"
#include "../Util/Util.h"

namespace Engine
{
    // Window flags
    constexpr u32 SDL_WINDOW_FLAGS = SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN;

    Window::Window()
    {
        // Get SDL version
        SDL_version version = {};
        SDL_GetVersion(&version);

        // Log it
        LOG_INFO
        (
            "Initializing SDL2 version: {}.{}.{}\n",
            static_cast<usize>(version.major),
            static_cast<usize>(version.minor),
            static_cast<usize>(version.patch)
        );

        // Initialise SDL2
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            // Log error
            LOG_ERROR("SDL_Init Failed\n{}\n", SDL_GetError());
        }

        // Create handle
        handle = SDL_CreateWindow
        (
            "Rachit's Engine",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            m_size.x,
            m_size.y,
            SDL_WINDOW_FLAGS
        );

        // If handle creation failed
        if (handle == nullptr)
        {
            // Log error
            LOG_ERROR("SDL_CreateWindow Failed\n{}\n", SDL_GetError());
        }

        // Log handle address
        LOG_DEBUG("Succesfully created window handle! [handle={}]\n", reinterpret_cast<void*>(handle));

        // For sanity, raise handle
        SDL_RaiseWindow(handle);
    }

    bool Window::PollEvents()
    {
        // While events exist
        while (SDL_PollEvent(&m_event))
        {
            // Check event type
            switch (m_event.type)
            {
                // Event to quit
                case SDL_QUIT:
                    return true;

                // Default event handler
                default:
                    continue;
            }
        }

        // Return
        return false;
    }

    Window::~Window()
    {
        // CLose SDL
        SDL_DestroyWindow(handle);
        SDL_Quit();
        // Log
        LOG_DEBUG("{}\n", "Window destroyed!");
    }
}