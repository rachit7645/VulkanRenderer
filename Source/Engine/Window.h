/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
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

#ifndef WINDOW_H
#define WINDOW_H

#include <SDL2/SDL.h>

#include "../Externals/GLM.h"

namespace Engine
{
    class Window
    {
    public:
        // Create window using SDL
        Window();
        // Destroy window
        ~Window();

        // Function to process SDL Events
        [[nodiscard]] bool PollEvents();
        // Sleeps till window is restored
        void WaitForRestoration();

        // SDL window handle
        SDL_Window* handle = nullptr;
        // Window size
        glm::ivec2 size = {1280, 768};
    private:
        // SDL event
        SDL_Event m_event = {};
        // Is input captured?
        bool m_isInputCaptured = true;
    };
}

#endif
