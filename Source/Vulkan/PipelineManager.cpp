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

#include "PipelineManager.h"

#include <ranges>
#include <vulkan/vk_enum_string_helper.h>

#include "DebugUtils.h"
#include "Util.h"
#include "Util/Log.h"
#include "Util/Files.h"
#include "Externals/ImGui.h"

namespace Vk
{
    constexpr auto ASSETS_SHADERS_DIR = "Shaders/";
    constexpr auto PYTHON_EXECUTABLE  = "C:/msys64/ucrt64/bin/python.exe";
    constexpr auto SCRIPT_LOCATION    = "../Scripts";

    #ifdef ENGINE_DEBUG
    constexpr auto COMPILATION_FLAGS = "";
    #else
    constexpr auto COMPILATION_FLAGS = "--release";
    #endif

    void PipelineManager::AddPipeline(Vk::PipelineID id, const Vk::PipelineConfig& config)
    {
        if (!m_dirtyPipelineConfigs.contains(id))
        {
            m_dirtyPipelineConfigs.emplace(id, config);
        }

        if (!m_pipelineConfigs.contains(id))
        {
            m_pipelineConfigs.emplace(id, config);
        }
    }

    void PipelineManager::Update
    (
        VkDevice device,
        Scratch::Allocator& scratchAllocator,
        tf::Executor& executor,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (m_dirtyPipelineConfigs.empty() && m_reloadRequests.empty())
        {
            return;
        }

        auto reloadFutures = Scratch::CreateVector<std::future<void>>(scratchAllocator);

        reloadFutures.reserve(m_reloadRequests.size());

        for (auto& id : m_reloadRequests)
        {
            reloadFutures.emplace_back(executor.async([this, id] () mutable
            {
                RecompilePipelineShaders(id);
            }));
        }

        for (auto& future : reloadFutures)
        {
            if (!future.valid())
            {
                Logger::Error("{}\n", "Future is not valid!");
            }

            future.wait();
        }

        auto builtPipelineFutures = Scratch::CreateVector<std::future<BuiltPipeline>>(scratchAllocator);

        builtPipelineFutures.reserve(m_dirtyPipelineConfigs.size());

        for (auto& pipelineConfig : m_dirtyPipelineConfigs)
        {
            builtPipelineFutures.emplace_back(executor.async([this, device, &pipelineConfig] () mutable -> PipelineManager::BuiltPipeline
            {
                return BuildPipeline(device, pipelineConfig.first, pipelineConfig.second);
            }));
        }

        for (auto& future : builtPipelineFutures)
        {
            if (!future.valid())
            {
                Logger::Error("{}\n", "Future is not valid!");
            }

            future.wait();

            const auto builtPipeline = future.get();

            auto iter = m_pipelines.find(builtPipeline.id);

            if (iter != m_pipelines.end())
            {
                if (iter->second.handle != VK_NULL_HANDLE)
                {
                    deletionQueue.Push([device, pipeline = iter->second] ()
                    {
                        pipeline.Destroy(device);
                    });
                }

                iter->second = builtPipeline.pipeline;
            }
            else
            {
                m_pipelines.emplace(builtPipeline.id, builtPipeline.pipeline);
            }
        }

        for (auto& config : m_dirtyPipelineConfigs | std::views::values)
        {
            config.Destroy(device);
        }

        m_reloadRequests.clear();
        m_dirtyPipelineConfigs.clear();
    }

    const Vk::Pipeline& PipelineManager::GetPipeline(Vk::PipelineID id) const
    {
        const auto iter = m_pipelines.find(id);

        if (iter == m_pipelines.cend())
        {
            Logger::Error("Failed to find pipeline! [ID={}]\n", GetPipelineName(id));
        }

        return iter->second;
    }

