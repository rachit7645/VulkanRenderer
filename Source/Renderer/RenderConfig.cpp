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
        multiQueue.isSupported = context.queueFamilies.HasAllFamilies();
        multiQueue.isEnabled   = multiQueue.isSupported;

        #ifdef ENGINE_DLSS
        // Validation issues workaround
        constexpr bool FORCE_DISABLE_DLSS = false;

        DLSS.isSupported = DLSSConfig.isSupported;
        antiAliasingMode = (DLSS.isSupported && !FORCE_DISABLE_DLSS) ? AntiAliasingMode::DLSS : AntiAliasingMode::TAA;
        DLSS.isEnabled   = antiAliasingMode == AntiAliasingMode::DLSS;
        #else
        DLSS.isSupported = false;
        DLSS.isEnabled   = false;
        antiAliasingMode = AntiAliasingMode::TAA;
        #endif

        m_currentAntiAliasingMode = static_cast<s32>(std::to_underlying(antiAliasingMode));
    }

    void RenderConfig::Update()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Renderer"))
            {
                if (ImGui::CollapsingHeader("Config"))
                {
                    ImGui::Checkbox("Multi-Queue", &multiQueue.isEnabled);

                    ImGui::Separator();

                    // Anti-aliasing mode selection
                    {
                        constexpr std::array ANTI_ALIASING_NAMES =
                        {
                            "None",
                            "TAA",
                            #ifdef ENGINE_DLSS
                            "DLSS"
                            #endif
                        };

                        constexpr std::array ANTI_ALIASING_MODES =
                        {
                            AntiAliasingMode::None,
                            AntiAliasingMode::TAA,
                            #ifdef ENGINE_DLSS
                            AntiAliasingMode::DLSS
                            #endif
                        };

                        #ifdef ENGINE_DLSS
                        const s32 itemCount = DLSS.isSupported ? ANTI_ALIASING_NAMES.size() : ANTI_ALIASING_NAMES.size() - 1;
                        #else
                        constexpr s32 itemCount = ANTI_ALIASING_NAMES.size();
                        #endif

                        if (ImGui::Combo("Anti-Aliasing Mode", &m_currentAntiAliasingMode, ANTI_ALIASING_NAMES.data(), itemCount))
                        {
                            antiAliasingMode = ANTI_ALIASING_MODES[m_currentAntiAliasingMode];
                        }

                        #ifdef ENGINE_DLSS
                        // This field is unused, but I'll keep it updated
                        DLSS.isEnabled = antiAliasingMode == AntiAliasingMode::DLSS;
                        #endif
                    }

                    ImGui::Separator();

                    #ifdef ENGINE_DLSS
                    if (antiAliasingMode != AntiAliasingMode::DLSS)
                    #endif
                    {
                        ImGui::DragFloat("Resolution Scale", &resolutionScale, 0.01f, 0.1f, 0.0f);
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        Validate();
    }

    void RenderConfig::Validate()
    {
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
