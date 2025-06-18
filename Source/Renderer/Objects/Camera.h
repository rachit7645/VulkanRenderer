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

#ifndef CAMERA_H
#define CAMERA_H

#include "Externals/GLM.h"
#include "Renderer/RenderConstants.h"
#include "Engine/Inputs.h"

namespace Renderer::Objects
{
    class Camera
    {
    public:
        Camera() = default;
        Camera(const glm::vec3& position, const glm::vec3& rotation, f32 FOV, f32 exposure);
        virtual ~Camera() = default;

        Camera(const Camera&) noexcept = default;
        Camera& operator=(const Camera&) noexcept = default;

        Camera(Camera&& other) noexcept = default;
        Camera& operator=(Camera&& other) noexcept = default;

        // Each subclass MUST define this method
        virtual void Update(f32 frameDelta, Engine::Inputs& inputs) = 0;
        virtual void ImGuiDisplay();

        [[nodiscard]] glm::mat4 GetViewMatrix() const;

        glm::vec3 position = {0.0f, 0.0f, 0.0f};
        // Pitch, Yaw, Roll (Radians)
        glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
        // Must be in radians
        f32 FOV = Renderer::DEFAULT_FOV;
        // In EV
        f32 exposure = 0.0f;

        glm::vec3 front = {0.0f, 0.0f, -1.0f};
        glm::vec3 up    = {0.0f, 1.0f,  0.0f};
        glm::vec3 right = glm::normalize(glm::cross(front, up));

        bool isEnabled = true;
    };
}

#endif
