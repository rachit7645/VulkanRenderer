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

#include "ModelManager.h"

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"
#include "Util/Files.h"
#include "Externals/ImGui.h"

namespace Models
{
    Models::ModelID ModelManager::Load(const std::string_view path)
    {
        const Models::ModelID id = {.value = std::hash<std::string_view>()(path)};

        auto loadedIter = m_loadedModels.find(id);

        if (loadedIter != m_loadedModels.end())
        {
            ++loadedIter->second.referenceCount;

            return id;
        }

        auto pendingIter = m_pendingModels.find(id);

        if (pendingIter != m_pendingModels.end())
        {
            ++pendingIter->second.referenceCount;

            return id;
        }

        m_pendingModels.emplace(id, ModelLoadInfo{
            .path           = std::string(path),
            .referenceCount = 1
        });

        return id;
    }

    void ModelManager::Free(Models::ModelID id)
    {
        const auto loadedIter = m_loadedModels.find(id);

        if (loadedIter != m_loadedModels.end())
        {
            if (loadedIter->second.referenceCount == 0)
            {
                Logger::Error("Model already freed! [ID={}]\n", id.value);
            }

            --loadedIter->second.referenceCount;

            if (loadedIter->second.referenceCount == 0)
            {
                m_requestedModelDeletions.emplace_back(id);
            }
        }

        const auto pendingIter = m_pendingModels.find(id);

        if (pendingIter != m_pendingModels.end())
        {
            if (pendingIter->second.referenceCount == 0)
            {
                Logger::Error("Model already freed! [ID={}]\n", id.value);
            }

            --pendingIter->second.referenceCount;

            if (pendingIter->second.referenceCount == 0)
            {
                m_pendingModels.erase(id);
            }
        }
    }

    bool ModelManager::IsModelLoaded(Models::ModelID id) const
    {
        const auto iter = m_loadedModels.find(id);

        if (iter == m_loadedModels.end())
        {
            return false;
        }

        if (iter->second.referenceCount == 0)
        {
            Logger::Error("Model already freed! [ID={}]\n", id.value);
        }

        return true;
    }

    const Model* ModelManager::TryGetModel(Models::ModelID id) const
    {
        const auto iter = m_loadedModels.find(id);

        if (iter == m_loadedModels.end())
        {
            return nullptr;
        }

        if (iter->second.referenceCount == 0)
        {
            Logger::Error("Model already freed! [ID={}]\n", id.value);
        }

        return &iter->second.model;
    }

    const Model& ModelManager::GetModel(Models::ModelID id) const
    {
        const auto* model = TryGetModel(id);

        if (model == nullptr)
        {
            Logger::Error("Invalid Model ID! [ID={}]", id.value);
        }

        return *model;
    }

