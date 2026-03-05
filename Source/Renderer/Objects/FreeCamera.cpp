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

#include "FreeCamera.h"
#include "Renderer/RenderConstants.h"
#include "Engine/Inputs.h"
#include "Externals/ImGui.h"

namespace Renderer::Objects
{
    FreeCamera::FreeCamera
    (
        const glm::vec3& position,
        const glm::vec3& rotation,
        f32 FOV,
        f32 speed,
        f32 sprint,
        f32 sensitivity,
        f32 zoom
    )
        : Camera(position, rotation, FOV),
          m_speed(speed),
          m_sprint(sprint),
          m_sensitivity(sensitivity),
          m_zoom(zoom)
    {
    }

    void FreeCamera::Update(const Util::FrameCounter& frameCounter, Engine::Inputs& inputs)
    {
        if (isEnabled == true)
        {
            CheckInputs(frameCounter, inputs);
        }

        front.x = std::cos(rotation.y) * std::cos(rotation.x);
        front.y = std::sin(rotation.x);
        front.z = std::sin(rotation.y) * std::cos(rotation.x);
        front   = glm::normalize(front);

        right = glm::normalize(glm::cross(front, Renderer::WORLD_UP));
        up    = glm::normalize(glm::cross(right, front));
    }

    void FreeCamera::CheckInputs(const Util::FrameCounter& frameCounter, Engine::Inputs& inputs)
    {
        Move(frameCounter, inputs);
        Rotate(frameCounter, inputs);
        Zoom(frameCounter, inputs);
    }

    void FreeCamera::Move(const Util::FrameCounter& frameCounter, const Engine::Inputs& inputs)
    {
        f32 velocity = m_speed * frameCounter.frameDelta;

        // Sprint
        if (inputs.IsKeyPressed(SDL_SCANCODE_LCTRL))
        {
            velocity *= m_sprint;
        }

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

        // Up
        if (inputs.IsKeyPressed(SDL_SCANCODE_SPACE))
        {
            position += WORLD_UP * velocity;
        }
        // Down
        if (inputs.IsKeyPressed(SDL_SCANCODE_LSHIFT))
        {
            position -= WORLD_UP * velocity;
        }

        const auto lStick = inputs.GetLStick();
        // Forward/Backward
        position -= lStick.y * front * velocity;
        // Left/Right
        position += lStick.x * right * velocity;
    }

    void FreeCamera::Rotate(const Util::FrameCounter& frameCounter, Engine::Inputs& inputs)
    {
        constexpr auto ROTATION_STICK_MULTIPLIER = 0.04f;

        constexpr f32 MAX_YAW = glm::radians(89.0f);

        const auto speed = m_sensitivity * frameCounter.frameDelta;

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
        rotation.x += rStick.y * speed * ROTATION_STICK_MULTIPLIER;
        // Yaw
        rotation.y += rStick.x * speed * ROTATION_STICK_MULTIPLIER;

        // Don't really want to flip the world around
        rotation.x = glm::clamp(rotation.x, -MAX_YAW, MAX_YAW);
    }

    void FreeCamera::Zoom(const Util::FrameCounter& frameCounter, Engine::Inputs& inputs)
    {
        constexpr f32 MIN_FOV = glm::radians(10.0f);
        constexpr f32 MAX_FOV = glm::radians(120.0f);

        // Stops things from going haywire
        if (!inputs.WasMouseScrolled())
        {
            return;
        }

        FOV -= inputs.GetMouseScroll().y * m_zoom * frameCounter.frameDelta;
        FOV  = glm::clamp(FOV, MIN_FOV, MAX_FOV);
    }

    void FreeCamera::ImGuiDisplay()
    {
        Camera::ImGuiDisplay();

        if (ImGui::BeginMenu("Camera"))
        {
            // Camera Settings
            ImGui::DragFloat("Speed",       &m_speed,       1.0f, 0.0f, 0.0f, "%.3f");
            ImGui::DragFloat("Sprint",      &m_sprint,      1.0f, 0.0f, 0.0f, "%.3f");
            ImGui::DragFloat("Sensitivity", &m_sensitivity, 1.0f, 0.0f, 0.0f, "%.3f");
            ImGui::DragFloat("Zoom",        &m_zoom,        1.0f, 0.0f, 0.0f, "%.3f");

            ImGui::EndMenu();
        }
    }

}