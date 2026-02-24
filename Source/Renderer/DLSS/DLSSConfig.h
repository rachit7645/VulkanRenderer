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

#ifndef DLSS_CONFIG_H
#define DLSS_CONFIG_H

#include "Vulkan/Context.h"
#include "Externals/DLSS.h"
#include "Vulkan/CommandBuffer.h"

namespace Renderer::DLSS
{
    class DLSSConfig
    {
    public:
        explicit DLSSConfig(const Vk::Context& context);

        void Destroy(VkDevice device);

        bool isSupported = false;

        [[nodiscard]] glm::uvec2 GetInternalResolution(const glm::uvec2& swapchainSize);

        void UpdateDLSSFeature
        (
            const Vk::CommandBuffer& cmdBuffer,
            const glm::uvec2& swapchainSize,
            Util::DeletionQueue& deletionQueue
        );

        NVSDK_NGX_Parameter* parameters = nullptr;
        NVSDK_NGX_Handle*    handle     = nullptr;

        glm::uvec2                  optimalResolution = {};
        NVSDK_NGX_PerfQuality_Value DLSSMode          = NVSDK_NGX_PerfQuality_Value_DLAA;
    
        bool resetNeeded = true;
    private:
        bool m_haveParametersChanged = true;

        glm::uvec2 m_oldOptimalResolution = {};
        glm::uvec2 m_oldSwapchainSize     = {};

        glm::uvec2 m_minResolution = {};
        glm::uvec2 m_maxResolution = {};
    };
}

#endif