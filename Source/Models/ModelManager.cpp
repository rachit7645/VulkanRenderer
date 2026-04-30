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
#include "Externals/ImGui.h"

namespace Models
{
    ModelManager::ModelManager(const Vk::Context& context, Vk::StagingPool& stagingPool)
        : geometryBuffer(context, stagingPool)
    {
    }

    Models::ModelID ModelManager::Load(const std::string_view path)
    {
        const Models::ModelID id = std::hash<std::string_view>()(path);

        auto iter = m_modelMap.find(id);

        if (iter != m_modelMap.end())
        {
            ++iter->second.referenceCount;
        }
        else
        {
            m_requestedModelLoads.emplace(id, path);

            m_modelMap.emplace(id, ModelInfo{
                .model          = {
                    .name     = Util::Files::GetNameWithoutExtension(path),
                    .meshes   = {},
                    .isLoaded = false
                },
                .referenceCount = 1
            });
        }

        return id;
    }

    void ModelManager::Free(Models::ModelID id)
    {
        const auto iter = m_modelMap.find(id);

        if (iter == m_modelMap.end())
        {
            Logger::Error("Invalid model ID! [ID={}]\n", id);
        }

        if (iter->second.referenceCount == 0)
        {
            Logger::Error("Model already freed! [ID={}]\n", id);
        }

        --iter->second.referenceCount;

        if (iter->second.referenceCount == 0)
        {
            m_requestedModelDeletions.emplace(id);
        }
    }

    const Model& ModelManager::GetModel(Models::ModelID id) const
    {
        const auto iter = m_modelMap.find(id);

        if (iter == m_modelMap.end())
        {
            Logger::Error("Invalid model ID! [ID={}]\n", id);
        }

        if (iter->second.referenceCount == 0)
        {
            Logger::Error("Model already freed! [ID={}]\n", id);
        }

        if (!iter->second.model.isLoaded)
        {
            Logger::Error("Model is not loaded! [ID={}]\n", id);
        }

        return iter->second.model;
    }

    void ModelManager::Update
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        Vk::MegaSet& megaSet,
        Vk::StagingPool& stagingPool,
        Engine::CacheManager& cacheManager,
        tf::Executor& executor,
        Util::DeletionQueue& deletionQueue
    )
    {
        if
        (
            !geometryBuffer.HasPendingUploads() &&
            !textureManager.HasPendingUploads() &&
            m_requestedModelLoads.empty() &&
            m_requestedModelDeletions.empty()
        )
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "ModelManager::Update", {0.9607f, 0.4392f, 0.2980f, 1.0f});

        for (const auto& id : m_requestedModelDeletions)
        {
            const auto iter = m_modelMap.find(id);

            if (iter == m_modelMap.end())
            {
                Logger::Warning("Attempted deletion of invalid model! [ID={}]\n", id);

                continue;
            }

            // Verify that new references to a model that was requested
            // to be deleted have not been made in the meantime
            if (iter->second.referenceCount != 0)
            {
                continue;
            }

            if (iter->second.model.isLoaded)
            {
                iter->second.model.Destroy
                (
                    device,
                    allocator,
                    megaSet,
                    textureManager,
                    geometryBuffer,
                    deletionQueue
                );
            }

            m_modelMap.erase(iter);
        }

        for (const auto& [id, path] : m_requestedModelLoads)
        {
            const auto iter = m_modelMap.find(id);

            if (iter == m_modelMap.end())
            {
                Logger::Warning("Attempted loading of invalid model! [ID={}]\n", id);

                continue;
            }

            if (iter->second.referenceCount == 0)
            {
                Logger::Warning("Model already freed! [ID={}]\n", id);

                continue;
            }

            if (iter->second.model.isLoaded)
            {
                Logger::Warning("Model already loaded! [ID={}]\n", id);

                continue;
            }

            iter->second.model.LoadFromFile
            (
                device,
                allocator,
                geometryBuffer,
                textureManager,
                stagingPool,
                cacheManager,
                executor,
                deletionQueue,
                path
            );
        }

        m_requestedModelDeletions.clear();
        m_requestedModelLoads.clear();

        geometryBuffer.Update
        (
            cmdBuffer,
            device,
            allocator,
            stagingPool,
            deletionQueue
        );

        textureManager.Update(cmdBuffer, device, megaSet);

        Vk::EndLabel(cmdBuffer);
    }

    void ModelManager::ImGuiDisplay()
    {
        if (ImGui::CollapsingHeader("Model Manager"))
        {
            for (const auto& [id, info] : m_modelMap)
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

                            ImGui::Text("Indices   | %u/%u", mesh.surfaceInfo.indexInfo.offset,    mesh.surfaceInfo.indexInfo.count);
                            ImGui::Text("Positions | %u/%u", mesh.surfaceInfo.positionInfo.offset, mesh.surfaceInfo.positionInfo.count);
                            ImGui::Text("Vertices  | %u/%u", mesh.surfaceInfo.vertexInfo.offset,   mesh.surfaceInfo.vertexInfo.count);

                            ImGui::Separator();
                            ImGui::Text("Texture Name              | UV Map ID | ID");
                            ImGui::Separator();

                            ImGui::Text("Albedo                    | %u         | %llu", mesh.material.albedoUVMapID,   mesh.material.albedoID);
                            ImGui::Text("Normal                    | %u         | %llu", mesh.material.normalUVMapID,   mesh.material.normalID);
                            ImGui::Text("AO + Roughness + Metallic | %u         | %llu", mesh.material.aoRghMtlUVMapID, mesh.material.aoRghMtlID);
                            ImGui::Text("Emissive                  | %u         | %llu", mesh.material.emissiveUVMapID, mesh.material.emissiveID);

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

        geometryBuffer.ImGuiDisplay();
        textureManager.ImGuiDisplay();
    }

    void ModelManager::Destroy(VkDevice device, VmaAllocator allocator, Vk::StagingPool& stagingPool)
    {
        geometryBuffer.Destroy(allocator, stagingPool);
        textureManager.Destroy(device, allocator);

        m_modelMap.clear();
        m_requestedModelDeletions.clear();
        m_requestedModelLoads.clear();
    }
}