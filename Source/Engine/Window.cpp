/*
 * Copyright (c) 2023 - 2025 Rachit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <SDL_vulkan.h>
#include "Window.h"

#include <thread>

#include "Util/Log.h"
#include "Util/Files.h"
#include "Inputs.h"

namespace Engine
{
    Window::Window()
    {
        SDL_version version = {};
        SDL_GetVersion(&version);

        Logger::Info
        (
            "Initializing SDL2 version: {}.{}.{}\n",
            static_cast<usize>(version.major),
            static_cast<usize>(version.minor),
            static_cast<usize>(version.patch)
        );

        constexpr u32 SDL_INIT_FLAGS = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER;
        if (SDL_Init(SDL_INIT_FLAGS) != 0)
        {
            Logger::Error("SDL_Init Failed: {}\n", SDL_GetError());
        }

        if (SDL_Vulkan_LoadLibrary(nullptr) != 0)
        {
            Logger::Error("SDL_Vulkan_LoadLibrary Failed: {}\n", SDL_GetError());
        }

        constexpr u32 SDL_WINDOW_FLAGS = SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
        handle = SDL_CreateWindow
        (
            "Rachit's Engine: Vulkan Edition",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            size.x,
            size.y,
            SDL_WINDOW_FLAGS
        );

        if (handle == nullptr)
        {
            Logger::Error("SDL_CreateWindow Failed\n{}\n", SDL_GetError());
        }

        Logger::Info("Succesfully created window handle! [handle={}]\n", std::bit_cast<void*>(handle));

        SDL_RaiseWindow(handle);
        SDL_SetWindowMinimumSize(handle, 1, 1);

        SDL_ShowCursor(SDL_FALSE);
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    bool Window::PollEvents()
    {
        auto& inputs = Inputs::Get();

        while (SDL_PollEvent(&m_event))
        {
            ImGui_ImplSDL2_ProcessEvent(&m_event);

            switch (m_event.type)
            {
            case SDL_QUIT:
                return true;

            case SDL_KEYDOWN:
                switch (m_event.key.keysym.scancode)
                {
                case SDL_SCANCODE_F1:
                    SDL_SetRelativeMouseMode(static_cast<SDL_bool>(!m_isInputCaptured));
                    m_isInputCaptured = !m_isInputCaptured;
                    break;

                default:
                    break;
                }
                break;

            case SDL_MOUSEMOTION:
                inputs.SetMousePosition(glm::ivec2(m_event.motion.xrel, m_event.motion.yrel));
                break;

            case SDL_MOUSEWHEEL:
                inputs.SetMouseScroll(glm::ivec2(m_event.wheel.x, m_event.wheel.y));
                break;

            case SDL_CONTROLLERDEVICEADDED:
                if (inputs.GetController() == nullptr)
                {
                    inputs.FindController();
                }
                break;

            case SDL_CONTROLLERDEVICEREMOVED:
                if (inputs.GetController() == nullptr && m_event.cdevice.which == inputs.GetControllerID())
                {
                    SDL_GameControllerClose(inputs.GetController());
                    inputs.FindController();
                }
                break;

            default:
                continue;
            }
        }

        return false;
    }

    // FIXME: This is quite hacky
    void Window::WaitForRestoration()
    {
        while (true)
        {
            if (PollEvents())
            {
                std::exit(-1);
            }

            if (!(SDL_GetWindowFlags(handle) & SDL_WINDOW_MINIMIZED))
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(10000));
        }
    }

    Window::~Window()
    {
        Inputs::Get().Destroy();

        SDL_Vulkan_UnloadLibrary();
        SDL_DestroyWindow(handle);
        SDL_Quit();

        Logger::Info("{}\n", "Window destroyed!");
    }
}