    void ModelManager::Update
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        Vk::MegaSet& megaSet,
        Vk::StagingPool& stagingPool,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Scratch::Allocator& scratchAllocator,
        tf::Executor& executor,
        Engine::DeletionQueue& deletionQueue
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        if (m_pendingModels.empty() && m_requestedModelDeletions.empty())
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "ModelManager::Update", {0.9607f, 0.4392f, 0.2980f, 1.0f});

        auto modelLoadFutures = Scratch::CreateVector<std::future<LoadedModel>>(scratchAllocator);

        const Models::LoadFromFileInfo loadFromFileInfo =
        {
            .device         = device,
            .allocator      = allocator,
            .geometryBuffer = geometryBuffer,
            .textureManager = textureManager,
            .stagingPool    = stagingPool,
            .executor       = executor,
            .deletionQueue  = deletionQueue
        };

        for (const auto& [id, loadInfo] : m_pendingModels)
        {
            if (loadInfo.referenceCount == 0)
            {
                continue;
            }

            modelLoadFutures.emplace_back(executor.async([id, loadInfo, loadFromFileInfo] () mutable
            {
                Models::Model model = {};

                model.LoadFromFile(loadFromFileInfo, loadInfo.path);

                return ModelManager::LoadedModel
                {
                    .id        = id,
                    .modelInfo = ModelManager::ModelInfo{
                        .model          = model,
                        .referenceCount = loadInfo.referenceCount
                    }
                };
            }));
        }

        m_pendingModels.clear();

        for (auto& future : modelLoadFutures)
        {
            if (!future.valid())
            {
                Logger::Error("{}\n", "Future is not valid!");
            }

            future.wait();

            const auto loadedModel = future.get();

            m_loadedModels.emplace(loadedModel.id, loadedModel.modelInfo);
        }

        modelLoadFutures.clear();

        for (const auto& id : m_requestedModelDeletions)
        {
            const auto loadedIter = m_loadedModels.find(id);

            if (loadedIter == m_loadedModels.end())
            {
                continue;
            }

            // Verify that new references to a model that was requested
            // to be deleted have not been made in the meantime
            if (loadedIter->second.referenceCount != 0)
            {
                continue;
            }

            loadedIter->second.model.Destroy
            (
                device,
                allocator,
                megaSet,
                textureManager,
                geometryBuffer,
                deletionQueue
            );

            m_loadedModels.erase(loadedIter);
        }

        m_requestedModelDeletions.clear();

        Vk::EndLabel(cmdBuffer);
    }

    void ModelManager::ImGuiDisplay()
    {
        if (ImGui::CollapsingHeader("Models"))
        {
            for (const auto& [id, info] : m_loadedModels)
            {
                const auto& [model, refCount] = info;

                if (ImGui::TreeNode(std::bit_cast<void*>(id), "%s", model.name.c_str()))
                {
                    ImGui::Text("Reference Count | %llu", refCount);

                    for (usize i = 0; i < model.meshes.size(); ++i)
                    {
                        if (ImGui::TreeNode(fmt::format("Mesh #{}", i).c_str()))
                        {
                            const auto& mesh = model.meshes[i];

                            ImGui::Separator();
                            ImGui::Text("Info Name | Offset/Count");
                            ImGui::Separator();

                            ImGui::Text("Indices   | %u/%u", mesh.surfaceInfo.indexInfo.offset,  mesh.surfaceInfo.indexInfo.count);
                            ImGui::Text("Vertices  | %u/%u", mesh.surfaceInfo.vertexInfo.offset, mesh.surfaceInfo.vertexInfo.count);

                            ImGui::Separator();
                            ImGui::Text("Texture Name              | UV Map ID | ID");
                            ImGui::Separator();

                            ImGui::Text("Albedo                    | %u         | %llu", mesh.material.albedoUVMapID,   mesh.material.albedoID.value);
                            ImGui::Text("Normal                    | %u         | %llu", mesh.material.normalUVMapID,   mesh.material.normalID.value);
                            ImGui::Text("AO + Roughness + Metallic | %u         | %llu", mesh.material.aoRghMtlUVMapID, mesh.material.aoRghMtlID.value);
                            ImGui::Text("Emissive                  | %u         | %llu", mesh.material.emissiveUVMapID, mesh.material.emissiveID.value);

                            ImGui::Separator();
                            ImGui::Text("Factor Name | Value");
                            ImGui::Separator();

                            ImGui::Text("Albedo      | [%.3f, %.3f, %.3f, %.3f]",
                                mesh.material.albedoFactor.r,
                                mesh.material.albedoFactor.g,
                                mesh.material.albedoFactor.b,
                                mesh.material.albedoFactor.a
                            );

                            ImGui::Text("Roughness   | %.3f", mesh.material.roughnessFactor);
                            ImGui::Text("Metallic    | %.3f", mesh.material.metallicFactor);

                            ImGui::Text("Emissive    | [%.3f, %.3f, %.3f]",
                                mesh.material.emissiveFactor.r,
                                mesh.material.emissiveFactor.g,
                                mesh.material.emissiveFactor.b
                            );

                            ImGui::Separator();
                            ImGui::Text("Misc              | Value");
                            ImGui::Separator();

                            ImGui::Text("Emissive Strength | %.3f", mesh.material.emissiveStrength);
                            ImGui::Text("Alpha Cutoff      | %.3f", mesh.material.alphaCutOff);
                            ImGui::Text("IoR               | %.3f", mesh.material.ior);

                            ImGui::Separator();
                            ImGui::Text("Bounds   | Value");
                            ImGui::Separator();

                            ImGui::Text("AABB Min | [%.3f, %.3f, %.3f]", mesh.aabb.min.x, mesh.aabb.min.y, mesh.aabb.min.z);
                            ImGui::Text("AABB Max | [%.3f, %.3f, %.3f]", mesh.aabb.max.x, mesh.aabb.max.y, mesh.aabb.max.z);

                            ImGui::TreePop();
                        }

                        ImGui::Separator();
                    }

                    ImGui::TreePop();
                }

                ImGui::Separator();
            }
        }
    }
}
