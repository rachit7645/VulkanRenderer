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
#include "FreeCamera.h"
#include "Renderer/RenderConstants.h"
#include "Externals/ImGui.h"
#include "Util/Maths.h"

namespace Engine
{
    constexpr f32 ROTATION_RATE = 24.0f;
    constexpr f32 MOVEMENT_RATE = 16.0f;
    constexpr f32 ZOOM_RATE     = 16.0f;

    constexpr f32 STICK_ROTATION_MULTIPLIER = 2.0f;

    constexpr f32 MIN_FOV = glm::radians(10.0f);
    constexpr f32 MAX_FOV = glm::radians(120.0f);

    constexpr SDL_Scancode KEY_SPRINT   = SDL_SCANCODE_LCTRL;
    constexpr SDL_Scancode KEY_FORWARD  = SDL_SCANCODE_W;
    constexpr SDL_Scancode KEY_BACKWARD = SDL_SCANCODE_S;
    constexpr SDL_Scancode KEY_LEFT     = SDL_SCANCODE_A;
    constexpr SDL_Scancode KEY_RIGHT    = SDL_SCANCODE_D;
    constexpr SDL_Scancode KEY_UP       = SDL_SCANCODE_SPACE;
    constexpr SDL_Scancode KEY_DOWN     = SDL_SCANCODE_LSHIFT;

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
          m_targetOrientation{orientation},
          m_targetFOV{FOV},
          m_yaw{glm::normalize(glm::angleAxis(rotation.y, Renderer::WORLD_UP))},
          m_pitch{glm::normalize(glm::angleAxis(rotation.x, glm::vec3(1.0f, 0.0f, 0.0f)))}
    {
    }

    void FreeCamera::Update(const Util::FrameCounter& frameCounter, const Engine::Inputs& inputs)
    {
        isEnabled = !inputs.IsKeyPressed(SDL_SCANCODE_F2) && !inputs.IsButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_DOWN);

        if (!isEnabled)
        {
            return;
        }

        Rotate(frameCounter, inputs);
        Move(frameCounter, inputs);
        Zoom(frameCounter, inputs);

        orientation = Maths::ExponentialDecay(orientation, m_targetOrientation, ROTATION_RATE, frameCounter.frameDelta);
        position    = Maths::ExponentialDecay(position,    m_targetPosition,    MOVEMENT_RATE, frameCounter.frameDelta);
        FOV         = Maths::ExponentialDecay(FOV,         m_targetFOV,         ZOOM_RATE,     frameCounter.frameDelta);

        orientation = glm::normalize(orientation);
    }

    void FreeCamera::Move(const Util::FrameCounter& frameCounter, const Engine::Inputs& inputs)
    {
        const glm::vec3 front = glm::normalize(m_targetOrientation * glm::vec3(0.0f, 0.0f, -1.0f));
        const glm::vec3 right = glm::normalize(m_targetOrientation * glm::vec3(1.0f, 0.0f,  0.0f));

        f32 velocity = m_speed * frameCounter.frameDelta;

        if (inputs.IsKeyPressed(KEY_SPRINT))
        {
            velocity *= m_sprint;
        }

        if (inputs.IsKeyPressed(KEY_FORWARD))
        {
            m_targetPosition += front * velocity;
        }
        else if (inputs.IsKeyPressed(KEY_BACKWARD))
        {
            m_targetPosition -= front * velocity;
        }

        if (inputs.IsKeyPressed(KEY_LEFT))
        {
            m_targetPosition -= right * velocity;
        }
        else if (inputs.IsKeyPressed(KEY_RIGHT))
        {
            m_targetPosition += right * velocity;
        }

        if (inputs.IsKeyPressed(KEY_UP))
        {
            m_targetPosition += Renderer::WORLD_UP * velocity;
        }
        else if (inputs.IsKeyPressed(KEY_DOWN))
        {
            m_targetPosition -= Renderer::WORLD_UP * velocity;
        }

        const glm::vec2 leftStickDirection = inputs.GetLeftStickDirection();
        const f32       leftTrigger        = inputs.GetLeftTriggerMovement();
        const f32       rightTrigger       = inputs.GetRightTriggerMovement();

        // Forward/Backward
        m_targetPosition -= leftStickDirection.y * front * velocity;
        // Left/Right
        m_targetPosition += leftStickDirection.x * right * velocity;
        // Up
        m_targetPosition += Renderer::WORLD_UP * rightTrigger * velocity;
        // Down
        m_targetPosition -= Renderer::WORLD_UP * leftTrigger * velocity;
    }

    void FreeCamera::Rotate(const Util::FrameCounter& frameCounter, const Engine::Inputs& inputs)
    {
        const f32 speed = m_sensitivity * frameCounter.frameDelta;

        f32 deltaPitch = 0.0f;
        f32 deltaYaw   = 0.0f;

        if (inputs.WasMouseMoved())
        {
            const auto mouseDelta = inputs.GetRelativeMouseMovement();

            const glm::vec2 angularMovement = speed * mouseDelta;

            deltaYaw   += angularMovement.x;
            deltaPitch += angularMovement.y;
        }

        glm::vec2 rightStickDirection = inputs.GetRightStickDirection();

        rightStickDirection.x *= -1.0f;

        deltaYaw   += rightStickDirection.x * speed * STICK_ROTATION_MULTIPLIER;
        deltaPitch += rightStickDirection.y * speed * STICK_ROTATION_MULTIPLIER;

        const glm::quat deltaYawQuat   = glm::angleAxis(-deltaYaw,   Renderer::WORLD_UP);
        const glm::quat deltaPitchQuat = glm::angleAxis( deltaPitch, glm::vec3(1.0f, 0.0f, 0.0f));

        m_yaw   = glm::normalize(deltaYawQuat   * m_yaw);
        m_pitch = glm::normalize(deltaPitchQuat * m_pitch);

        m_targetOrientation = glm::normalize(m_yaw * m_pitch);
    }

    void FreeCamera::Zoom(const Util::FrameCounter& frameCounter, const Engine::Inputs& inputs)
    {
        if (!inputs.WasMouseScrolled())
        {
            return;
        }

        m_targetFOV -= inputs.GetMouseScroll().y * m_zoom * frameCounter.frameDelta;
        m_targetFOV  = glm::clamp(m_targetFOV, MIN_FOV, MAX_FOV);
    }

    void FreeCamera::ImGuiDisplay()
    {
        if (ImGui::CollapsingHeader("Camera"))
        {
            Camera::ImGuiDisplay();

            ImGui::DragFloat("Speed",       &m_speed,       1.0f, 0.0f, 0.0f, "%.3f");
            ImGui::DragFloat("Sprint",      &m_sprint,      1.0f, 0.0f, 0.0f, "%.3f");
            ImGui::DragFloat("Sensitivity", &m_sensitivity, 1.0f, 0.0f, 0.0f, "%.3f");
            ImGui::DragFloat("Zoom",        &m_zoom,        1.0f, 0.0f, 0.0f, "%.3f");
        }
    }
}