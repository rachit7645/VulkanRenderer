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

#include <numbers>

#include "FreeCamera.h"
#include "RenderConstants.h"
#include "Engine/Inputs.h"

namespace Renderer
{
    // Constants (per us)
    constexpr f32 CAMERA_SPEED       = 0.00025f;
    constexpr f32 CAMERA_SENSITIVITY = 0.0001f;
    constexpr f32 CAMERA_ZOOM        = 0.000045f;

    FreeCamera::FreeCamera()
        : FreeCamera(glm::vec3(30.0f, 30.0f, 4.0f), glm::vec3(0.0f, std::numbers::pi, 0.0f), Renderer::DEFAULT_FOV)
    {
    }

    FreeCamera::FreeCamera(const glm::vec3& position, const glm::vec3& rotation, f32 FOV)
        : Camera(position, rotation, FOV)
    {
    }

    void FreeCamera::Update(f32 frameDelta)
    {
        CheckInputs(frameDelta);

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
        const auto& inputs = Engine::Inputs::GetInstance();

        f32 velocity = CAMERA_SPEED * frameDelta;

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

        auto lStick = inputs.GetLStick();
        // Forward/Backward
        position -= lStick.y * front * velocity;
        // Left/Right
        position += lStick.x * right * velocity;
    }

    void FreeCamera::Rotate(f32 frameDelta)
    {
        auto& inputs = Engine::Inputs::GetInstance();

        auto speed = CAMERA_SENSITIVITY * frameDelta;

        // Avoids freaking out
        if (inputs.WasMouseMoved())
        {
            // Yaw
            rotation.y += glm::radians(static_cast<f32>(inputs.GetMousePosition().x) * speed);
            // Pitch
            rotation.x += glm::radians(static_cast<f32>(inputs.GetMousePosition().y) * speed);
        }

        auto rStick = inputs.GetRStick();
        // Pitch
        rotation.x += rStick.y * speed * 0.04f;
        // Yaw
        rotation.y += rStick.x * speed * 0.04f;

        // Don't really want to flip the world around
        rotation.x = glm::clamp(rotation.x, glm::radians(-89.0f), glm::radians(89.0f));
    }

    void FreeCamera::Zoom(f32 frameDelta)
    {
        auto& inputs = Engine::Inputs::GetInstance();

        // Stops things from going haywire
        if (inputs.WasMouseScrolled())
        {
            FOV -= static_cast<f32>(inputs.GetMouseScroll().y) * CAMERA_ZOOM * frameDelta;
            FOV = glm::clamp(FOV, glm::radians(10.0f), glm::radians(120.0f));
        }
    }
}