    void PipelineManager::ImGuiDisplay()
    {
        if (ImGui::CollapsingHeader("Pipelines"))
        {
            if (ImGui::TreeNode("Reload"))
            {
                if (ImGui::Button("Reload All Pipelines"))
                {
                    for (const auto& id : m_pipelineConfigs | std::views::keys)
                    {
                        m_reloadRequests.emplace(id);
                    }
                }

                ImGui::TreePop();
            }

            ImGui::Separator();

            for (const auto& [id, pipeline] : m_pipelines)
            {
                if (ImGui::TreeNode(std::bit_cast<void*>(static_cast<std::uintptr_t>(id)), "%s", GetPipelineName(id).data()))
                {
                    ImGui::Text("Handle     | %p", reinterpret_cast<void*>(pipeline.handle));
                    ImGui::Text("Layout     | %p", reinterpret_cast<void*>(pipeline.layout));
                    ImGui::Text("Bind Point | %s", string_VkPipelineBindPoint(pipeline.bindPoint));

                    if (ImGui::Button("Reload Pipeline"))
                    {
                        m_reloadRequests.emplace(id);
                    }

                    ImGui::TreePop();
                }

                ImGui::Separator();
            }
        }
    }

    PipelineManager::BuiltPipeline PipelineManager::BuildPipeline
    (
        VkDevice device,
        Vk::PipelineID id,
        Vk::PipelineConfig& config
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", id.c_str());
        #endif

        config.Build(device);

        Vk::Pipeline pipeline = {};

        pipeline.bindPoint = config.GetPipelineType();
        pipeline.layout    = config.BuildLayout(device);

        const std::string name = std::string(GetPipelineName(id));

        Vk::SetDebugName(device, pipeline.layout, name + "/Pipeline/Layout");

        // Create Pipeline
        {
            #ifdef ENGINE_PROFILE
            ZoneScopedN("Create Pipeline");
            #endif

            switch (pipeline.bindPoint)
            {
            case VK_PIPELINE_BIND_POINT_GRAPHICS:
            {
                const auto createInfo = config.BuildGraphicsPipelineCreateInfo(pipeline.layout);

                Vk::CheckResult(vkCreateGraphicsPipelines(
                    device,
                    nullptr,
                    1,
                    &createInfo,
                    nullptr,
                    &pipeline.handle),
                    "Failed to create graphics pipelines!"
                );

                break;
            }

            case VK_PIPELINE_BIND_POINT_COMPUTE:
            {
                const auto createInfo = config.BuildComputePipelineCreateInfo(pipeline.layout);

                Vk::CheckResult(vkCreateComputePipelines(
                    device,
                    nullptr,
                    1,
                    &createInfo,
                    nullptr,
                    &pipeline.handle),
                    "Failed to create compute pipeline!"
                );

                break;
            }

            case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
            {
                const auto createInfo = config.BuildRayTracingPipelineCreateInfo(pipeline.layout);

                Vk::CheckResult(vkCreateRayTracingPipelinesKHR(
                    device,
                    VK_NULL_HANDLE,
                    VK_NULL_HANDLE,
                    1,
                    &createInfo,
                    nullptr,
                    &pipeline.handle),
                    "Failed to create ray tracing pipelines!"
                );

                break;
            }

            default:
                Logger::Error("Unsupported pipeline type! [ID={}] [Bind Point={}]\n", name, string_VkPipelineBindPoint(pipeline.bindPoint));
            }
        }

        Vk::SetDebugName(device, pipeline.handle, name + "/Pipeline");

        return PipelineManager::BuiltPipeline
        {
            .id       = id,
            .pipeline = pipeline
        };
    }

    void PipelineManager::RecompilePipelineShaders(Vk::PipelineID id)
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", id.data());
        #endif

        bool reloadSucceeded = true;

        Vk::PipelineConfig pipelineConfig = {};

        // Get shader files
        {
            #ifdef ENGINE_PROFILE
            ZoneScopedN("Pipeline Config Lookup");
            #endif

            const std::scoped_lock lock{m_pipelineConfigsMutex};

            const auto iter = m_pipelineConfigs.find(id);

            if (iter == m_pipelineConfigs.cend())
            {
                Logger::Error("Can't reload an invalid pipeline! [ID={}]\n", GetPipelineName(id));
            }

            pipelineConfig = iter->second;
        }

