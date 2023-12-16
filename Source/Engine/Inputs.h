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

#include "Externals/GLM.h"
#include "Util/Util.h"

namespace Engine
{
    class Inputs
    {
    public:
        // Get instance
        static Inputs& GetInstance();

        // Set mouse position
        void SetMousePosition(const glm::ivec2& position);
        // Set mouse scroll
        void SetMouseScroll(const glm::ivec2& scroll);

        // Check if key is pressed
        [[nodiscard]] bool IsKeyPressed(SDL_Scancode key) const;
        // Get mouse position
        [[nodiscard]] const glm::ivec2& GetMousePosition();
        // Get mouse scroll
        [[nodiscard]] const glm::ivec2& GetMouseScroll();

        // Mouse position
        glm::ivec2 mousePosition = {};
        // Mouse scroll
        glm::ivec2 mouseScroll = {};

        // Flags
        bool wasMouseMoved    = false;
        bool wasMouseScrolled = false;
    private:
        // Main constructor
        Inputs();

        // Key array
        const u8* m_keys = nullptr;
    };
}

#endif