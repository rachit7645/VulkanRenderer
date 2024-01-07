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

#include "Inputs.h"

#include "Externals/ImGui.h"

namespace Engine
{
    Inputs::Inputs()
        : m_keys(SDL_GetKeyboardState(nullptr))
    {
        // Fix joy-con state
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS,  "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOYCON_HOME_LED,   "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH_PLAYER_LED, "1");

        FindController();
    }

    Inputs& Inputs::Get()
    {
        static Inputs inputs;
        return inputs;
    }

    void Inputs::SetMousePosition(const glm::ivec2& position)
    {
        m_mousePosition = position;
        m_wasMouseMoved = true;
    }

    void Inputs::SetMouseScroll(const glm::ivec2& scroll)
    {
        m_mouseScroll      = scroll;
        m_wasMouseScrolled = true;
    }

    void Inputs::FindController()
    {
        m_controller = nullptr;

        for (s32 i = 0; i < SDL_NumJoysticks(); ++i)
        {
            // We need a proper game controller
            if (SDL_IsGameController(i))
            {
                m_controller = SDL_GameControllerOpen(i);
            }
        }
    }

    bool Inputs::IsKeyPressed(SDL_Scancode key) const
    {
        return static_cast<bool>(m_keys[key]);
    }

    const glm::ivec2& Inputs::GetMousePosition()
    {
        m_wasMouseMoved = false;
        return m_mousePosition;
    }

    const glm::ivec2& Inputs::GetMouseScroll()
    {
        m_wasMouseScrolled = false;
        return m_mouseScroll;
    }

    glm::vec2 Inputs::GetLStick() const
    {
        return GetNormalisedAxisDirection
        (
            SDL_CONTROLLER_AXIS_LEFTX,
            SDL_CONTROLLER_AXIS_LEFTY,
            {0.3f, 0.3f}
        );
    }

    glm::vec2 Inputs::GetRStick() const
    {
        return GetNormalisedAxisDirection
        (
            SDL_CONTROLLER_AXIS_RIGHTX,
            SDL_CONTROLLER_AXIS_RIGHTY,
            {0.1f, 0.1f}
        );
    }

    SDL_GameController* Inputs::GetController() const
    {
        return m_controller;
    }

    SDL_JoystickID Inputs::GetControllerID() const
    {
        return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_controller));
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
        SDL_GameControllerAxis axisHorizontal,
        SDL_GameControllerAxis axisVertical,
        const glm::vec2& deadZone
    ) const
    {
        // No controller connected
        if (m_controller == nullptr)
        {
            return {0, 0};
        }

        auto x = SDL_GameControllerGetAxis(m_controller, axisHorizontal);
        auto y = SDL_GameControllerGetAxis(m_controller, axisVertical);

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
                glm::ivec2 mousePos = {};
                SDL_GetMouseState(&mousePos.x, &mousePos.y);

                // Mouse data
                ImGui::DragInt2("Mouse Position", &mousePos[0]);
                ImGui::DragInt2("Mouse Relative", &m_mousePosition[0]);
                ImGui::DragInt2("Mouse Scroll", &m_mouseScroll[0]);

                // If controller is connected
                if (m_controller != nullptr)
                {
                    auto controllerType = [] (SDL_GameControllerType controllerType)
                    {
                        switch (controllerType)
                        {
                            case SDL_CONTROLLER_TYPE_UNKNOWN:                      return "Unknown";
                            case SDL_CONTROLLER_TYPE_XBOX360:                      return "Xbox 360";
                            case SDL_CONTROLLER_TYPE_XBOXONE:                      return "Xbox One";
                            case SDL_CONTROLLER_TYPE_PS3:                          return "PlayStation 3";
                            case SDL_CONTROLLER_TYPE_PS4:                          return "PlayStation 4";
                            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:          return "Nintendo Switch Pro";
                            case SDL_CONTROLLER_TYPE_VIRTUAL:                      return "Virtual";
                            case SDL_CONTROLLER_TYPE_PS5:                          return "PlayStation 5";
                            case SDL_CONTROLLER_TYPE_AMAZON_LUNA:                  return "Amazon Luna";
                            case SDL_CONTROLLER_TYPE_GOOGLE_STADIA:                return "Google Stadia";
                            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:  return "Nintendo Switch Joy-Con (Left)";
                            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: return "Nintendo Switch Joy-Con (Right)";
                            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:  return "Nintendo Switch Joy-Con (Pair)";
                            default:                                               return "Unknown";
                        }
                    }(SDL_GameControllerGetType(m_controller));

                    // Controller data
                    ImGui::Text("%s", controllerType);
                    ImGui::DragFloat2("LStick", &GetLStick()[0], 1.0f, 0.0f, 0.0f, "%.3f");
                    ImGui::DragFloat2("RStick", &GetRStick()[0], 1.0f, 0.0f, 0.0f, "%.3f");
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }
}