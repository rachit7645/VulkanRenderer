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

#ifndef FREE_CAMERA_H
#define FREE_CAMERA_H

#include "Camera.h"
#include "Engine/Inputs.h"
#include "Externals/GLM.h"

namespace Renderer::Objects
{
    class FreeCamera : public Camera
    {
    public:
        FreeCamera() = default;

        FreeCamera
        (
            const glm::vec3& position,
            const glm::vec3& rotation,
            f32 FOV,
            f32 exposure,
            f32 speed,
            f32 sensitivity,
            f32 zoom
        );

        void Update(f32 frameDelta, Engine::Inputs& inputs) override;
        void ImGuiDisplay() override;
    private:
        void CheckInputs(f32 frameDelta, Engine::Inputs& inputs);
        void Move(f32 frameDelta, Engine::Inputs& inputs);
        void Rotate(f32 frameDelta, Engine::Inputs& inputs);
        void Zoom(f32 frameDelta, Engine::Inputs& inputs);

        // Settings
        f32 m_speed       = 0.000015f;
        f32 m_sensitivity = 0.0001f;
        f32 m_zoom        = 0.000045f;
    };
}

#endif
