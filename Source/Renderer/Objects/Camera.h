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

#ifndef CAMERA_H
#define CAMERA_H

#include "Externals/GLM.h"
#include "Renderer/RenderConstants.h"
#include "Engine/Inputs.h"
#include "Util/FrameCounter.h"

namespace Renderer::Objects
{
    class Camera
    {
    public:
        Camera() = default;
        Camera(const glm::vec3& position, const glm::vec3& rotation, f32 FOV);
        virtual ~Camera() = default;

        Camera(const Camera&) noexcept = default;
        Camera& operator=(const Camera&) noexcept = default;

        Camera(Camera&& other) noexcept = default;
        Camera& operator=(Camera&& other) noexcept = default;

        virtual void Update(const Util::FrameCounter& frameCounter, const Engine::Inputs& inputs) = 0;
        virtual void ImGuiDisplay();

        [[nodiscard]] glm::mat4 GetViewMatrix() const;

        glm::vec3 position    = {0.0f, 0.0f, 0.0f};
        glm::quat orientation = glm::identity<glm::quat>();
        f32       FOV         = Renderer::DEFAULT_FOV;

        bool isEnabled = true;
    private:
        bool m_enableQuaternionInputMode = false;
    };
}

#endif
