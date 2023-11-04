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

#ifndef INPUTS_H
#define INPUTS_H

#include <SDL2/SDL.h>

#include "Util.h"
#include "../Externals/GLM.h"

namespace Engine
{
    class Inputs
    {
    private:
        // Main constructor
        Inputs();
    public:
        // Get mouse position
        glm::ivec2& GetMousePos();
        // Get mouse scroll
        glm::ivec2& GetMouseScroll();
        // Check if key is pressed
        bool IsKeyPressed(SDL_Scancode key);
    private:
        // Mouse Position
        glm::ivec2 m_mousePos = {0, 0};
        // Mouse scroll
        glm::ivec2 m_mouseScroll = {0, 0};
        // Key array
        const u8* m_keys = nullptr;
    public:
        // Get instance
        static Inputs& GetInstance();
    };
}

#endif