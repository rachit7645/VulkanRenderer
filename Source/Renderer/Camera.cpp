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

#include "Camera.h"

namespace Renderer
{
    Camera::Camera(const glm::vec3& position, const glm::vec3& rotation, f32 FOV)
        : position(position),
          rotation(rotation),
          FOV(FOV)
    {
    }

    glm::mat4 Camera::GetViewMatrix() const
    {
        // Return
        return glm::lookAt(position, position + front, up);
    }

    void Camera::ImGuiDisplay()
    {
        // Render ImGui
        if (ImGui::BeginMainMenuBar())
        {
            // If camera menu is visible
            if (ImGui::BeginMenu("Camera"))
            {
                // Camera position
                ImGui::DragFloat3("Position", &position[0], 1.0f, 0.0f, 0.0f, "%.1f");
                // Camera rotation
                ImGui::DragFloat3("Rotation", &rotation[0], 1.0f, 0.0f, 0.0f, "%.1f");

                // Camera front vector
                ImGui::DragFloat3("Front", &front[0], 1.0f, 0.0f, 0.0f, "%.1f");
                // Camera up vector
                ImGui::DragFloat3("Up", &up[0], 1.0f, 0.0f, 0.0f, "%.1f");
                // Camera right vector
                ImGui::DragFloat3("Right", &right[0], 1.0f, 0.0f, 0.0f, "%.1f");
                // Camera FOV
                ImGui::DragFloat("FOV", &FOV, 1.0f, 0.0f, 0.0f, "%.1f");

                // End menu
                ImGui::EndMenu();
            }
            // End menu bar
            ImGui::EndMainMenuBar();
        }
    }
}