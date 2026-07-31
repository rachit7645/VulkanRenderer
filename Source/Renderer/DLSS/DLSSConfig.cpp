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

#include "DLSSConfig.h"

#include "Util/Log.h"
#include "Vulkan/CommandBuffer.h"

namespace Renderer::DLSS
{
    DLSSConfig::DLSSConfig(const Vk::Context& context)
    {
        NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init_with_ProjectID
        (
            ::DLSS::PROJECT_ID,
            NVSDK_NGX_ENGINE_TYPE_CUSTOM,
            ::DLSS::ENGINE_VERSION,
            ::DLSS::APPLICATION_DATA_PATH,
            context.instance,
            context.physicalDevice,
            context.device,
            vkGetInstanceProcAddr,
            vkGetDeviceProcAddr,
            &::DLSS::FEATURE_COMMON_INFO,
            NVSDK_NGX_Version_API
        );

        if (result != NVSDK_NGX_Result_Success)
        {
            isSupported = false;

            return;
        }

        result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters);

        if (result != NVSDK_NGX_Result_Success)
        {
            isSupported = false;

            return;
        }

        s32 DLSSSupported = 0;

        result = parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &DLSSSupported);

        if (result != NVSDK_NGX_Result_Success || DLSSSupported == 0)
        {
            isSupported = false;

            return;
        }

        // DLSS is Supported!
        isSupported = true;
    }

    glm::uvec2 DLSSConfig::GetInternalResolution(const glm::uvec2& swapchainSize)
    {
        if (!isSupported)
        {
            Logger::Error("{}\n", "DLSS is not supported!");
        }

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Effects"))
            {
                if (ImGui::CollapsingHeader("DLSS"))
                {
                    ImGui::Text("Optimal Resolution | [%u, %u]", optimalResolution.x, optimalResolution.y);
                    ImGui::Text("Min Resolution     | [%u, %u]", m_minResolution.x,   m_minResolution.y);
                    ImGui::Text("Max Resolution     | [%u, %u]", m_maxResolution.x,   m_maxResolution.y);

                    ImGui::Separator();

                    usize allocatedVRAM      = 0;
                    u32   optimizationLevel  = 0;
                    u32   isDevSnippetBranch = 0;

                    const NVSDK_NGX_Result result =  NGX_DLSS_GET_STATS_2
                    (
                        parameters,
                        &allocatedVRAM,
                        &optimizationLevel,
                        &isDevSnippetBranch
                    );

                    if (result != NVSDK_NGX_Result_Success)
                    {
                        Logger::Error("Failed to query DLSS statistics! [Error={}]\n", static_cast<u64>(result));
                    }

                    ImGui::Text("Allocated VRAM     | %llu bytes", allocatedVRAM);
                    ImGui::Text("Optimization Level | %u",         optimizationLevel);
                    ImGui::Text("Dev Branch         | %s",         isDevSnippetBranch ? "true" : "false");

                    ImGui::Separator();

                    constexpr std::array DLSS_MODE_NAMES =
                    {
                        "Performance",
                        "Balanced",
                        "Quality",
                        "Ultra Performance",
                        "DLAA"
                    };

                    constexpr std::array DLSS_MODE_VALUES =
                    {
                        NVSDK_NGX_PerfQuality_Value_MaxPerf,
                        NVSDK_NGX_PerfQuality_Value_Balanced,
                        NVSDK_NGX_PerfQuality_Value_MaxQuality,
                        NVSDK_NGX_PerfQuality_Value_UltraPerformance,
                        NVSDK_NGX_PerfQuality_Value_DLAA
                    };

                    s32 currentModeIndex = 0;

                    for (s32 i = 0; i < static_cast<s32>(DLSS_MODE_VALUES.size()); ++i)
                    {
                        if (DLSS_MODE_VALUES[i] == DLSSMode)
                        {
                            currentModeIndex = i;

                            break;
                        }
                    }

                    if (ImGui::Combo("Mode", &currentModeIndex, DLSS_MODE_NAMES.data(), DLSS_MODE_NAMES.size()))
                    {
                        DLSSMode                = DLSS_MODE_VALUES[currentModeIndex];
                        m_haveParametersChanged = true;
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        f32 sharpness = 0.0f;

        if (m_oldSwapchainSize != swapchainSize)
        {
            m_haveParametersChanged = true;
        }

        m_oldSwapchainSize = swapchainSize;

        NVSDK_NGX_Result result = NGX_DLSS_GET_OPTIMAL_SETTINGS
        (
            parameters,
            swapchainSize.x,
            swapchainSize.y,
            DLSSMode,
            &optimalResolution.x,
            &optimalResolution.y,
            &m_maxResolution.x,
            &m_maxResolution.y,
            &m_minResolution.x,
            &m_minResolution.y,
            &sharpness
        );

        if (result != NVSDK_NGX_Result_Success)
        {
            Logger::Info("Could not query optimal settings! [Error={}]", static_cast<u64>(result));
        }

        if (optimalResolution.x == 0 || optimalResolution.y == 0)
        {
            Logger::Error("{}\n", "Invalid optimal resolution!");
        }

        if (m_oldOptimalResolution != optimalResolution)
        {
            m_haveParametersChanged = true;
        }

        m_oldOptimalResolution = optimalResolution;

        return optimalResolution;
    }

    void DLSSConfig::UpdateDLSSFeature
    (
        const Vk::CommandBuffer& cmdBuffer,
        const glm::uvec2& swapchainSize,
        Engine::DeletionQueue& deletionQueue
    )
    {
        if (!isSupported)
        {
            Logger::Error("{}\n", "DLSS is not supported!");
        }

        if (!m_haveParametersChanged && handle != nullptr)
        {
            return;
        }

        if (handle != nullptr)
        {
            deletionQueue.Push([_handle = handle] ()
            {
                const NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_ReleaseFeature(_handle);

                if (result != NVSDK_NGX_Result_Success)
                {
                    Logger::Error("Failed to release DLSS Feature! [Error={}]", static_cast<u64>(result));
                }
            });
        }

        NVSDK_NGX_DLSS_Create_Params DLSSCreateParameters =
        {
            .Feature                = {
                .InWidth            = optimalResolution.x,
                .InHeight           = optimalResolution.y,
                .InTargetWidth      = swapchainSize.x,
                .InTargetHeight     = swapchainSize.y,
                .InPerfQualityValue = DLSSMode
            },
            .InFeatureCreateFlags   = NVSDK_NGX_DLSS_Feature_Flags_IsHDR |
                                      NVSDK_NGX_DLSS_Feature_Flags_DepthInverted |
                                      NVSDK_NGX_DLSS_Feature_Flags_MVLowRes,
            .InEnableOutputSubrects = false
        };

        NVSDK_NGX_Result result = NGX_VULKAN_CREATE_DLSS_EXT
        (
            cmdBuffer.handle,
            1,
            1,
            &handle,
            parameters,
            &DLSSCreateParameters
        );

        if (result != NVSDK_NGX_Result_Success)
        {
            Logger::Error("Failed to create DLSS Feature! [Error={}]", static_cast<u64>(result));
        }

        resetNeeded             = true;
        m_haveParametersChanged = false;
    }

    void DLSSConfig::Destroy(VkDevice device) const
    {
        if (!isSupported)
        {
            Logger::Error("{}\n", "DLSS is not supported!");
        }

        NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_DestroyParameters(parameters);

        if (result != NVSDK_NGX_Result_Success)
        {
            Logger::Error("Failed to destroy DLSS parameters! [Error={}]", static_cast<u64>(result));
        }

        result = NVSDK_NGX_VULKAN_Shutdown1(device);

        if (result != NVSDK_NGX_Result_Success)
        {
            Logger::Error("Failed to shutdown DLSS SDK! [Error={}]", static_cast<u64>(result));
        }
    }
}
