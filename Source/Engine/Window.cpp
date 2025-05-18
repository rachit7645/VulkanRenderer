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
        Logger::Debug
        (
            "Initializing SDL3! [Version = {}.{}.{}]\n",
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

        if (!SDL_RaiseWindow(handle))
        {
            Logger::Error("SDL_RaiseWindow Failed: {}\n", SDL_GetError());
        }

        if (!SDL_SetWindowRelativeMouseMode(handle, true))
        {
            Logger::Error("SDL_SetWindowRelativeMouseMode Failed: {}\n", SDL_GetError());
        }

        inputs = Inputs(true);
    }

    Window::~Window()
    {
        inputs.Destroy();

        SDL_DestroyWindow(handle);
        SDL_Quit();
    }
}