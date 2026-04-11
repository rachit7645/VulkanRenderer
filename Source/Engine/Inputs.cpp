/*
 * Copyright (c) 2023 - 2026 Rachit
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
#include "Util/Types.h"

namespace Engine
{
    Inputs::Inputs(bool enableJoyConFixes)
        : m_keys(SDL_GetKeyboardState(nullptr))
    {
        if (enableJoyConFixes)
        {
            SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH,            "1");
            SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH_HOME_LED,   "0");
            SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH_PLAYER_LED, "1");
            SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS,          "1");
            SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOYCON_HOME_LED,   "0");
            SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS,  "1");
        }
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
        gamepad = nullptr;

        s32 joystickCount = 0;

        const SDL_JoystickID* joysticks = SDL_GetJoysticks(&joystickCount);

        if (joysticks == nullptr)
        {
            return;
        }

        for (s32 i = 0; i < joystickCount; ++i)
        {
            if (SDL_IsGamepad(joysticks[i]))
            {
                gamepad = SDL_OpenGamepad(joysticks[i]);

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

    glm::vec2 Inputs::GetLeftStickDirection() const
    {
        return GetNormalisedAxisDirection
        (
            SDL_GAMEPAD_AXIS_LEFTX,
            SDL_GAMEPAD_AXIS_LEFTY,
            {0.1f, 0.1f}
        );
    }

    glm::vec2 Inputs::GetRightStickDirection() const
    {
        return GetNormalisedAxisDirection
        (
            SDL_GAMEPAD_AXIS_RIGHTX,
            SDL_GAMEPAD_AXIS_RIGHTY,
            {0.3f, 0.3f}
        );
    }

    SDL_JoystickID Inputs::GetGamepadID() const
    {
        return SDL_GetJoystickID(SDL_GetGamepadJoystick(gamepad));
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
        if (gamepad == nullptr)
        {
            return {0, 0};
        }

        const s16 x = SDL_GetGamepadAxis(gamepad, axisHorizontal);
        const s16 y = SDL_GetGamepadAxis(gamepad, axisVertical);

        constexpr auto AXIS_MAX = static_cast<f32>(SDL_JOYSTICK_AXIS_MAX);

        auto normalized = glm::vec2(x, y) / AXIS_MAX;

        if (std::abs(normalized.x) < deadZone.x)
        {
            normalized.x = 0.0f;
        }

        if (std::abs(normalized.y) < deadZone.y)
        {
            normalized.y = 0.0f;
        }

        auto direction = glm::normalize(normalized);

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
            if (ImGui::BeginMenu("Engine"))
            {
                if (ImGui::CollapsingHeader("Input"))
                {
                    glm::vec2 mousePos = {};
                    // Realtime(ish) mouse position
                    SDL_GetMouseState(&mousePos.x, &mousePos.y);

                    ImGui::DragFloat2("Mouse Position", &mousePos[0]);
                    ImGui::DragFloat2("Mouse Relative", &m_mousePosition[0]);
                    ImGui::DragFloat2("Mouse Scroll",   &m_mouseScroll[0]);

                    if (gamepad != nullptr)
                    {
                        ImGui::Text("%s", SDL_GetGamepadName(gamepad));

                        ImGui::DragFloat2("LStick", &GetLeftStickDirection()[0],  1.0f, 0.0f, 0.0f, "%.3f");
                        ImGui::DragFloat2("RStick", &GetRightStickDirection()[0], 1.0f, 0.0f, 0.0f, "%.3f");
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void Inputs::Destroy()
    {
        SDL_CloseGamepad(gamepad);
    }
}
