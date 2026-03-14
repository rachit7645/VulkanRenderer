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
    RenderConfig::RenderConfig(const Vk::Context& context)
    #ifdef ENGINE_DLSS
        : DLSSConfig{context}
    #endif
    {
        rayTracing.isSupported = context.extensions.HasRayTracing();
        rayTracing.isEnabled   = rayTracing.isSupported;

        // If ray tracing is not available then it would be slower, so disable it
        multiQueue.isSupported = context.queueFamilies.HasAllFamilies();
        multiQueue.isEnabled   = multiQueue.isSupported && rayTracing.isSupported;

        #ifdef ENGINE_DLSS
        DLSS.isSupported = DLSSConfig.isSupported;
        DLSS.isEnabled   = DLSS.isSupported;
        #else
        DLSS.isSupported = false;
        DLSS.isEnabled   = DLSS.isSupported;
        #endif
    }

    void RenderConfig::Update()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Renderer"))
            {
                if (ImGui::CollapsingHeader("Config"))
                {
                    ImGui::Checkbox("Raytracing",  &rayTracing.isEnabled);
                    ImGui::Checkbox("Multi-Queue", &multiQueue.isEnabled);
                    ImGui::Checkbox("DLSS",        &DLSS.isEnabled);
                }

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
        DLSS.Validate();
    }

    void RenderConfig::Destroy(ENGINE_UNUSED VkDevice device)
    {
        #ifdef ENGINE_DLSS
        DLSSConfig.Destroy(device);
        #endif
    }

    void RenderConfig::Entry::Validate()
    {
        isEnabled = isEnabled && isSupported;
    }
}
