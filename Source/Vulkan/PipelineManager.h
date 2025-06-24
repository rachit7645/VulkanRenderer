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

#ifndef PIPELINE_MANAGER_H
#define PIPELINE_MANAGER_H

#include <vulkan/vulkan.h>

#include "Pipeline.h"
#include "PipelineConfig.h"
#include "Externals/UnorderedDense.h"

namespace Vk
{
    class PipelineManager
    {
    public:
        void AddPipeline(const std::string_view id, const Vk::PipelineConfig& config);

        void Update(VkDevice device, Util::DeletionQueue& deletionQueue);
        void Reload();

        [[nodiscard]] Vk::Pipeline& GetPipeline(const std::string_view id);
        [[nodiscard]] const Vk::Pipeline& GetPipeline(const std::string_view id) const;

        void ImGuiDisplay();

        void Destroy(VkDevice device);
    private:
        ankerl::unordered_dense::map<std::string, Vk::Pipeline>       m_pipelines;
        ankerl::unordered_dense::map<std::string, Vk::PipelineConfig> m_pipelineConfigs;
        ankerl::unordered_dense::map<std::string, Vk::PipelineConfig> m_dirtyPipelineConfigs;
    };
}

#endif
