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

#include "Window.h"

#include <thread>
#include <SDL3/SDL_vulkan.h>

#include "Externals/ImGui.h"

#include "Inputs.h"
#include "Util/Log.h"

namespace Engine
{
    Window::Window()
    {
        Logger::Info
        (
            "Initializing SDL2 version: {}.{}.{}\n",
            SDL_MAJOR_VERSION,
            SDL_MINOR_VERSION,
            SDL_MICRO_VERSION
        );

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        {
            Logger::Error("SDL_Init Failed: {}\n", SDL_GetError());
        }

        handle = SDL_CreateWindow
        (
            "Rachit's Engine: Vulkan Edition",
            size.x,
            size.y,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN
        );

        if (handle == nullptr)
        {
            Logger::Error("SDL_CreateWindow Failed: {}\n", SDL_GetError());
        }

        Logger::Info("Succesfully created window handle! [handle={}]\n", std::bit_cast<void*>(handle));

        if (!SDL_RaiseWindow(handle))
        {
            Logger::Error("SDL_RaiseWindow Failed: {}\n", SDL_GetError());
        }

        if (!SDL_SetWindowRelativeMouseMode(handle, true))
        {
            Logger::Error("SDL_SetWindowRelativeMouseMode Failed: {}\n", SDL_GetError());
        }
    }

    bool Window::PollEvents()
    {
        auto& inputs = Inputs::Get();

        while (SDL_PollEvent(&m_event))
        {
            ImGui_ImplSDL3_ProcessEvent(&m_event);

            switch (m_event.type)
            {
            case SDL_EVENT_QUIT:
                return true;

            case SDL_EVENT_KEY_DOWN:
                switch (m_event.key.scancode)
                {
                case SDL_SCANCODE_F1:
                    if (!SDL_SetWindowRelativeMouseMode(handle, !SDL_GetWindowRelativeMouseMode(handle)))
                    {
                        Logger::Error("SDL_SetWindowRelativeMouseMode Failed: {}\n", SDL_GetError());
                    }
                    break;

                default:
                    break;
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                inputs.SetMousePosition(glm::vec2(m_event.motion.xrel, m_event.motion.yrel));
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                inputs.SetMouseScroll(glm::vec2(m_event.wheel.x, m_event.wheel.y));
                break;

            case SDL_EVENT_GAMEPAD_ADDED:
                if (inputs.GetGamepad() == nullptr)
                {
                    inputs.FindGamepad();
                }
                break;

            case SDL_EVENT_GAMEPAD_REMOVED:
                if (inputs.GetGamepad() == nullptr && m_event.gdevice.which == inputs.GetGamepadID())
                {
                    SDL_CloseGamepad(inputs.GetGamepad());
                    inputs.FindGamepad();
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

        SDL_DestroyWindow(handle);
        SDL_Quit();

        Logger::Info("{}\n", "Window destroyed!");
    }
}