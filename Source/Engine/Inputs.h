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
        // Find controller
        void FindController();

        // Check if key is pressed
        [[nodiscard]] bool IsKeyPressed(SDL_Scancode key) const;
        // Get mouse position
        [[nodiscard]] const glm::ivec2& GetMousePosition();
        // Get mouse scroll
        [[nodiscard]] const glm::ivec2& GetMouseScroll();
        // Get left stick direction vector
        [[nodiscard]] glm::vec2 GetLStick() const;
        // Get right stick direction vector
        [[nodiscard]] glm::vec2 GetRStick() const;

        // Get controller
        [[nodiscard]] SDL_GameController* GetController() const;
        // Get joystick id
        [[nodiscard]] SDL_JoystickID GetControllerID() const;

        // Get flags
        [[nodiscard]] bool WasMouseMoved() const;
        [[nodiscard]] bool WasMouseScrolled() const;

        // Display ImGui info
        void ImGuiDisplay();
    private:
        // Main constructor
        Inputs();

        // Get axis direction (Dead zone must be between 0.0f and 1.0f)
        glm::vec2 GetNormalisedAxisDirection
        (
            SDL_GameControllerAxis axisHorizontal,
            SDL_GameControllerAxis axisVertical,
            const glm::vec2& deadZone
        ) const;

        // Key array
        const u8* m_keys = nullptr;
        // Controller
        SDL_GameController* m_controller = nullptr;

        // Mouse position
        glm::ivec2 m_mousePosition = {};
        // Mouse scroll
        glm::ivec2 m_mouseScroll = {};

        // Flags
        bool m_wasMouseMoved = false;
        bool m_wasMouseScrolled = false;
    };
}

#endif