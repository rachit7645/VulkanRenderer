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
#include "PipelineID.h"
#include "Externals/UnorderedDense.h"

namespace Vk
{
    class PipelineManager
    {
    public:
        void AddPipeline(Vk::PipelineID id, const Vk::PipelineConfig& config);

        void Update
        (
            VkDevice device,
            Scratch::Allocator& scratchAllocator,
            tf::Executor& executor,
            Util::DeletionQueue& deletionQueue
        );

        [[nodiscard]] const Vk::Pipeline& GetPipeline(Vk::PipelineID id) const;

        void ImGuiDisplay();

        void Destroy(VkDevice device);
    private:
        struct BuiltPipeline
        {
            Vk::PipelineID  id       = {};
            Vk::Pipeline    pipeline = {};
        };

        PipelineManager::BuiltPipeline BuildPipeline
        (
            VkDevice device,
            Vk::PipelineID id,
            Vk::PipelineConfig& config
        );

        void RecompilePipelineShaders(Vk::PipelineID id);

        [[nodiscard]] std::string_view GetPipelineName(Vk::PipelineID id) const;

        ankerl::unordered_dense::map<Vk::PipelineID, Vk::Pipeline>       m_pipelines;
        ankerl::unordered_dense::map<Vk::PipelineID, Vk::PipelineConfig> m_pipelineConfigs;
        ankerl::unordered_dense::map<Vk::PipelineID, Vk::PipelineConfig> m_dirtyPipelineConfigs;
        ankerl::unordered_dense::set<Vk::PipelineID>                     m_reloadRequests;

        std::mutex m_pipelineConfigsMutex;
        std::mutex m_dirtyPipelineConfigsMutex;
    };
}

#endif
