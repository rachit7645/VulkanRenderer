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

#include "FreeCamera.h"
#include "Renderer/RenderConstants.h"
#include "Engine/Inputs.h"

namespace Renderer::Objects
{
    FreeCamera::FreeCamera
    (
        const glm::vec3& position,
        const glm::vec3& rotation,
        f32 FOV,
        f32 speed,
        f32 sensitivity,
        f32 zoom
    )
        : Camera(position, rotation, FOV),
          m_speed(speed),
          m_sensitivity(sensitivity),
          m_zoom(zoom)
    {
    }

    void FreeCamera::Update(f32 frameDelta)
    {
        if (isEnabled == true)
        {
            CheckInputs(frameDelta);
        }

        front.x = std::cos(rotation.y) * std::cos(rotation.x);
        front.y = std::sin(rotation.x);
        front.z = std::sin(rotation.y) * std::cos(rotation.x);
        front   = glm::normalize(front);

        right = glm::normalize(glm::cross(front, Renderer::WORLD_UP));
        up    = glm::normalize(glm::cross(right, front));

        ImGuiDisplay();
    }

    void FreeCamera::CheckInputs(f32 frameDelta)
    {
        Move(frameDelta);
        Rotate(frameDelta);
        Zoom(frameDelta);
    }

    void FreeCamera::Move(f32 frameDelta)
    {
        const auto& inputs = Engine::Inputs::Get();

        const f32 velocity = m_speed * frameDelta;

        // Forward
        if (inputs.IsKeyPressed(SDL_SCANCODE_W))
        {
            position += front * velocity;
        }
        // Backward
        else if (inputs.IsKeyPressed(SDL_SCANCODE_S))
        {
            position -= front * velocity;
        }

        // Left
        if (inputs.IsKeyPressed(SDL_SCANCODE_A))
        {
            position -= right * velocity;
        }
        // Right
        else if (inputs.IsKeyPressed(SDL_SCANCODE_D))
        {
            position += right * velocity;
        }

        const auto lStick = inputs.GetLStick();
        // Forward/Backward
        position -= lStick.y * front * velocity;
        // Left/Right
        position += lStick.x * right * velocity;
    }

    void FreeCamera::Rotate(f32 frameDelta)
    {
        auto& inputs = Engine::Inputs::Get();

        const auto speed = m_sensitivity * frameDelta;

        // Avoids freaking out
        if (inputs.WasMouseMoved())
        {
            // Yaw
            rotation.y += glm::radians(inputs.GetMousePosition().x * speed);
            // Pitch
            rotation.x += glm::radians(inputs.GetMousePosition().y * speed);
        }

        const auto rStick = inputs.GetRStick();
        // Pitch
        rotation.x += rStick.y * speed * 0.04f;
        // Yaw
        rotation.y += rStick.x * speed * 0.04f;

        // Don't really want to flip the world around
        rotation.x = glm::clamp(rotation.x, glm::radians(-89.0f), glm::radians(89.0f));
    }

    void FreeCamera::Zoom(f32 frameDelta)
    {
        auto& inputs = Engine::Inputs::Get();

        // Stops things from going haywire
        if (inputs.WasMouseScrolled())
        {
            FOV -= inputs.GetMouseScroll().y * m_zoom * frameDelta;
            FOV = glm::clamp(FOV, glm::radians(10.0f), glm::radians(120.0f));
        }
    }

    void FreeCamera::ImGuiDisplay()
    {
        Camera::ImGuiDisplay();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Camera"))
            {
                // Camera Settings
                ImGui::DragFloat("Speed",       &m_speed,       1.0f, 0.0f, 0.0f, "%.7f");
                ImGui::DragFloat("Sensitivity", &m_sensitivity, 1.0f, 0.0f, 0.0f, "%.7f");
                ImGui::DragFloat("Zoom",        &m_zoom,        1.0f, 0.0f, 0.0f, "%.7f");

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

}