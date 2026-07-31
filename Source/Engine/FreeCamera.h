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

#ifndef FREE_CAMERA_H
#define FREE_CAMERA_H

#include "Camera.h"
#include "Engine/Inputs.h"
#include "Externals/GLM.h"

namespace Engine
{
    class FreeCamera final : public Engine::Camera
    {
    public:
        FreeCamera() = default;

        FreeCamera
        (
            const glm::vec3& position,
            const glm::vec3& rotation,
            f32 FOV,
            f32 speed,
            f32 sprint,
            f32 sensitivity,
            f32 zoom
        );

        void Update(const Engine::FrameCounter& frameCounter, const Engine::Inputs& inputs) override;
        void ImGuiDisplay() override;

        f32 speed       = 15.0f;
        f32 sprint      = 1.85f;
        f32 sensitivity = 100.0f;
        f32 zoom        = 45.0f;
    private:
        void Rotate(const Engine::FrameCounter& frameCounter, const Engine::Inputs& inputs);
        void Move(const Engine::FrameCounter& frameCounter, const Engine::Inputs& inputs);
        void Zoom(const Engine::FrameCounter& frameCounter, const Engine::Inputs& inputs);

        glm::vec3 m_targetPosition    = {};
        glm::quat m_targetOrientation = glm::identity<glm::quat>();
        f32       m_targetFOV         = 0.0f;

        glm::quat m_yaw   = glm::identity<glm::quat>();
        glm::quat m_pitch = glm::identity<glm::quat>();
    };
}

#endif
