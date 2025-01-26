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

#include "Inputs.h"

#include "Externals/ImGui.h"

namespace Engine
{
    Inputs::Inputs()
        : m_keys(SDL_GetKeyboardState(nullptr))
    {
        // HACK?: Fix joy-con state
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS,  "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOYCON_HOME_LED,   "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH_PLAYER_LED, "1");
    }

    Inputs& Inputs::Get()
    {
        static Inputs inputs;
        return inputs;
    }

    void Inputs::SetMousePosition(const glm::vec2& position)
    {
        m_mousePosition = position;
        m_wasMouseMoved = true;
    }

    void Inputs::SetMouseScroll(const glm::vec2& scroll)
    {
        m_mouseScroll      = scroll;
        m_wasMouseScrolled = true;
    }

    void Inputs::FindGamepad()
    {
        m_gamepad = nullptr;

        s32 joystickCount = 0;
        auto* joysticks = SDL_GetJoysticks(&joystickCount);

        // No joystick found, abort
        if (joysticks == nullptr)
        {
            return;
        }

        for (s32 i = 0; i < joystickCount; ++i)
        {
            // We need a proper game controller
            if (SDL_IsGamepad(joysticks[i]))
            {
                m_gamepad = SDL_OpenGamepad(joysticks[i]);
                break;
            }
        }
    }

    bool Inputs::IsKeyPressed(SDL_Scancode key) const
    {
        return m_keys[key];
    }

    const glm::vec2& Inputs::GetMousePosition()
    {
        m_wasMouseMoved = false;
        return m_mousePosition;
    }

    const glm::vec2& Inputs::GetMouseScroll()
    {
        m_wasMouseScrolled = false;
        return m_mouseScroll;
    }

    glm::vec2 Inputs::GetLStick() const
    {
        return GetNormalisedAxisDirection
        (
            SDL_GAMEPAD_AXIS_LEFTX,
            SDL_GAMEPAD_AXIS_LEFTY,
            {0.1f, 0.1f}
        );
    }

    glm::vec2 Inputs::GetRStick() const
    {
        return GetNormalisedAxisDirection
        (
            SDL_GAMEPAD_AXIS_RIGHTX,
            SDL_GAMEPAD_AXIS_RIGHTY,
            {0.3f, 0.3f}
        );
    }

    SDL_Gamepad* Inputs::GetGamepad() const
    {
        return m_gamepad;
    }

    SDL_JoystickID Inputs::GetGamepadID() const
    {
        return SDL_GetJoystickID(SDL_GetGamepadJoystick(m_gamepad));
    }

    bool Inputs::WasMouseMoved() const
    {
        return m_wasMouseMoved;
    }

    bool Inputs::WasMouseScrolled() const
    {
        return m_wasMouseScrolled;
    }

    glm::vec2 Inputs::GetNormalisedAxisDirection
    (
        SDL_GamepadAxis axisHorizontal,
        SDL_GamepadAxis axisVertical,
        const glm::vec2& deadZone
    ) const
    {
        // No controller connected
        if (m_gamepad == nullptr)
        {
            return {0, 0};
        }

        const auto x = SDL_GetGamepadAxis(m_gamepad, axisHorizontal);
        const auto y = SDL_GetGamepadAxis(m_gamepad, axisVertical);

        constexpr auto AXIS_MAX = static_cast<f32>(SDL_JOYSTICK_AXIS_MAX);
        f32 normalizedX = static_cast<f32>(x) / AXIS_MAX;
        f32 normalizedY = static_cast<f32>(y) / AXIS_MAX;

        if (std::abs(normalizedX) < deadZone.x) normalizedX = 0.0f;
        if (std::abs(normalizedY) < deadZone.y) normalizedY = 0.0f;

        auto direction = glm::normalize(glm::vec2(normalizedX, normalizedY));

        if (std::isnan(direction.x) || std::isinf(direction.x))
        {
            direction.x = 0.0f;
        }

        if (std::isnan(direction.y) || std::isinf(direction.y))
        {
            direction.y = 0.0f;
        }

        return direction;
    }

    void Inputs::ImGuiDisplay()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Input"))
            {
                // Realtime(ish) mouse position
                glm::vec2 mousePos = {};
                SDL_GetMouseState(&mousePos.x, &mousePos.y);

                // Mouse data
                ImGui::DragFloat2("Mouse Position", &mousePos[0]);
                ImGui::DragFloat2("Mouse Relative", &m_mousePosition[0]);
                ImGui::DragFloat2("Mouse Scroll",   &m_mouseScroll[0]);

                // If controller is connected
                if (m_gamepad != nullptr)
                {
                    // Controller data
                    ImGui::Text("%s", SDL_GetGamepadName(m_gamepad));
                    ImGui::DragFloat2("LStick", &GetLStick()[0], 1.0f, 0.0f, 0.0f, "%.3f");
                    ImGui::DragFloat2("RStick", &GetRStick()[0], 1.0f, 0.0f, 0.0f, "%.3f");
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void Inputs::Destroy()
    {
        SDL_CloseGamepad(m_gamepad);

        m_gamepad = nullptr;

        m_mousePosition = {};
        m_mouseScroll   = {};

        m_wasMouseMoved    = false;
        m_wasMouseScrolled = false;
    }
}