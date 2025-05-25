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
        Models::ModelID id = std::hash<std::string_view>()(path);

        if (!modelMap.contains(id))
        {
            modelMap.emplace(id, Model(
                allocator,
                geometryBuffer,
                textureManager,
                deletionQueue,
                path
            ));
        }

        return id;
    }

    const Model& ModelManager::GetModel(Models::ModelID id) const
    {
        const auto iter = modelMap.find(id);

        if (iter == modelMap.end())
        {
            Logger::Error("Invalid model ID! [ID={}]\n", id);
        }

        return iter->second;
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
                for (const auto& [id, model] : modelMap)
                {
                    if (ImGui::TreeNode(fmt::format("[{}]", id).c_str()))
                    {
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
                                ImGui::Text("Texture Name                  | ID");
                                ImGui::Separator();

                                ImGui::Text("Albedo                        | %llu", mesh.material.albedo);
                                ImGui::Text("Normal Map                    | %llu", mesh.material.normal);
                                ImGui::Text("AO + Roughness + Metallic Map | %llu", mesh.material.aoRghMtl);

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

                                ImGui::Separator();

                                ImGui::Text("AABB Min    | [%.3f, %.3f, %.3f]", mesh.aabb.min.x, mesh.aabb.min.y, mesh.aabb.min.z);
                                ImGui::Text("AABB Max    | [%.3f, %.3f, %.3f]", mesh.aabb.max.x, mesh.aabb.max.y, mesh.aabb.max.z);

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
