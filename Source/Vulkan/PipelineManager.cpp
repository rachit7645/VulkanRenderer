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

#include "PipelineManager.h"

#include "DebugUtils.h"
#include "Util.h"
#include "Util/Log.h"

namespace Vk
{
    void PipelineManager::AddPipeline(const std::string_view id, const Vk::PipelineConfig& config)
    {
        m_dirtyPipelineConfigs.emplace(id, config);
        m_pipelineConfigs.emplace(id, config);
    }

    void PipelineManager::Update(VkDevice device, Util::DeletionQueue& deletionQueue)
    {
        if (m_dirtyPipelineConfigs.empty())
        {
            return;
        }

        std::vector<VkGraphicsPipelineCreateInfo> graphicsPipelineCreateInfos;
        std::vector<std::string>                  graphicsPipelineIDs;

        std::vector<VkComputePipelineCreateInfo> computePipelineCreateInfos;
        std::vector<std::string>                 computePipelinesIDs;

        std::vector<VkRayTracingPipelineCreateInfoKHR> rayTracingPipelineCreateInfos;
        std::vector<std::string>                       rayTracingPipelinesIDs;

        for (auto& [id, config] : m_dirtyPipelineConfigs)
        {
            config.Build(device);

            auto& pipeline = m_pipelines.emplace(id, Vk::Pipeline{}).first->second;

            if (pipeline.handle != VK_NULL_HANDLE)
            {
                deletionQueue.PushDeletor([device, _pipeline = pipeline] ()
                {
                    _pipeline.Destroy(device);
                });
            }

            pipeline.bindPoint = config.GetPipelineType();
            pipeline.layout    = config.BuildLayout(device);

            Vk::SetDebugName(device, pipeline.layout, id + "/Pipeline/Layout");

            switch (pipeline.bindPoint)
            {
            case VK_PIPELINE_BIND_POINT_GRAPHICS:
                graphicsPipelineCreateInfos.emplace_back(config.BuildGraphicsPipelineCreateInfo(pipeline.layout));
                graphicsPipelineIDs.emplace_back(id);
                break;

            case VK_PIPELINE_BIND_POINT_COMPUTE:
                computePipelineCreateInfos.emplace_back(config.BuildComputePipelineCreateInfo(pipeline.layout));
                computePipelinesIDs.emplace_back(id);
                break;

            case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
                rayTracingPipelineCreateInfos.emplace_back(config.BuildRayTracingPipelineCreateInfo(pipeline.layout));
                rayTracingPipelinesIDs.emplace_back(id);
                break;

            default:
                continue;
            }
        }

        Logger::Debug
        (
            "Compiling pipelines! [Total={}] [Graphics={}] [Compute={}] [RayTracing={}]\n",
            m_dirtyPipelineConfigs.size(),
            graphicsPipelineCreateInfos.size(),
            computePipelineCreateInfos.size(),
            rayTracingPipelineCreateInfos.size()
        );

        if (!graphicsPipelineIDs.empty())
        {
            auto handles = std::vector<VkPipeline>(graphicsPipelineCreateInfos.size());

            Vk::CheckResult(vkCreateGraphicsPipelines(
                device,
                nullptr,
                graphicsPipelineCreateInfos.size(),
                graphicsPipelineCreateInfos.data(),
                nullptr,
                handles.data()),
                "Failed to create graphics pipelines!"
            );

            for (usize i = 0; i < graphicsPipelineIDs.size(); ++i)
            {
                const auto& id     = graphicsPipelineIDs[i];
                const auto  handle = handles[i];

                auto& pipeline = GetPipeline(id);

                pipeline.handle = handle;

                Vk::SetDebugName(device, pipeline.handle, id + "/Pipeline");
            }
        }

        if (!computePipelinesIDs.empty())
        {
            auto handles = std::vector<VkPipeline>(computePipelineCreateInfos.size());

            Vk::CheckResult(vkCreateComputePipelines(
                device,
                nullptr,
                computePipelineCreateInfos.size(),
                computePipelineCreateInfos.data(),
                nullptr,
                handles.data()),
                "Failed to create compute pipelines!"
            );

            for (usize i = 0; i < computePipelinesIDs.size(); ++i)
            {
                const auto& id     = computePipelinesIDs[i];
                const auto  handle = handles[i];

                auto& pipeline = GetPipeline(id);

                pipeline.handle = handle;

                Vk::SetDebugName(device, pipeline.handle, id + "/Pipeline");
            }
        }

        if (!rayTracingPipelinesIDs.empty())
        {
            auto handles = std::vector<VkPipeline>(rayTracingPipelineCreateInfos.size());

            Vk::CheckResult(vkCreateRayTracingPipelinesKHR(
                device,
                VK_NULL_HANDLE,
                VK_NULL_HANDLE,
                rayTracingPipelineCreateInfos.size(),
                rayTracingPipelineCreateInfos.data(),
                nullptr,
                handles.data()),
                "Failed to create ray tracing pipelines!"
            );

            for (usize i = 0; i < rayTracingPipelinesIDs.size(); ++i)
            {
                const auto& id     = rayTracingPipelinesIDs[i];
                const auto  handle = handles[i];

                auto& pipeline = GetPipeline(id);

                pipeline.handle = handle;

                Vk::SetDebugName(device, pipeline.handle, id + "/Pipeline");
            }
        }

        for (auto& config : m_dirtyPipelineConfigs | std::views::values)
        {
            config.Destroy(device);
        }

        m_dirtyPipelineConfigs.clear();
    }

