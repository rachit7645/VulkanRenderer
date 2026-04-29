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

#ifndef PIPELINE_MANAGER_H
#define PIPELINE_MANAGER_H

#include <vulkan/vulkan.h>

#include "Pipeline.h"
#include "PipelineConfig.h"
#include "Externals/UnorderedDense.h"
#include "Util/String.h"

namespace Vk
{
    class PipelineManager
    {
    public:
        void AddPipeline(const std::string_view id, const Vk::PipelineConfig& config);

        void Update
        (
            VkDevice device,
            tf::Executor& executor,
            Util::DeletionQueue& deletionQueue
        );

        [[nodiscard]] Vk::Pipeline& GetPipeline(const std::string_view id);
        [[nodiscard]] const Vk::Pipeline& GetPipeline(const std::string_view id) const;

        void ImGuiDisplay();

        void Destroy(VkDevice device);
    private:
        struct BuiltPipeline
        {
            std::string  id       = "Null/Pipeline";
            Vk::Pipeline pipeline = {};
        };

        PipelineManager::BuiltPipeline BuildPipeline
        (
            VkDevice device,
            const std::string& id,
            Vk::PipelineConfig& config
        );

        void RecompilePipelineShaders(const std::string_view id);

        ankerl::unordered_dense::map<std::string, Vk::Pipeline,       Util::StringHash, std::equal_to<>> m_pipelines;
        ankerl::unordered_dense::map<std::string, Vk::PipelineConfig, Util::StringHash, std::equal_to<>> m_pipelineConfigs;

        ankerl::unordered_dense::map<std::string, Vk::PipelineConfig, Util::StringHash, std::equal_to<>> m_dirtyPipelineConfigs;

        ankerl::unordered_dense::set<std::string, Util::StringHash, std::equal_to<>> m_reloadRequests;

        std::mutex m_pipelineConfigsMutex;
        std::mutex m_dirtyPipelineConfigsMutex;
    };
}

#endif
