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

#include "RenderConfig.h"

#include "Externals/ImGui.h"

namespace Renderer
{
    RenderConfig::RenderConfig(const Vk::QueueFamilies& queueFamilies, const Vk::Extensions& extensions)
    {
        rayTracing.isSupported = extensions.HasRayTracing();
        rayTracing.isEnabled   = rayTracing.isSupported;

        // If ray tracing is not available then it would be slower, so disable it
        multiQueue.isSupported = queueFamilies.HasAllFamilies();
        multiQueue.isEnabled   = multiQueue.isSupported && rayTracing.isSupported;
    }

    void RenderConfig::Update()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Renderer Config"))
            {
                ImGui::Checkbox("Raytracing",  &rayTracing.isEnabled);
                ImGui::Checkbox("Multi-Queue", &multiQueue.isEnabled);

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        Validate();
    }

    void RenderConfig::Validate()
    {
        rayTracing.Validate();
        multiQueue.Validate();
    }

    void RenderConfig::Entry::Validate()
    {
        isEnabled = isEnabled && isSupported;
    }
}