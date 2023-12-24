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

#include <numbers>

#include "FreeCamera.h"
#include "RenderConstants.h"
#include "Engine/Inputs.h"

namespace Renderer
{
    // Constants
    constexpr f32 CAMERA_SPEED       = 0.00025f;
    constexpr f32 CAMERA_SENSITIVITY = 0.0001f;
    constexpr f32 CAMERA_ZOOM        = 0.00045f;

    FreeCamera::FreeCamera()
        : FreeCamera(glm::vec3(0.0f), glm::vec3(0.0f, -std::numbers::pi / 2.0f, 0.0f), Renderer::DEFAULT_FOV)
    {
    }

    FreeCamera::FreeCamera(const glm::vec3& position, const glm::vec3& rotation, f32 FOV)
        : Camera(position, rotation, FOV)
    {
    }

    void FreeCamera::Update(f32 frameDelta)
    {
        // Check inputs
        CheckInputs(frameDelta);

        // Calculate front vector
        front.x = std::cos(rotation.y) * std::cos(rotation.x);
        front.y = std::sin(rotation.x);
        front.z = std::sin(rotation.y) * std::cos(rotation.x);
        front   = glm::normalize(front);

        // Calculate right & up vectors
        right = glm::normalize(glm::cross(front, Renderer::WORLD_UP));
        up    = glm::normalize(glm::cross(right, front));

        // Display ImGui widget
        ImGuiDisplay();
    }

    void FreeCamera::CheckInputs(f32 frameDelta)
    {
        // Move camera
        Move(frameDelta);
        // Rotate camera
        Rotate(frameDelta);
        // Zoom camera (FIXME: Broken AF)
        // Zoom(frameDelta);
    }

    void FreeCamera::Move(f32 frameDelta)
    {
        // Get inputs
        const auto& inputs = Engine::Inputs::GetInstance();

        // Calculate velocity
        f32 velocity = CAMERA_SPEED * frameDelta;

        // Move forward
        if (inputs.IsKeyPressed(SDL_SCANCODE_W))
        {
            position += front * velocity;
        }
        // Move backward
        else if (inputs.IsKeyPressed(SDL_SCANCODE_S))
        {
            position -= front * velocity;
        }

        // Move left
        if (inputs.IsKeyPressed(SDL_SCANCODE_A))
        {
            position -= right * velocity;
        }
        // Move right
        else if (inputs.IsKeyPressed(SDL_SCANCODE_D))
        {
            position += right * velocity;
        }
    }

    void FreeCamera::Rotate(f32 frameDelta)
    {
        // Get inputs
        auto& inputs = Engine::Inputs::GetInstance();

        // If mouse was moved
        if (inputs.wasMouseMoved)
        {
            // Yaw
            rotation.y += glm::radians(static_cast<f32>(inputs.GetMousePosition().x) * CAMERA_SENSITIVITY * frameDelta);
            // Pitch
            rotation.x += glm::radians(static_cast<f32>(inputs.GetMousePosition().y) * CAMERA_SENSITIVITY * frameDelta);

            // Clamp pitch
            rotation.x = glm::clamp(rotation.x, glm::radians(-89.0f), glm::radians(89.0f));
        }
    }

    void FreeCamera::Zoom(f32 frameDelta)
    {
        // Get inputs
        auto& inputs = Engine::Inputs::GetInstance();

        // If mouse was scrolled
        if (inputs.wasMouseScrolled)
        {
            // Set zoom
            FOV *= static_cast<f32>(inputs.GetMouseScroll().y) * CAMERA_ZOOM * frameDelta;
            FOV = glm::clamp(FOV, glm::radians(10.0f), glm::radians(120.0f));
        }
    }
}