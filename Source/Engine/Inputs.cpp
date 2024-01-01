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

#include "Inputs.h"

#include "Externals/ImGui.h"

namespace Engine
{
    Inputs::Inputs()
        : m_keys(SDL_GetKeyboardState(nullptr))
    {
        // Set Joy-Con state
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS,  "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOYCON_HOME_LED,   "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH_PLAYER_LED, "1");
        // Find controller
        FindController();
    }

    Inputs& Inputs::GetInstance()
    {
        // Static storage
        static Inputs inputs;
        // Return
        return inputs;
    }

    void Inputs::SetMousePosition(const glm::ivec2& position)
    {
        // Set position
        m_mousePosition = position;
        // Set flag
        m_wasMouseMoved = true;
    }

    void Inputs::SetMouseScroll(const glm::ivec2& scroll)
    {
        // Set scroll
        m_mouseScroll = scroll;
        // Set flag
        m_wasMouseScrolled = true;
    }

    void Inputs::FindController()
    {
        // Reset controller handle
        m_controller = nullptr;

        // Loop over controller
        for (s32 i = 0; i < SDL_NumJoysticks(); ++i)
        {
            // We need a proper game controller
            if (SDL_IsGameController(i))
            {
                // Store controller
                m_controller = SDL_GameControllerOpen(i);
            }
        }
    }

    bool Inputs::IsKeyPressed(SDL_Scancode key) const
    {
        // Return key state
        return static_cast<bool>(m_keys[key]);
    }

    const glm::ivec2& Inputs::GetMousePosition()
    {
        // Set flag
        m_wasMouseMoved = false;
        // Return
        return m_mousePosition;
    }

    const glm::ivec2& Inputs::GetMouseScroll()
    {
        // Set flag
        m_wasMouseScrolled = false;
        // Return
        return m_mouseScroll;
    }

    glm::vec2 Inputs::GetLStick() const
    {
        // Return
        return GetNormalisedAxisDirection
        (
            SDL_CONTROLLER_AXIS_LEFTX,
            SDL_CONTROLLER_AXIS_LEFTY,
            {0.3f, 0.3f}
        );
    }

    glm::vec2 Inputs::GetRStick() const
    {
        // Return
        return GetNormalisedAxisDirection
        (
            SDL_CONTROLLER_AXIS_RIGHTX,
            SDL_CONTROLLER_AXIS_RIGHTY,
            {0.1f, 0.1f}
        );
    }

    SDL_GameController* Inputs::GetController() const
    {
        // Return
        return m_controller;
    }

    SDL_JoystickID Inputs::GetControllerID() const
    {
        // Return
        return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_controller));
    }

    bool Inputs::WasMouseMoved() const
    {
        // Return
        return m_wasMouseMoved;
    }

    bool Inputs::WasMouseScrolled() const
    {
        // Return
        return m_wasMouseScrolled;
    }

    glm::vec2 Inputs::GetNormalisedAxisDirection
    (
        SDL_GameControllerAxis axisHorizontal,
        SDL_GameControllerAxis axisVertical,
        const glm::vec2& deadZone
    ) const
    {
        // Null check
        if (m_controller == nullptr)
        {
            // Return
            return {0, 0};
        }

        // Get left stick input
        auto x = SDL_GameControllerGetAxis(m_controller, axisHorizontal);
        auto y = SDL_GameControllerGetAxis(m_controller, axisVertical);

        // Normalize the values to get a 2D vector
        constexpr auto AXIS_MAX = static_cast<f32>(SDL_JOYSTICK_AXIS_MAX);
        f32 normalizedX = static_cast<f32>(x) / AXIS_MAX;
        f32 normalizedY = static_cast<f32>(y) / AXIS_MAX;

        // Check
        if (std::abs(normalizedX) < deadZone.x) normalizedX = 0.0f;
        if (std::abs(normalizedY) < deadZone.y) normalizedY = 0.0f;

        // Normalise
        auto direction = glm::normalize(glm::vec2(normalizedX, normalizedY));

        // Check for NaN and Infinity in X
        if (std::isnan(direction.x) || std::isinf(direction.x))
        {
            // Set
            direction.x = 0.0f;
        }

        // Check for NaN and Infinity in Y
        if (std::isnan(direction.y) || std::isinf(direction.y))
        {
            // Set
            direction.y = 0.0f;
        }

        // Return
        return direction;
    }

    void Inputs::ImGuiDisplay()
    {
        // Menu bar
        if (ImGui::BeginMainMenuBar())
        {
            // Input menu
            if (ImGui::BeginMenu("Input"))
            {
                // Get mouse pos
                glm::ivec2 mousePos = {};
                SDL_GetMouseState(&mousePos.x, &mousePos.y);

                // Mouse position
                ImGui::DragInt2("Mouse Position", &mousePos[0]);
                // Mouse position
                ImGui::DragInt2("Mouse Relative", &m_mousePosition[0]);
                // Mouse scroll
                ImGui::DragInt2("Mouse Scroll", &m_mouseScroll[0]);

                // If controller exists
                if (m_controller != nullptr)
                {
                    // Get type
                    auto controllerType = [] (SDL_GameControllerType controllerType)
                    {
                        // Switch for controller type
                        switch (controllerType)
                        {
                            case SDL_CONTROLLER_TYPE_UNKNOWN: return "Unknown";
                            case SDL_CONTROLLER_TYPE_XBOX360: return "Xbox 360";
                            case SDL_CONTROLLER_TYPE_XBOXONE: return "Xbox One";
                            case SDL_CONTROLLER_TYPE_PS3: return "PlayStation 3";
                            case SDL_CONTROLLER_TYPE_PS4: return "PlayStation 4";
                            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: return "Nintendo Switch Pro";
                            case SDL_CONTROLLER_TYPE_VIRTUAL: return "Virtual";
                            case SDL_CONTROLLER_TYPE_PS5: return "PlayStation 5";
                            case SDL_CONTROLLER_TYPE_AMAZON_LUNA: return "Amazon Luna";
                            case SDL_CONTROLLER_TYPE_GOOGLE_STADIA: return "Google Stadia";
                            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT: return "Nintendo Switch Joy-Con (Left)";
                            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: return "Nintendo Switch Joy-Con (Right)";
                            case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return "Nintendo Switch Joy-Con (Pair)";
                            default: return "Unknown";
                        }
                    }(SDL_GameControllerGetType(m_controller));

                    // Controller type
                    ImGui::Text("%s", controllerType);
                    // Left stick
                    ImGui::DragFloat2("LStick", &GetLStick()[0], 1.0f, 0.0f, 0.0f, "%.3f");
                    // Right stick
                    ImGui::DragFloat2("RStick", &GetRStick()[0], 1.0f, 0.0f, 0.0f, "%.3f");
                }

                // End menu
                ImGui::EndMenu();
            }
            // End menu bar
            ImGui::EndMainMenuBar();
        }
    }
}