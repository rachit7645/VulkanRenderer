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

#include "ModelManager.h"

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"

namespace Models
{
    ModelManager::ModelManager(VkDevice device, VmaAllocator allocator)
        : geometryBuffer(device, allocator)
    {
    }

    Models::ModelID ModelManager::AddModel
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const std::string_view path
    )
    {
        const Models::ModelID id = std::hash<std::string_view>()(path);

        auto iter = m_modelMap.find(id);

        if (iter != m_modelMap.end())
        {
            ++iter->second.referenceCount;
        }
        else
        {
            m_modelMap.emplace(id, ModelInfo{
                .model = Model(
                    allocator,
                    geometryBuffer,
                    textureManager,
                    deletionQueue,
                    path
                ),
                .referenceCount = 1
            });
        }

        return id;
    }

    void ModelManager::DestroyModel
    (
        ModelID id,
        VkDevice device,
        VmaAllocator allocator,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
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
            iter->second.model.Destroy
            (
                device,
                allocator,
                megaSet,
                textureManager,
                geometryBuffer,
                deletionQueue
            );

            m_modelMap.erase(iter);
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

        return iter->second.model;
    }

    void ModelManager::Update
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (!geometryBuffer.HasPendingUploads() && !textureManager.HasPendingUploads())
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "ModelManager::Update", {0.9607f, 0.4392f, 0.2980f, 1.0f});

        geometryBuffer.Update(cmdBuffer, device, allocator, deletionQueue);
        textureManager.Update(cmdBuffer, device, megaSet);

        Vk::EndLabel(cmdBuffer);
    }

    void ModelManager::ImGuiDisplay()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Model Manager"))
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
                                ImGui::Text("Emmisive                  | %u         | %llu", mesh.material.emmisiveUVMapID, mesh.material.emmisiveID);

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

                                ImGui::Text("Emmisive    | [%.3f, %.3f, %.3f]",
                                    mesh.material.emmisiveFactor.r,
                                    mesh.material.emmisiveFactor.g,
                                    mesh.material.emmisiveFactor.b
                                );

                                ImGui::Separator();
                                ImGui::Text("Misc              | Value");
                                ImGui::Separator();

                                ImGui::Text("Emmisive Strength | %.3f", mesh.material.emmisiveStrength);
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

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        geometryBuffer.ImGuiDisplay();
        textureManager.ImGuiDisplay();
    }

    void ModelManager::Destroy(VkDevice device, VmaAllocator allocator)
    {
        geometryBuffer.Destroy(allocator);
        textureManager.Destroy(device, allocator);
    }
}