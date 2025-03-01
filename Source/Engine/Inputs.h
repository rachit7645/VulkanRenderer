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

#ifndef INPUTS_H
#define INPUTS_H

#include <SDL3/SDL.h>

#include "Externals/GLM.h"
#include "Util/Util.h"

namespace Engine
{
    class Inputs
    {
    public:
        static Inputs& Get();

        void SetMousePosition(const glm::vec2& position);
        void SetMouseScroll(const glm::vec2& scroll);

        void FindGamepad();

        [[nodiscard]] bool IsKeyPressed(SDL_Scancode key) const;

        [[nodiscard]] const glm::vec2& GetMousePosition();
        [[nodiscard]] const glm::vec2& GetMouseScroll();

        [[nodiscard]] glm::vec2 GetLStick() const;
        [[nodiscard]] glm::vec2 GetRStick() const;

        [[nodiscard]] SDL_Gamepad* GetGamepad() const;
        [[nodiscard]] SDL_JoystickID GetGamepadID() const;

        [[nodiscard]] bool WasMouseMoved() const;
        [[nodiscard]] bool WasMouseScrolled() const;

        void ImGuiDisplay();
        void Destroy();
    private:
        Inputs();

        // Dead zone must be between 0.0f and 1.0f
        [[nodiscard]] glm::vec2 GetNormalisedAxisDirection
        (
            SDL_GamepadAxis axisHorizontal,
            SDL_GamepadAxis axisVertical,
            const glm::vec2& deadZone
        ) const;

        // Key array from SDL
        const bool* m_keys = nullptr;
        // Game controller from SDL
        SDL_Gamepad* m_gamepad = nullptr;

        // Mouse data
        glm::vec2 m_mousePosition = {};
        glm::vec2 m_mouseScroll   = {};

        // Flags
        bool m_wasMouseMoved    = false;
        bool m_wasMouseScrolled = false;
    };
}

#endif