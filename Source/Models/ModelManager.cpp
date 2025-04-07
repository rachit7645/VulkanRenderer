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
    ModelManager::ModelManager(const Vk::Context& context, const Vk::FormatHelper& formatHelper)
        : geometryBuffer(context.device, context.allocator),
          textureManager(formatHelper)
    {
    }

    usize ModelManager::AddModel(const Vk::Context& context, Vk::MegaSet& megaSet, const std::string_view path)
    {
        usize pathHash = std::hash<std::string_view>()(path);

        if (!modelMap.contains(pathHash))
        {
            modelMap.emplace(pathHash, Model(context, megaSet, geometryBuffer, textureManager, path));
        }

        return pathHash;
    }

    const Model& ModelManager::GetModel(usize modelID) const
    {
        const auto iter = modelMap.find(modelID);

        if (iter == modelMap.end())
        {
            Logger::Error("Invalid model ID! [ID={}]\n", modelID);
        }

        return iter->second;
    }

    void ModelManager::Update(const Vk::Context& context, Vk::CommandBufferAllocator& cmdBufferAllocator)
    {
        const auto cmdBuffer = cmdBufferAllocator.AllocateGlobalCommandBuffer(context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        Vk::BeginLabel(context.graphicsQueue, "ModelManager::Update", {0.9607f, 0.4392f, 0.2980f, 1.0f});

        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            geometryBuffer.Update(cmdBuffer, context.device, context.allocator);
            textureManager.Update(cmdBuffer);
        cmdBuffer.EndRecording();

        VkFence transferFence = VK_NULL_HANDLE;

        // Submit
        {
            const VkFenceCreateInfo fenceCreateInfo =
            {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0
            };

            vkCreateFence(context.device, &fenceCreateInfo, nullptr, &transferFence);

            VkCommandBufferSubmitInfo cmdBufferInfo =
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = cmdBuffer.handle,
                .deviceMask    = 0
            };

            const VkSubmitInfo2 submitInfo =
            {
                .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext                    = nullptr,
                .flags                    = 0,
                .waitSemaphoreInfoCount   = 0,
                .pWaitSemaphoreInfos      = nullptr,
                .commandBufferInfoCount   = 1,
                .pCommandBufferInfos      = &cmdBufferInfo,
                .signalSemaphoreInfoCount = 0,
                .pSignalSemaphoreInfos    = nullptr
            };

            Vk::CheckResult(vkQueueSubmit2(
                context.graphicsQueue,
                1,
                &submitInfo,
                transferFence),
                "Failed to submit transfer command buffers!"
            );

            Vk::CheckResult(vkWaitForFences(
                context.device,
                1,
                &transferFence,
                VK_TRUE,
                std::numeric_limits<u64>::max()),
                "Error while waiting for transfer!"
            );
        }

        Vk::EndLabel(context.graphicsQueue);

        // Clean
        {
            geometryBuffer.ClearUploads(context.allocator);
            textureManager.ClearUploads(context.allocator);

            vkDestroyFence(context.device, transferFence, nullptr);
            cmdBufferAllocator.FreeGlobalCommandBuffer(cmdBuffer);
        }
    }

    void ModelManager::ImGuiDisplay()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Model Manager"))
            {
                for (const auto& [pathHash, model] : modelMap)
                {
                    if (ImGui::TreeNode(fmt::format("[{}]", pathHash).c_str()))
                    {
                        for (usize i = 0; i < model.meshes.size(); ++i)
                        {
                            if (ImGui::TreeNode(fmt::format("Mesh #{}", i).c_str()))
                            {
                                const auto& mesh = model.meshes[i];

                                ImGui::Separator();
                                ImGui::Text("Info Name | Offset/Count");
                                ImGui::Separator();

                                ImGui::Text("Indices   | %u/%u", mesh.indexInfo.offset,    mesh.indexInfo.count);
                                ImGui::Text("Positions | %u/%u", mesh.positionInfo.offset, mesh.positionInfo.count);
                                ImGui::Text("Vertices  | %u/%u", mesh.vertexInfo.offset,   mesh.vertexInfo.count);

                                ImGui::Separator();
                                ImGui::Text("Texture Name                  | ID");
                                ImGui::Separator();

                                ImGui::Text("Albedo                        | %u", mesh.material.albedo);
                                ImGui::Text("Normal Map                    | %u", mesh.material.normal);
                                ImGui::Text("AO + Roughness + Metallic Map | %u", mesh.material.aoRghMtl);

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
