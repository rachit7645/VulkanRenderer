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
    ModelManager::ModelManager(const Vk::Context& context)
        : geometryBuffer(context.device, context.allocator),
          textureManager(context.physicalDevice)
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

    void ModelManager::Update(const Vk::Context& context)
    {
        auto cmdBuffer = Vk::CommandBuffer
        (
            context.device,
            context.commandPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY
        );

        Vk::BeginLabel(context.graphicsQueue, "ModelManager::Update", {0.9607f, 0.4392f, 0.2980f, 1.0f});

        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            geometryBuffer.Update(cmdBuffer);
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
                "Failed to submit tranfer command buffers!"
            );

            Vk::CheckResult(vkWaitForFences(
                context.device,
                1,
                &transferFence,
                VK_TRUE,
                std::numeric_limits<u64>::max()),
                "Error while waiting for tranfers!"
            );
        }

        Vk::EndLabel(context.graphicsQueue);

        // Clean
        {
            geometryBuffer.Clear(context.allocator);
            textureManager.Clear(context.allocator);

            vkDestroyFence(context.device, transferFence, nullptr);
            cmdBuffer.Free(context.device, context.commandPool);
        }
    }

    void ModelManager::Destroy(VkDevice device, VmaAllocator allocator)
    {
        geometryBuffer.Destroy(allocator);
        textureManager.Destroy(device, allocator);
    }
}
