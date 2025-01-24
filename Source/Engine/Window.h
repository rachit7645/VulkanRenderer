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

#ifndef WINDOW_H
#define WINDOW_H

#include <SDL3/SDL.h>

#include "Externals/GLM.h"
#include "Vulkan/Swapchain.h"

namespace Engine
{
    class Window
    {
    public:
        Window();
        ~Window();

        // No copying
        Window(const Window&)            = delete;
        Window& operator=(const Window&) = delete;

        // Only moving
        Window(Window&& other)            = default;
        Window& operator=(Window&& other) = default;

        // SDL window handle
        SDL_Window* handle = nullptr;
        // Window size
        glm::ivec2 size = {1600, 900};
    private:
        // SDL event
        SDL_Event m_event = {};
    };
}

#endif
