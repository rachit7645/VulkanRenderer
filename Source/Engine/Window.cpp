/*
 *    Copyright 2023 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <SDL_vulkan.h>
#include "Window.h"

#include "Util/Log.h"
#include "Util/Files.h"
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
        Logger::Info
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
            Logger::Error("SDL_Init Failed: {}\n", SDL_GetError());
        }

        // Load vulkan library
        if (SDL_Vulkan_LoadLibrary(nullptr) != 0)
        {
            // Log error
            Logger::Error("SDL_Vulkan_LoadLibrary Failed: {}\n", SDL_GetError());
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
            Logger::Error("SDL_CreateWindow Failed\n{}\n", SDL_GetError());
        }

        // Log handle address
        Logger::Info("Succesfully created window handle! [handle={}]\n", reinterpret_cast<void*>(handle));

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

            // Default event handler
            default:
                continue;
            }
        }

        // Return
        return false;
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
        Logger::Info("{}\n", "Window destroyed!");
    }
}