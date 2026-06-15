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

#include "Camera.h"

#include "Externals/ImGui.h"

namespace Engine
{
    Camera::Camera
    (
        const glm::vec3& position,
        const glm::vec3& rotation,
        f32 FOV
    )
        : position{position},
          orientation{glm::normalize(glm::quat(rotation))},
          FOV{FOV}
    {
    }

    glm::mat4 Camera::GetViewMatrix() const
    {
        glm::mat4 matrix = glm::mat4(glm::inverse(orientation));
                  matrix = glm::translate(matrix, -position);

        return matrix;
    }

    void Camera::ImGuiDisplay()
    {
        ImGui::DragFloat3("Position", &position[0], 1.0f, 0.0f, 0.0f, "%.2f");

        if (m_enableQuaternionInputMode)
        {
            ImGui::DragFloat4("Orientation", &orientation[0], 0.01f, 0.0f, 0.0f, "%.4f");

            orientation = glm::normalize(orientation);
        }
        else
        {
            glm::vec3 eulerAngles = glm::eulerAngles(orientation);

            constexpr f32 ONE_DEGREE = glm::radians(1.0f);

            ImGui::DragFloat3("Rotation", &eulerAngles[0], ONE_DEGREE, 0.0f, 0.0f, "%.2f");

            orientation = glm::normalize(glm::quat(eulerAngles));
        }

        ImGui::DragFloat("FOV", &FOV, 1.0f, 0.0f, 0.0f, "%.2f");

        ImGui::Separator();

        ImGui::Checkbox("Quaternion Mode", &m_enableQuaternionInputMode);

        ImGui::Separator();
    }
}