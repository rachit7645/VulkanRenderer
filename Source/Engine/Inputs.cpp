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
#include "Util/Log.h"

namespace Engine
{
    Inputs::Inputs()
    {
        s32 count = 0;

        const bool* pointer = SDL_GetKeyboardState(&count);

        if (pointer == nullptr || count == 0)
        {
            Logger::Error("{}\n", "Failed to get keyboard state!");
        }

        m_keys = std::span(pointer, pointer + count);

        SDL::SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH,            "1");
        SDL::SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH_HOME_LED,   "0");
        SDL::SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH_PLAYER_LED, "1");
        SDL::SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS,          "1");
        SDL::SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOYCON_HOME_LED,   "0");
        SDL::SetHint(SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS,  "1");
    }

    void Inputs::Reset()
    {
        m_relativeMouseMovement = {};
        m_mouseScroll           = {};
    }

    void Inputs::SetRelativeMouseMovement(const glm::vec2& movement)
    {
        m_relativeMouseMovement = movement;
    }

    void Inputs::SetMouseScroll(const glm::vec2& scroll)
    {
        m_mouseScroll = scroll;
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
        if (key >= m_keys.size())
        {
            return false;
        }

        return m_keys[key];
    }

    bool Inputs::IsButtonPressed(SDL_GamepadButton button) const
    {
        if (gamepad == nullptr)
        {
            return false;
        }

        return SDL_GetGamepadButton(gamepad, button);
    }

    const glm::vec2& Inputs::GetRelativeMouseMovement() const
    {
        return m_relativeMouseMovement;
    }

    const glm::vec2& Inputs::GetMouseScroll() const
    {
        return m_mouseScroll;
    }

    glm::vec2 Inputs::GetLeftStickDirection() const
    {
        return GetAxisDirection
        (
            SDL_GAMEPAD_AXIS_LEFTX,
            SDL_GAMEPAD_AXIS_LEFTY,
            {0.15f, 0.15f}
        );
    }

    glm::vec2 Inputs::GetRightStickDirection() const
    {
        return GetAxisDirection
        (
            SDL_GAMEPAD_AXIS_RIGHTX,
            SDL_GAMEPAD_AXIS_RIGHTY,
            {0.3f, 0.3f}
        );
    }

    f32 Inputs::GetLeftTriggerMovement() const
    {
        return GetAxisNormalized(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    }

    f32 Inputs::GetRightTriggerMovement() const
    {
        return GetAxisNormalized(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    }

    SDL_JoystickID Inputs::GetGamepadID() const
    {
        return SDL_GetJoystickID(SDL_GetGamepadJoystick(gamepad));
    }

    bool Inputs::WasMouseMoved() const
    {
        return m_relativeMouseMovement != glm::vec2{};
    }

    bool Inputs::WasMouseScrolled() const
    {
        return m_mouseScroll != glm::vec2{};
    }

    glm::vec2 Inputs::GetAxisDirection
    (
        SDL_GamepadAxis axisHorizontal,
        SDL_GamepadAxis axisVertical,
        const glm::vec2& deadZone
    ) const
    {
        if (gamepad == nullptr)
        {
            return {0.0f, 0.0};
        }

        auto normalized = glm::vec2(GetAxisNormalized(axisHorizontal), GetAxisNormalized(axisVertical));

        if (std::abs(normalized.x) < deadZone.x)
        {
            normalized.x = 0.0f;
        }

        if (std::abs(normalized.y) < deadZone.y)
        {
            normalized.y = 0.0f;
        }

        return normalized;
    }

    f32 Inputs::GetAxisNormalized(SDL_GamepadAxis axis) const
    {
        if (gamepad == nullptr)
        {
            return 0.0f;
        }

        const s16 value = SDL_GetGamepadAxis(gamepad, axis);

        const f32 normalized = static_cast<f32>(value) / static_cast<f32>(SDL_JOYSTICK_AXIS_MAX);

        return glm::clamp(normalized, -1.0f, 1.0f);
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
                    ImGui::DragFloat2("Mouse Relative", &m_relativeMouseMovement[0]);
                    ImGui::DragFloat2("Mouse Scroll",   &m_mouseScroll[0]);

                    if (gamepad != nullptr)
                    {
                        ImGui::Text("%s", SDL_GetGamepadName(gamepad));

                        glm::vec2 lStick = GetLeftStickDirection();
                        glm::vec2 rStick = GetRightStickDirection();

                        ImGui::DragFloat2("LStick", &lStick[0],  1.0f, 0.0f, 0.0f, "%.3f");
                        ImGui::DragFloat2("RStick", &rStick[0],  1.0f, 0.0f, 0.0f, "%.3f");
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void Inputs::Destroy()
    {
        Reset();

        SDL_CloseGamepad(gamepad);

        gamepad = nullptr;
        m_keys  = {};
    }
}