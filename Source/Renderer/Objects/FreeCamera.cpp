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
#include "Util/Maths.h"

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
        : Camera{position, rotation, FOV},
          m_speed{speed},
          m_sprint{sprint},
          m_sensitivity{sensitivity},
          m_zoom{zoom},
          m_targetPosition{position},
          m_targetFOV{FOV}
    {
        m_yaw   = glm::normalize(glm::angleAxis(rotation.y, WORLD_UP));
        m_pitch = glm::normalize(glm::angleAxis(rotation.x, glm::vec3(1.0f, 0.0f, 0.0f)));

        m_targetOrientation = orientation;
    }

    void FreeCamera::Update(const Util::FrameCounter& frameCounter, Engine::Inputs& inputs)
    {
        constexpr f32 ROTATION_SMOOTHNESS = 24.0f;
        constexpr f32 MOVEMENT_SMOOTHNESS = 16.0f;
        constexpr f32 ZOOM_SMOOTHNESS     = 16.0f;

        if (!isEnabled)
        {
            return;
        }

        Rotate(frameCounter, inputs);
        Move(frameCounter, inputs);
        Zoom(frameCounter, inputs);

        orientation = Maths::ExponentialDecay(orientation, m_targetOrientation, ROTATION_SMOOTHNESS, frameCounter.frameDelta);
        position    = Maths::ExponentialDecay(position,    m_targetPosition,    MOVEMENT_SMOOTHNESS, frameCounter.frameDelta);
        FOV         = Maths::ExponentialDecay(FOV,         m_targetFOV,         ZOOM_SMOOTHNESS,     frameCounter.frameDelta);

        orientation = glm::normalize(orientation);
    }

    void FreeCamera::Move(const Util::FrameCounter& frameCounter, const Engine::Inputs& inputs)
    {
        const glm::vec3 front = m_targetOrientation * glm::vec3(0.0f, 0.0f, -1.0f);
        const glm::vec3 right = m_targetOrientation * glm::vec3(1.0f, 0.0f,  0.0f);

        f32 velocity = m_speed * frameCounter.frameDelta;

        // Sprint
        if (inputs.IsKeyPressed(SDL_SCANCODE_LCTRL))
        {
            velocity *= m_sprint;
        }

        // Forward
        if (inputs.IsKeyPressed(SDL_SCANCODE_W))
        {
            m_targetPosition += front * velocity;
        }
        // Backward
        else if (inputs.IsKeyPressed(SDL_SCANCODE_S))
        {
            m_targetPosition -= front * velocity;
        }

        // Left
        if (inputs.IsKeyPressed(SDL_SCANCODE_A))
        {
            m_targetPosition -= right * velocity;
        }
        // Right
        else if (inputs.IsKeyPressed(SDL_SCANCODE_D))
        {
            m_targetPosition += right * velocity;
        }

        // Up
        if (inputs.IsKeyPressed(SDL_SCANCODE_SPACE))
        {
            m_targetPosition += WORLD_UP * velocity;
        }
        // Down
        else if (inputs.IsKeyPressed(SDL_SCANCODE_LSHIFT))
        {
            m_targetPosition -= WORLD_UP * velocity;
        }

        const auto lStick = inputs.GetLStick();
        // Forward/Backward
        m_targetPosition -= lStick.y * front * velocity;
        // Left/Right
        m_targetPosition += lStick.x * right * velocity;
    }

    void FreeCamera::Rotate(const Util::FrameCounter& frameCounter, Engine::Inputs& inputs)
    {
        constexpr f32 ROTATION_STICK_MULTIPLIER = 0.04f;

        const f32 speed = m_sensitivity * frameCounter.frameDelta;

        f32 deltaPitch = 0.0f;
        f32 deltaYaw   = 0.0f;

        // Avoids freaking out
        if (inputs.WasMouseMoved())
        {
            const auto mouseDelta = inputs.GetMousePosition();

            const glm::vec2 angularMovement = glm::radians(speed * mouseDelta);

            deltaYaw   += angularMovement.x;
            deltaPitch += angularMovement.y;
        }

        const auto rStick = inputs.GetRStick();

        deltaYaw   += rStick.x * speed * ROTATION_STICK_MULTIPLIER;
        deltaPitch += rStick.y * speed * ROTATION_STICK_MULTIPLIER;

        const glm::quat deltaYawQuat   = glm::angleAxis(-deltaYaw,   WORLD_UP);
        const glm::quat deltaPitchQuat = glm::angleAxis( deltaPitch, glm::vec3(1.0f, 0.0f, 0.0f));

        m_yaw   = glm::normalize(deltaYawQuat   * m_yaw);
        m_pitch = glm::normalize(deltaPitchQuat * m_pitch);

        m_targetOrientation = glm::normalize(m_yaw * m_pitch);
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

        m_targetFOV -= inputs.GetMouseScroll().y * m_zoom * frameCounter.frameDelta;
        m_targetFOV  = glm::clamp(m_targetFOV, MIN_FOV, MAX_FOV);
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