    void PipelineManager::Reload(const std::string_view id)
    {
        const auto iter = m_pipelineConfigs.find(id.data());

        if (iter == m_pipelineConfigs.cend())
        {
            Logger::Error("Can't reload invalid pipeline! [ID={}]\n", id);
        }

        m_dirtyPipelineConfigs.emplace(iter->first, iter->second);
    }

    void PipelineManager::ReloadAll()
    {
        m_dirtyPipelineConfigs.insert(m_pipelineConfigs.begin(), m_pipelineConfigs.end());
    }

    Vk::Pipeline& PipelineManager::GetPipeline(const std::string_view id)
    {
        auto iter = m_pipelines.find(id.data());

        if (iter == m_pipelines.end())
        {
            Logger::Error("Failed to find pipeline! [ID={}]\n", id);
        }

        return iter->second;
    }

    const Vk::Pipeline& PipelineManager::GetPipeline(const std::string_view id) const
    {
        const auto iter = m_pipelines.find(id.data());

        if (iter == m_pipelines.cend())
        {
            Logger::Error("Failed to find pipeline! [ID={}]\n", id);
        }

        return iter->second;
    }

    void PipelineManager::ImGuiDisplay()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Pipeline Manager"))
            {
                if (ImGui::TreeNode("Reload"))
                {
                    if (ImGui::Button("Reload All Pipelines"))
                    {
                        ReloadAll();
                    }

                    ImGui::TreePop();
                }

                ImGui::Separator();

                for (const auto& [id, pipeline] : m_pipelines)
                {
                    if (ImGui::TreeNode(std::bit_cast<void*>(pipeline.handle), "%s", id.c_str()))
                    {
                        ImGui::Text("Handle     | %p", std::bit_cast<void*>(pipeline.handle));
                        ImGui::Text("Layout     | %p", std::bit_cast<void*>(pipeline.layout));
                        ImGui::Text("Bind Point | %s", string_VkPipelineBindPoint(pipeline.bindPoint));

                        if (ImGui::Button("Reload Pipeline"))
                        {
                            Reload(id);
                        }

                        ImGui::TreePop();
                    }

                    ImGui::Separator();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void PipelineManager::Destroy(VkDevice device)
    {
        for (const auto& pipeline : m_pipelines | std::views::values)
        {
            pipeline.Destroy(device);
        }

        m_pipelines.clear();
        m_dirtyPipelineConfigs.clear();
    }
}