        for (const auto& [path, _] : pipelineConfig.GetShaders())
        {
            #ifdef ENGINE_PROFILE
            ZoneNamed(zone, true);
            zone.NameFmt("%s", path.c_str());
            #endif

            const auto shaderAssetPath = std::filesystem::absolute(std::filesystem::path("../" + Files::GetAssetPath(ASSETS_SHADERS_DIR, path))).string();

            const auto result = std::system(fmt::format(
                "{} {}/CompileShader.py {} {}",
                PYTHON_EXECUTABLE,
                SCRIPT_LOCATION,
                COMPILATION_FLAGS,
                shaderAssetPath
            ).c_str());

            if (result != 0)
            {
                Logger::Warning("Pipeline Reload Failed! [Result={}]\n", result);

                reloadSucceeded = false;

                break;
            }
        }

        // Append to dirty pipeline configs
        if (reloadSucceeded)
        {
            #ifdef ENGINE_PROFILE
            ZoneScopedN("Dirty Pipeline Config Insertion");
            #endif

            const std::scoped_lock lock{m_dirtyPipelineConfigsMutex};

            m_dirtyPipelineConfigs.emplace(id, pipelineConfig);
        }
    }

    std::string_view PipelineManager::GetPipelineName(Vk::PipelineID id) const
    {
        switch (id)
        {
        case PipelineID::Tonemap:
            return "Tonemap";

        case PipelineID::BloomDownSample:
            return "Bloom/DownSample";

        case PipelineID::BloomDownSampleFirstSample:
            return "Bloom/DownSample/FirstSample";

        case PipelineID::BloomUpSample:
            return "Bloom/UpSample";

        case PipelineID::BloomCombine:
            return "Bloom/Combine";

        case PipelineID::DepthOpaque:
            return "Depth/Opaque";

        case PipelineID::DepthAlphaMasked:
            return "Depth/AlphaMasked";

        case PipelineID::DearImGui:
            return "DearImGui";

        case PipelineID::Lighting:
            return "Lighting";

        case PipelineID::Skybox:
            return "Skybox";

        case PipelineID::PointShadowOpaque:
            return "PointShadow/Opaque";

        case PipelineID::PointShadowAlphaMasked:
            return "PointShadow/AlphaMasked";

        case PipelineID::GBufferSingleSided:
            return "GBuffer/SingleSided";

        case PipelineID::GBufferDoubleSided:
            return "GBuffer/DoubleSided";

        case PipelineID::FrustumCulling:
            return "Culling/Frustum";

        case PipelineID::SpotShadowOpaque:
            return "SpotShadow/Opaque";

        case PipelineID::SpotShadowAlphaMasked:
            return "SpotShadow/AlphaMasked";

        case PipelineID::AutoExposureHistogram:
            return "Exposure/Histogram";

        case PipelineID::AutoExposureAverage:
            return "Exposure/Average";

        case PipelineID::AutoExposureCombine:
            return "Exposure/Combine";

        case PipelineID::TAA:
            return "TAA";

        case PipelineID::VBAODepthPreFilter:
            return "VBAO/DepthPreFilter";

        case PipelineID::VBAO:
            return "VBAO/Occlusion";

        case PipelineID::VBAOSpatialDenoise:
            return "VBAO/SpatialDenoise";

        case PipelineID::TiledLightCullingBounds:
            return "TiledLightCulling/Bounds";

        case PipelineID::TiledLightCulling:
            return "TiledLightCulling/Culling";

        case PipelineID::DebugAABBGenerateDrawCalls:
            return "Debug/AABB/GenerateDrawCalls";

        case PipelineID::DebugAABB:
            return "Debug/AABB";

        case PipelineID::DebugLightSphere:
            return "Debug/Light/Sphere";

        case PipelineID::DebugFrustumCullingStatistics:
            return "Debug/Frustum/CullingStatistics";

        case PipelineID::DebugTiledLightCullingStatistics:
            return "Debug/TiledLightCulling/CullingStatistics";

        case PipelineID::ShadowRayTraced:
            return "Shadow/RayTraced";

        case PipelineID::IBLEquirectangularToCubemap:
            return "IBL/EquirectangularToCubemap";

        case PipelineID::IBLIrradiance:
            return "IBL/Irradiance";

        case PipelineID::IBLPreFilter:
            return "IBL/PreFilter";

        case PipelineID::IBLBRDF:
            return "IBL/BRDF";

        default:
            return "Null/Pipeline";
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