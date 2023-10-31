#include <SDL_vulkan.h>
#include "Window.h"

#include "Util/Log.h"
#include "Renderer/RenderConstants.h"

namespace Engine
{
    // Window flags
    constexpr u32 SDL_WINDOW_FLAGS = SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;

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
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
        {
            // Log error
            LOG_ERROR("SDL_Init Failed: {}\n", SDL_GetError());
        }

        // Load vulkan library
        if (SDL_Vulkan_LoadLibrary(nullptr) != 0)
        {
            // Log error
            LOG_ERROR("SDL_Vulkan_LoadLibrary Failed: {}\n", SDL_GetError());
        }

        // Create handle
        handle = SDL_CreateWindow
        (
            "Rachit's Engine",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            size.x,
            size.y,
            SDL_WINDOW_FLAGS
        );

        // If handle creation failed
        if (handle == nullptr)
        {
            // Log error
            LOG_ERROR("SDL_CreateWindow Failed\n{}\n", SDL_GetError());
        }

        // Log handle address
        LOG_INFO("Succesfully created window handle! [handle={}]\n", reinterpret_cast<void*>(handle));

        // For sanity, raise handle
        SDL_RaiseWindow(handle);
        SDL_SetWindowMinimumSize(handle, 1, 1);

        // FIXME: This needs to be done here for now
        Files::GetInstance().SetResources("Assets/");
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

            case SDL_WINDOWEVENT:
                switch (m_event.display.event)
                {
                // Resize event
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                case SDL_WINDOWEVENT_RESIZED:
                    // Set flag
                    wasResized = true;
                    // Set size
                    ScaleWindowSize();
                    break;
                }
                break;    

            // Default event handler
            default:
                continue;
            }
        }

        // Return
        return false;
    }

    void Window::ScaleWindowSize()
    {
        // New size
        size = {m_event.window.data1, m_event.window.data2};
        // Ratio
        auto ratio = static_cast<f32>(size.x) / static_cast<f32>(size.y);
        // If size is in right ratio exit
        if (std::abs(ratio - Renderer::ASPECT_RATIO) < 0.01f) return;

        // Get width
        auto width = size.x;
        // Get height from width
        auto height = static_cast<s32>(std::round(static_cast<f32>(width) / Renderer::ASPECT_RATIO));

        // Set size
        size = {width, height};
        SDL_SetWindowSize(handle, size.x, size.y);

        // Log
        LOG_INFO("Resizing! [size=[{}, {}]]\n", size.x, size.y);
    }

    void Window::WaitForRestoration()
    {
        // Loop
        while (true)
        {
            // Poll events (LEAKS MEMORY WHEN EXITING)
            if (PollEvents()) std::exit(-1);
            // Check if not minimised
            if (!(SDL_GetWindowFlags(handle) & SDL_WINDOW_MINIMIZED)) break;
        }
    }

    Window::~Window()
    {
        // CLose SDL
        SDL_Vulkan_UnloadLibrary();
        SDL_DestroyWindow(handle);
        SDL_Quit();
        // Log
        LOG_INFO("{}\n", "Window destroyed!");
    }
}