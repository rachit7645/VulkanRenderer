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

#include "RenderManager.h"

#include "AO/VBGTAO/VBGTAO.h"
#include "Util/Log.h"
#include "Vulkan/Util.h"
#include "Vulkan/DebugUtils.h"
#include "Vulkan/ImmediateSubmit.h"
#include "Engine/Inputs.h"
#include "Externals/ImGui.h"
#include "Externals/Tracy.h"

namespace Renderer
{
    RenderManager::RenderManager()
        : m_context(m_window.handle),
          m_graphicsCmdBufferAllocator(m_context.device, *m_context.queueFamilies.graphicsFamily),
          m_swapchain(m_window.size, m_context, m_graphicsCmdBufferAllocator),
          m_graphicsTimeline(m_context.device),
          m_formatHelper(m_context.physicalDevice),
          m_megaSet(m_context),
          m_modelManager(m_context.device, m_context.allocator),
          m_samplers(m_context, m_megaSet, m_modelManager.textureManager),
          m_postProcess(m_context, m_formatHelper, m_megaSet, m_framebufferManager),
          m_depth(m_context, m_formatHelper, m_megaSet, m_framebufferManager),
          m_imGui(m_context, m_swapchain, m_megaSet),
          m_skybox(m_context, m_formatHelper, m_megaSet),
          m_bloom(m_context, m_formatHelper, m_megaSet, m_framebufferManager),
          m_pointShadow(m_context, m_formatHelper, m_megaSet, m_framebufferManager),
          m_gBuffer(m_context, m_formatHelper, m_megaSet, m_framebufferManager),
          m_lighting(m_context, m_formatHelper, m_megaSet, m_framebufferManager),
          m_shadowRT(m_context, m_megaSet, m_graphicsCmdBufferAllocator, m_framebufferManager),
          m_taa(m_context, m_formatHelper, m_megaSet, m_framebufferManager),
          m_culling(m_context),
          m_vbgtao(m_context, m_megaSet, m_framebufferManager),
          m_iblGenerator(m_context, m_formatHelper, m_megaSet),
          m_meshBuffer(m_context.device, m_context.allocator),
          m_indirectBuffer(m_context.device, m_context.allocator),
          m_sceneBuffer(m_context.device, m_context.allocator)
    {
        if (m_context.queueFamilies.computeFamily.has_value())
        {
            m_computeCmdBufferAllocator = Vk::CommandBufferAllocator(m_context.device, *m_context.queueFamilies.computeFamily);
            m_computeTimeline           = Vk::ComputeTimeline(m_context.device);
            m_sceneBufferCompute        = Buffers::SceneBuffer(m_context.device, m_context.allocator);
        }

        // ImGui Yoy
        Init();

        m_frameCounter.Reset();

        m_globalDeletionQueue.PushDeletor([&] ()
        {
            m_sceneBuffer.Destroy(m_context.allocator);
            m_indirectBuffer.Destroy(m_context.allocator);
            m_meshBuffer.Destroy(m_context.allocator);

            m_iblGenerator.Destroy(m_context.device, m_context.allocator);
            m_vbgtao.Destroy(m_context.device);
            m_culling.Destroy(m_context.device, m_context.allocator);
            m_taa.Destroy(m_context.device);
            m_shadowRT.Destroy(m_context.device, m_context.allocator);
            m_lighting.Destroy(m_context.device);
            m_gBuffer.Destroy(m_context.device);
            m_pointShadow.Destroy(m_context.device);
            m_bloom.Destroy(m_context.device);
            m_skybox.Destroy(m_context.device);
            m_imGui.Destroy(m_context.device, m_context.allocator);
            m_depth.Destroy(m_context.device);
            m_postProcess.Destroy(m_context.device);

            m_megaSet.Destroy(m_context.device);
            m_accelerationStructure.Destroy(m_context.device, m_context.allocator);
            m_framebufferManager.Destroy(m_context.device, m_context.allocator);
            m_modelManager.Destroy(m_context.device, m_context.allocator);

            m_graphicsTimeline.Destroy(m_context.device);
            m_swapchain.Destroy(m_context.device);
            m_graphicsCmdBufferAllocator.Destroy(m_context.device);

            if (m_computeCmdBufferAllocator.has_value())
            {
                m_computeCmdBufferAllocator->Destroy(m_context.device);
            }

            if (m_sceneBufferCompute.has_value())
            {
                m_sceneBufferCompute->Destroy(m_context.allocator);
            }

            if (m_computeTimeline.has_value())
            {
                m_computeTimeline->Destroy(m_context.device);
            }

            m_context.Destroy();
        });
    }

    void RenderManager::Render()
    {
        WaitForTimeline();

        // Swapchain is not ok, wait for resize event
        if (!m_isSwapchainOk)
        {
            return;
        }

        AcquireSwapchainImage();
        BeginFrame();

        if (m_context.queueFamilies.HasAllFamilies())
        {
            RenderMultiQueue();
        }
        else if (m_context.queueFamilies.HasRequiredFamilies())
        {
            RenderGraphicsQueueOnly();
        }

        EndFrame();
    }

    void RenderManager::WaitForTimeline()
    {
        // Frame indices 0 to Vk::FRAMES_IN_FLIGHT - 1 do not need to wait for anything
        if (m_frameIndex >= Vk::FRAMES_IN_FLIGHT)
        {
            m_graphicsTimeline.WaitForStage
            (
                m_frameIndex - Vk::FRAMES_IN_FLIGHT,
                Vk::GraphicsTimeline::GRAPHICS_TIMELINE_STAGE_RENDER_FINISHED,
                m_context.device
            );
        }
    }

    void RenderManager::AcquireSwapchainImage()
    {
        const auto result = m_swapchain.AcquireSwapChainImage(m_context.device, m_FIF);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_isSwapchainOk = false;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            Vk::CheckResult(result, "Failed to acquire swapchain image!");
        }

        m_graphicsTimeline.AcquireImageToTimeline
        (
            m_frameIndex,
            m_context.graphicsQueue,
            m_swapchain.imageAvailableSemaphores[m_FIF]
        );
    }

    void RenderManager::BeginFrame()
    {
        m_deletionQueues[m_FIF].FlushQueue();

        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        m_graphicsCmdBufferAllocator.ResetPool(m_FIF, m_context.device);

        if (m_computeCmdBufferAllocator.has_value())
        {
            m_computeCmdBufferAllocator->ResetPool(m_FIF, m_context.device);
        }
    }

    void RenderManager::RenderGraphicsQueueOnly()
    {
        const auto cmdBuffer = m_graphicsCmdBufferAllocator.AllocateCommandBuffer(m_FIF, m_context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            GBufferGeneration(cmdBuffer);
            Occlusion(cmdBuffer, m_sceneBuffer, "SceneDepthView", "GNormalView");
            TraceRays(cmdBuffer);
            Lighting(cmdBuffer);
        cmdBuffer.EndRecording();

        const VkSemaphoreSubmitInfo waitSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = m_graphicsTimeline.semaphore,
            .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::GRAPHICS_TIMELINE_STAGE_SWAPCHAIN_IMAGE_ACQUIRED),
            .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        };

        const VkSemaphoreSubmitInfo signalSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = m_graphicsTimeline.semaphore,
            .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::GRAPHICS_TIMELINE_STAGE_RENDER_FINISHED),
            .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        };

        const VkCommandBufferSubmitInfo cmdBufferInfo =
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
            .waitSemaphoreInfoCount   = 1,
            .pWaitSemaphoreInfos      = &waitSemaphoreInfo,
            .commandBufferInfoCount   = 1,
            .pCommandBufferInfos      = &cmdBufferInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos    = &signalSemaphoreInfo
        };

        Vk::CheckResult(vkQueueSubmit2(
            m_context.graphicsQueue,
            1,
            &submitInfo,
            VK_NULL_HANDLE),
            "Failed to submit queue!"
        );
    }

    void RenderManager::RenderMultiQueue()
    {
        // GBuffer Generation Submit
        {
            const auto gBufferGenerationCmdBuffer = m_graphicsCmdBufferAllocator.AllocateCommandBuffer(m_FIF, m_context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            gBufferGenerationCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
                GBufferGeneration(gBufferGenerationCmdBuffer);
                GraphicsToAsyncComputeRelease(gBufferGenerationCmdBuffer);
            gBufferGenerationCmdBuffer.EndRecording();

            const VkSemaphoreSubmitInfo swapchainImageAcquireWaitSemaphoreInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext       = nullptr,
                .semaphore   = m_graphicsTimeline.semaphore,
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::GRAPHICS_TIMELINE_STAGE_SWAPCHAIN_IMAGE_ACQUIRED),
                .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .deviceIndex = 0
            };

            const VkCommandBufferSubmitInfo gBufferGenerationCmdBufferInfo =
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = gBufferGenerationCmdBuffer.handle,
                .deviceMask    = 0
            };

            const VkSemaphoreSubmitInfo gBufferGenerationSignalSemaphoreInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext       = nullptr,
                .semaphore   = m_graphicsTimeline.semaphore,
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::GRAPHICS_TIMELINE_STAGE_GBUFFER_GENERATION_COMPLETE),
                .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .deviceIndex = 0
            };

            const VkSubmitInfo2 gBufferGenerationSubmitInfo =
            {
                .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext                    = nullptr,
                .flags                    = 0,
                .waitSemaphoreInfoCount   = 1,
                .pWaitSemaphoreInfos      = &swapchainImageAcquireWaitSemaphoreInfo,
                .commandBufferInfoCount   = 1,
                .pCommandBufferInfos      = &gBufferGenerationCmdBufferInfo,
                .signalSemaphoreInfoCount = 1,
                .pSignalSemaphoreInfos    = &gBufferGenerationSignalSemaphoreInfo
            };

            Vk::CheckResult(vkQueueSubmit2(
                m_context.graphicsQueue,
                1,
                &gBufferGenerationSubmitInfo,
                VK_NULL_HANDLE),
                "Failed to submit to graphics queue!"
            );
        }

        // Async Compute Submit
        {
            const auto asyncComputeCmdBuffer = m_computeCmdBufferAllocator->AllocateCommandBuffer(m_FIF, m_context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            asyncComputeCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
                GraphicsToAsyncComputeAcquire(asyncComputeCmdBuffer);
                Occlusion(asyncComputeCmdBuffer, *m_sceneBufferCompute, "SceneDepthAsyncComputeView", "GNormalAsyncComputeView");
                AsyncComputeToGraphicsRelease(asyncComputeCmdBuffer);
            asyncComputeCmdBuffer.EndRecording();

            const VkSemaphoreSubmitInfo gBufferGenerationAsyncComputeWaitSemaphoreInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext       = nullptr,
                .semaphore   = m_graphicsTimeline.semaphore,
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::GRAPHICS_TIMELINE_STAGE_GBUFFER_GENERATION_COMPLETE),
                .stageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .deviceIndex = 0
            };

            const VkCommandBufferSubmitInfo asyncComputeCmdBufferInfo =
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = asyncComputeCmdBuffer.handle,
                .deviceMask    = 0
            };

            const VkSemaphoreSubmitInfo asyncComputeSignalSemaphoreInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext       = nullptr,
                .semaphore   = m_computeTimeline->semaphore,
                .value       = m_computeTimeline->GetTimelineValue(m_frameIndex, Vk::ComputeTimeline::COMPUTE_TIMELINE_STAGE_ASYNC_COMPUTE_FINISHED),
                .stageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .deviceIndex = 0
            };

            const VkSubmitInfo2 asyncComputeSubmitInfo =
            {
                .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext                    = nullptr,
                .flags                    = 0,
                .waitSemaphoreInfoCount   = 1,
                .pWaitSemaphoreInfos      = &gBufferGenerationAsyncComputeWaitSemaphoreInfo,
                .commandBufferInfoCount   = 1,
                .pCommandBufferInfos      = &asyncComputeCmdBufferInfo,
                .signalSemaphoreInfoCount = 1,
                .pSignalSemaphoreInfos    = &asyncComputeSignalSemaphoreInfo
            };

            Vk::CheckResult(vkQueueSubmit2(
                m_context.computeQueue,
                1,
                &asyncComputeSubmitInfo,
                VK_NULL_HANDLE),
                "Failed to submit to compute queue!"
            );
        }

        // Ray Dispatch Submit
        {
            const auto rayDispatchCmdBuffer = m_graphicsCmdBufferAllocator.AllocateCommandBuffer(m_FIF, m_context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            rayDispatchCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
                TraceRays(rayDispatchCmdBuffer);
            rayDispatchCmdBuffer.EndRecording();

            const VkSemaphoreSubmitInfo gBufferGenerationWaitSemaphoreInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext       = nullptr,
                .semaphore   = m_graphicsTimeline.semaphore,
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::GRAPHICS_TIMELINE_STAGE_GBUFFER_GENERATION_COMPLETE),
                .stageMask   = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                .deviceIndex = 0
            };

            const VkCommandBufferSubmitInfo rayDispatchCmdBufferInfo =
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = rayDispatchCmdBuffer.handle,
                .deviceMask    = 0
            };

            const VkSemaphoreSubmitInfo rayDispatchSignalSemaphoreInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext       = nullptr,
                .semaphore   = m_graphicsTimeline.semaphore,
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::GRAPHICS_TIMELINE_STAGE_RAY_DISPATCH),
                .stageMask   = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                .deviceIndex = 0
            };

            const VkSubmitInfo2 rayDispatchSubmitInfo =
            {
                .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext                    = nullptr,
                .flags                    = 0,
                .waitSemaphoreInfoCount   = 1,
                .pWaitSemaphoreInfos      = &gBufferGenerationWaitSemaphoreInfo,
                .commandBufferInfoCount   = 1,
                .pCommandBufferInfos      = &rayDispatchCmdBufferInfo,
                .signalSemaphoreInfoCount = 1,
                .pSignalSemaphoreInfos    = &rayDispatchSignalSemaphoreInfo
            };

            Vk::CheckResult(vkQueueSubmit2(
                m_context.graphicsQueue,
                1,
                &rayDispatchSubmitInfo,
                VK_NULL_HANDLE),
                "Failed to submit to graphics queue!"
            );
        }

        // Lighting Submit
        {
            const auto lightingCmdBuffer = m_graphicsCmdBufferAllocator.AllocateCommandBuffer(m_FIF, m_context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            lightingCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
                AsyncComputeToGraphicsAcquire(lightingCmdBuffer);
                Lighting(lightingCmdBuffer);
            lightingCmdBuffer.EndRecording();

            const VkSemaphoreSubmitInfo asyncComputeWaitSemaphoreInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext       = nullptr,
                .semaphore   = m_computeTimeline->semaphore,
                .value       = m_computeTimeline->GetTimelineValue(m_frameIndex, Vk::ComputeTimeline::COMPUTE_TIMELINE_STAGE_ASYNC_COMPUTE_FINISHED),
                .stageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .deviceIndex = 0
            };

            const VkSemaphoreSubmitInfo rayDispatchWaitSemaphoreInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext       = nullptr,
                .semaphore   = m_graphicsTimeline.semaphore,
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::GRAPHICS_TIMELINE_STAGE_RAY_DISPATCH),
                .stageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .deviceIndex = 0
            };

            const std::array lightingWaitSemaphoreInfos = {asyncComputeWaitSemaphoreInfo, rayDispatchWaitSemaphoreInfo};

            const VkCommandBufferSubmitInfo lightingCmdBufferInfo =
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = lightingCmdBuffer.handle,
                .deviceMask    = 0
            };

            const VkSemaphoreSubmitInfo renderFinishedSignalSemaphoreInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext       = nullptr,
                .semaphore   = m_graphicsTimeline.semaphore,
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::GRAPHICS_TIMELINE_STAGE_RENDER_FINISHED),
                .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .deviceIndex = 0
            };

            const VkSubmitInfo2 lightingSubmitInfo =
            {
                .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext                    = nullptr,
                .flags                    = 0,
                .waitSemaphoreInfoCount   = static_cast<u32>(lightingWaitSemaphoreInfos.size()),
                .pWaitSemaphoreInfos      = lightingWaitSemaphoreInfos.data(),
                .commandBufferInfoCount   = 1,
                .pCommandBufferInfos      = &lightingCmdBufferInfo,
                .signalSemaphoreInfoCount = 1,
                .pSignalSemaphoreInfos    = &renderFinishedSignalSemaphoreInfo
            };

            Vk::CheckResult(vkQueueSubmit2(
                m_context.graphicsQueue,
                1,
                &lightingSubmitInfo,
                VK_NULL_HANDLE),
                "Failed to submit to graphics queue!"
            );
        }
    }

    void RenderManager::GBufferGeneration(const Vk::CommandBuffer& cmdBuffer)
    {
        Update(cmdBuffer);

        if (m_scene->haveRenderObjectsChanged)
        {
            m_deletionQueues[m_FIF].PushDeletor([device = m_context.device, allocator = m_context.allocator, as = m_accelerationStructure] () mutable
            {
                as.Destroy(device, allocator);
            });

            m_accelerationStructure = {};

            m_accelerationStructure.BuildBottomLevelAS
            (
                m_frameIndex,
                cmdBuffer,
                m_context.device,
                m_context.allocator,
                m_modelManager,
                m_scene->renderObjects,
                m_deletionQueues[m_FIF]
            );

            m_scene->haveRenderObjectsChanged = false;
        }

        m_accelerationStructure.TryCompactBottomLevelAS
        (
            cmdBuffer,
            m_context.device,
            m_context.allocator,
            m_graphicsTimeline,
            m_deletionQueues[m_FIF]
        );

        m_accelerationStructure.BuildTopLevelAS
        (
            m_FIF,
            cmdBuffer,
            m_context.device,
            m_context.allocator,
            m_modelManager,
            m_scene->renderObjects,
            m_deletionQueues[m_FIF]
        );

        m_pointShadow.Render
        (
            m_FIF,
            m_frameIndex,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_samplers,
            m_culling
        );

        m_depth.Render
        (
            m_FIF,
            m_frameIndex,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_samplers,
            m_culling
        );

        m_gBuffer.Render
        (
            m_FIF,
            m_frameIndex,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_samplers
        );
    }

    void RenderManager::Occlusion
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Buffers::SceneBuffer& sceneBuffer,
        const std::string_view sceneDepthID,
        const std::string_view gNormalID
    )
    {
        m_vbgtao.Execute
        (
            m_FIF,
            m_frameIndex,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager.textureManager,
            sceneBuffer,
            m_samplers,
            sceneDepthID,
            gNormalID
        );
    }

    void RenderManager::TraceRays(const Vk::CommandBuffer& cmdBuffer)
    {
        m_shadowRT.TraceRays
        (
            m_FIF,
            m_frameIndex,
            cmdBuffer,
            m_megaSet,
            m_modelManager,
            m_framebufferManager,
            m_sceneBuffer,
            m_meshBuffer,
            m_samplers,
            m_accelerationStructure
        );
    }

    void RenderManager::Lighting(const Vk::CommandBuffer& cmdBuffer)
    {
        m_lighting.Render
        (
            m_FIF,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager.textureManager,
            m_sceneBuffer,
            m_samplers,
            m_scene->iblMaps
        );

        m_skybox.Render
        (
            m_FIF,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager,
            m_sceneBuffer,
            m_samplers,
            m_scene->iblMaps
        );

        m_taa.Render
        (
            m_frameIndex,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager.textureManager,
            m_samplers
        );

        m_bloom.Render
        (
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager.textureManager,
            m_samplers
        );

        m_postProcess.Render
        (
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager.textureManager,
            m_scene->camera,
            m_samplers
        );

        BlitToSwapchain(cmdBuffer);

        m_imGui.Render
        (
            m_FIF,
            m_context.device,
            m_context.allocator,
            cmdBuffer,
            m_megaSet,
            m_modelManager.textureManager,
            m_swapchain,
            m_samplers,
            m_deletionQueues[m_FIF]
        );
    }

    void RenderManager::BlitToSwapchain(const Vk::CommandBuffer& cmdBuffer)
    {
        const auto& finalColor     = m_framebufferManager.GetFramebuffer("FinalColor");
        const auto& swapchainImage = m_swapchain.images[m_swapchain.imageIndex];

        Vk::BeginLabel(cmdBuffer, "Blit To Swapchain", glm::vec4(0.4098f, 0.2843f, 0.7529f, 1.0f));

        Vk::BarrierWriter barrierWriter = {};

        barrierWriter
        .WriteImageBarrier(
            finalColor.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = finalColor.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = finalColor.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            swapchainImage,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = swapchainImage.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = swapchainImage.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        const VkImageBlit2 blitRegion =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .pNext = nullptr,
            .srcSubresource = {
                .aspectMask     = finalColor.image.aspect,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = finalColor.image.arrayLayers
            },
            .srcOffsets = {
                {0, 0, 0},
                {static_cast<s32>(finalColor.image.width), static_cast<s32>(finalColor.image.height), 1}
            },
            .dstSubresource = {
                .aspectMask     = swapchainImage.aspect,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = swapchainImage.arrayLayers
            },
            .dstOffsets = {
                {0, 0, 0},
                {static_cast<s32>(swapchainImage.width), static_cast<s32>(swapchainImage.height), 1}
            }
        };

        const VkBlitImageInfo2 blitImageInfo =
        {
            .sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .pNext          = nullptr,
            .srcImage       = finalColor.image.handle,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage       = swapchainImage.handle,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount    = 1,
            .pRegions       = &blitRegion,
            .filter         = VK_FILTER_LINEAR
        };

        vkCmdBlitImage2(cmdBuffer.handle, &blitImageInfo);

        barrierWriter
        .WriteImageBarrier(
            finalColor.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = finalColor.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = finalColor.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            swapchainImage,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = swapchainImage.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = swapchainImage.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderManager::GraphicsToAsyncComputeRelease(const Vk::CommandBuffer& cmdBuffer)
    {
        Vk::BeginLabel(cmdBuffer, "Graphics -> Async Compute | Release", {0.6726f, 0.6538f, 0.4518f, 1.0f});

        const auto& sceneDepth             = m_framebufferManager.GetFramebuffer("SceneDepth");
        const auto& sceneDepthAsyncCompute = m_framebufferManager.GetFramebuffer("SceneDepthAsyncCompute");
        const auto& gNormal                = m_framebufferManager.GetFramebuffer("GNormal");
        const auto& gNormalAsyncCompute    = m_framebufferManager.GetFramebuffer("GNormalAsyncCompute");
        const auto& depthMipChain          = m_framebufferManager.GetFramebuffer("VBGTAO/DepthMipChain");
        const auto& depthDifferences       = m_framebufferManager.GetFramebuffer("VBGTAO/DepthDifferences");
        const auto& noisyAO                = m_framebufferManager.GetFramebuffer("VBGTAO/NoisyAO");
        const auto& occlusion              = m_framebufferManager.GetFramebuffer("VBGTAO/Occlusion");
        const auto& hilbertLUT             = m_modelManager.textureManager.GetTexture(m_vbgtao.hilbertLUT);

        Vk::BarrierWriter barrierWriter = {};

        barrierWriter
        .WriteImageBarrier(
            sceneDepth.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = sceneDepth.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = sceneDepth.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            sceneDepthAsyncCompute.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = sceneDepthAsyncCompute.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = sceneDepthAsyncCompute.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gNormal.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = gNormal.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gNormal.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gNormalAsyncCompute.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = gNormalAsyncCompute.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gNormalAsyncCompute.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        const VkImageCopy2 depthCopyRegion =
        {
            .sType          = VK_STRUCTURE_TYPE_IMAGE_COPY_2,
            .pNext          = nullptr,
            .srcSubresource = {
                .aspectMask     = sceneDepth.image.aspect,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1
            },
            .srcOffset      = {0, 0, 0},
            .dstSubresource = {
                .aspectMask     = sceneDepthAsyncCompute.image.aspect,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1
            },
            .dstOffset      = {0, 0, 0},
            .extent         = {sceneDepth.image.width, sceneDepth.image.height, 1}
        };

        const VkCopyImageInfo2 depthCopyInfo =
        {
            .sType          = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
            .pNext          = nullptr,
            .srcImage       = sceneDepth.image.handle,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage       = sceneDepthAsyncCompute.image.handle,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount    = 1,
            .pRegions       = &depthCopyRegion
        };

        vkCmdCopyImage2(cmdBuffer.handle, &depthCopyInfo);

        const VkImageCopy2 normalCopyRegion =
        {
            .sType          = VK_STRUCTURE_TYPE_IMAGE_COPY_2,
            .pNext          = nullptr,
            .srcSubresource = {
                .aspectMask     = gNormal.image.aspect,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1
            },
            .srcOffset      = {0, 0, 0},
            .dstSubresource = {
                .aspectMask     = gNormalAsyncCompute.image.aspect,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1
            },
            .dstOffset      = {0, 0, 0},
            .extent         = {gNormal.image.width, gNormal.image.height, 1}
        };

        const VkCopyImageInfo2 normalCopyInfo =
        {
            .sType          = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
            .pNext          = nullptr,
            .srcImage       = gNormal.image.handle,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage       = gNormalAsyncCompute.image.handle,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount    = 1,
            .pRegions       = &normalCopyRegion
        };

        vkCmdCopyImage2(cmdBuffer.handle, &normalCopyInfo);

        barrierWriter
        .WriteImageBarrier(
            sceneDepth.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = sceneDepth.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = sceneDepth.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            sceneDepthAsyncCompute.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = sceneDepthAsyncCompute.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = sceneDepthAsyncCompute.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            sceneDepthAsyncCompute.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = sceneDepthAsyncCompute.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = sceneDepthAsyncCompute.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gNormal.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = gNormal.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gNormal.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gNormalAsyncCompute.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = gNormalAsyncCompute.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gNormalAsyncCompute.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gNormalAsyncCompute.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = gNormalAsyncCompute.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gNormalAsyncCompute.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            depthMipChain.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = depthMipChain.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthMipChain.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            depthDifferences.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = depthDifferences.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthDifferences.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            noisyAO.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = noisyAO.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = noisyAO.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            occlusion.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = occlusion.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = occlusion.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            hilbertLUT.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = hilbertLUT.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = hilbertLUT.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderManager::GraphicsToAsyncComputeAcquire(const Vk::CommandBuffer& cmdBuffer)
    {
        Vk::BeginLabel(cmdBuffer, "Graphics -> Async Compute | Acquire", {0.6726f, 0.6538f, 0.4518f, 1.0f});

        const auto& sceneDepthAsyncCompute = m_framebufferManager.GetFramebuffer("SceneDepthAsyncCompute");
        const auto& gNormalAsyncCompute    = m_framebufferManager.GetFramebuffer("GNormalAsyncCompute");
        const auto& depthMipChain          = m_framebufferManager.GetFramebuffer("VBGTAO/DepthMipChain");
        const auto& depthDifferences       = m_framebufferManager.GetFramebuffer("VBGTAO/DepthDifferences");
        const auto& noisyAO                = m_framebufferManager.GetFramebuffer("VBGTAO/NoisyAO");
        const auto& occlusion              = m_framebufferManager.GetFramebuffer("VBGTAO/Occlusion");
        const auto& hilbertLUT             = m_modelManager.textureManager.GetTexture(m_vbgtao.hilbertLUT);

        Vk::BarrierWriter{}
        .WriteImageBarrier(
            sceneDepthAsyncCompute.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = sceneDepthAsyncCompute.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = sceneDepthAsyncCompute.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gNormalAsyncCompute.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = gNormalAsyncCompute.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gNormalAsyncCompute.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            depthMipChain.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = depthMipChain.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthMipChain.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            depthDifferences.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = depthDifferences.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthDifferences.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            noisyAO.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = noisyAO.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = noisyAO.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            occlusion.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = occlusion.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = occlusion.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            hilbertLUT.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .dstQueueFamily = *m_context.queueFamilies.computeFamily,
                .baseMipLevel   = 0,
                .levelCount     = hilbertLUT.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = hilbertLUT.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderManager::AsyncComputeToGraphicsRelease(const Vk::CommandBuffer& cmdBuffer)
    {
        Vk::BeginLabel(cmdBuffer, "Async Compute -> Graphics | Release", {0.6726f, 0.6538f, 0.4518f, 1.0f});

        const auto& sceneDepthAsyncCompute = m_framebufferManager.GetFramebuffer("SceneDepthAsyncCompute");
        const auto& gNormalAsyncCompute    = m_framebufferManager.GetFramebuffer("GNormalAsyncCompute");
        const auto& depthMipChain          = m_framebufferManager.GetFramebuffer("VBGTAO/DepthMipChain");
        const auto& depthDifferences       = m_framebufferManager.GetFramebuffer("VBGTAO/DepthDifferences");
        const auto& noisyAO                = m_framebufferManager.GetFramebuffer("VBGTAO/NoisyAO");
        const auto& occlusion              = m_framebufferManager.GetFramebuffer("VBGTAO/Occlusion");
        const auto& hilbertLUT             = m_modelManager.textureManager.GetTexture(m_vbgtao.hilbertLUT);

        Vk::BarrierWriter{}
        .WriteImageBarrier(
            sceneDepthAsyncCompute.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = sceneDepthAsyncCompute.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = sceneDepthAsyncCompute.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gNormalAsyncCompute.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = gNormalAsyncCompute.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gNormalAsyncCompute.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            depthMipChain.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = depthMipChain.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthMipChain.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            depthDifferences.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = depthDifferences.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthDifferences.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            noisyAO.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = noisyAO.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = noisyAO.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            occlusion.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = occlusion.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = occlusion.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            hilbertLUT.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = hilbertLUT.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = hilbertLUT.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderManager::AsyncComputeToGraphicsAcquire(const Vk::CommandBuffer& cmdBuffer)
    {
        Vk::BeginLabel(cmdBuffer, "Async Compute -> Graphics | Acquire", {0.6726f, 0.6538f, 0.4518f, 1.0f});

        const auto& sceneDepthAsyncCompute = m_framebufferManager.GetFramebuffer("SceneDepthAsyncCompute");
        const auto& gNormalAsyncCompute    = m_framebufferManager.GetFramebuffer("GNormalAsyncCompute");
        const auto& depthMipChain          = m_framebufferManager.GetFramebuffer("VBGTAO/DepthMipChain");
        const auto& depthDifferences       = m_framebufferManager.GetFramebuffer("VBGTAO/DepthDifferences");
        const auto& noisyAO                = m_framebufferManager.GetFramebuffer("VBGTAO/NoisyAO");
        const auto& occlusion              = m_framebufferManager.GetFramebuffer("VBGTAO/Occlusion");
        const auto& hilbertLUT             = m_modelManager.textureManager.GetTexture(m_vbgtao.hilbertLUT);

        Vk::BarrierWriter{}
        .WriteImageBarrier(
            sceneDepthAsyncCompute.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = sceneDepthAsyncCompute.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = sceneDepthAsyncCompute.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gNormalAsyncCompute.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = gNormalAsyncCompute.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gNormalAsyncCompute.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            depthMipChain.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = depthMipChain.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthMipChain.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            depthDifferences.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = depthDifferences.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthDifferences.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            noisyAO.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = noisyAO.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = noisyAO.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            occlusion.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = occlusion.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = occlusion.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            hilbertLUT.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = *m_context.queueFamilies.computeFamily,
                .dstQueueFamily = *m_context.queueFamilies.graphicsFamily,
                .baseMipLevel   = 0,
                .levelCount     = occlusion.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = occlusion.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderManager::Update(const Vk::CommandBuffer& cmdBuffer)
    {
        m_frameCounter.Update();

        if (!m_scene.has_value())
        {
            m_scene = Engine::Scene
            (
                m_config,
                cmdBuffer,
                m_context,
                m_formatHelper,
                m_samplers,
                m_modelManager,
                m_megaSet,
                m_iblGenerator,
                m_deletionQueues[m_FIF]
            );

            m_modelManager.Update
            (
                cmdBuffer,
                m_context.device,
                m_context.allocator,
                m_megaSet,
                m_deletionQueues[m_FIF]
            );

            m_megaSet.Update(m_context.device);
        }

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Scene"))
            {
                bool toReload = false;

                if (ImGui::BeginMenu("Reload"))
                {
                    if (ImGui::Button("Reload Scene"))
                    {
                        m_config = Engine::Config();
                        toReload = true;
                    }

                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::BeginMenu("Load"))
                {
                    ImGui::InputText("Scene", &m_config.scene);

                    if (ImGui::Button("Load Scene"))
                    {
                        toReload = true;
                    }

                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (toReload)
                {
                    if (m_scene.has_value())
                    {
                        m_scene->Destroy
                        (
                            m_context,
                            m_modelManager,
                            m_megaSet,
                            m_deletionQueues[m_FIF]
                        );
                    }

                    m_scene = Engine::Scene
                    (
                        m_config,
                        cmdBuffer,
                        m_context,
                        m_formatHelper,
                        m_samplers,
                        m_modelManager,
                        m_megaSet,
                        m_iblGenerator,
                        m_deletionQueues[m_FIF]
                    );

                    m_modelManager.Update
                    (
                        cmdBuffer,
                        m_context.device,
                        m_context.allocator,
                        m_megaSet,
                        m_deletionQueues[m_FIF]
                    );

                    m_megaSet.Update(m_context.device);

                    m_taa.ResetHistory();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        m_framebufferManager.Update
        (
            cmdBuffer,
            m_context.device,
            m_context.allocator,
            m_formatHelper,
            m_swapchain.extent,
            m_megaSet,
            m_deletionQueues[m_FIF]
        );

        m_scene->Update
        (
            cmdBuffer,
            m_frameCounter,
            m_window.inputs,
            m_context,
            m_formatHelper,
            m_samplers,
            m_modelManager,
            m_megaSet,
            m_iblGenerator,
            m_deletionQueues[m_FIF]
        );

        m_modelManager.Update
        (
            cmdBuffer,
            m_context.device,
            m_context.allocator,
            m_megaSet,
            m_deletionQueues[m_FIF]
        );

        m_megaSet.Update(m_context.device);

        m_sceneBuffer.WriteScene
        (
            m_FIF,
            m_frameIndex,
            m_context.allocator,
            m_swapchain.extent,
            *m_scene
        );

        if (m_sceneBufferCompute.has_value())
        {
            m_sceneBufferCompute->WriteScene
            (
                m_FIF,
                m_frameIndex,
                m_context.allocator,
                m_swapchain.extent,
                *m_scene
            );
        }

        m_meshBuffer.LoadMeshes
        (
            m_frameIndex,
            m_context.allocator,
            m_modelManager,
            m_scene->renderObjects
        );

        m_indirectBuffer.WriteDrawCalls
        (
            m_FIF,
            m_context.allocator,
            m_modelManager,
            m_scene->renderObjects
        );

        ImGuiDisplay();
    }

    void RenderManager::ImGuiDisplay()
    {
        m_window.inputs.ImGuiDisplay();
        m_modelManager.ImGuiDisplay();
        m_framebufferManager.ImGuiDisplay();
        m_megaSet.ImGuiDisplay();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Memory"))
            {
                std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets = {};
                vmaGetHeapBudgets(m_context.allocator, budgets.data());

                usize usedBytes       = 0;
                usize budgetBytes     = 0;
                usize allocatedBytes  = 0;
                usize allocationCount = 0;
                usize blockCount      = 0;

                for (const auto& budget : budgets)
                {
                    usedBytes       += budget.usage;
                    budgetBytes     += budget.budget;
                    allocatedBytes  += budget.statistics.blockBytes;
                    allocationCount += budget.statistics.allocationCount;
                    blockCount      += budget.statistics.blockCount;
                }

                ImGui::Text("Total Used             | %llu", usedBytes);
                ImGui::Text("Total Allocated        | %llu", allocatedBytes);
                ImGui::Text("Total Available        | %llu", budgetBytes - usedBytes);
                ImGui::Text("Total Budget           | %llu", budgetBytes);
                ImGui::Text("Total Allocation Count | %llu", allocationCount);
                ImGui::Text("Total Block Count      | %llu", blockCount);
                ImGui::Separator();

                for (usize i = 0; i < budgets.size(); ++i)
                {
                    if (budgets[i].budget == 0) continue;

                    if (ImGui::TreeNode(fmt::format("Memory Heap #{}", i).c_str()))
                    {
                        ImGui::Text("Used             | %llu", budgets[i].usage);
                        ImGui::Text("Allocated        | %llu", budgets[i].statistics.allocationBytes);
                        ImGui::Text("Available        | %llu", budgets[i].budget - budgets[i].usage);
                        ImGui::Text("Budget           | %llu", budgets[i].budget);
                        ImGui::Text("Allocation Count | %u",   budgets[i].statistics.allocationCount);
                        ImGui::Text("Block Count      | %u",   budgets[i].statistics.blockCount);

                        ImGui::TreePop();
                    }

                    ImGui::Separator();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Queues"))
            {
                ImGui::Text("Queue    | Family Index | Handle");
                ImGui::Separator();

                ImGui::Text("Graphics | %u            | %p", *m_context.queueFamilies.graphicsFamily, std::bit_cast<void*>(m_context.graphicsQueue));

                if (m_context.queueFamilies.computeFamily.has_value())
                {
                    ImGui::Text("Compute  | %u            | %p", *m_context.queueFamilies.computeFamily, std::bit_cast<void*>(m_context.computeQueue));
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void RenderManager::EndFrame()
    {
        m_graphicsTimeline.TimelineToRenderFinished
        (
            m_frameIndex,
            m_context.graphicsQueue,
            m_swapchain.renderFinishedSemaphores[m_swapchain.imageIndex]
        );

        const auto result = m_swapchain.Present(m_context.device, m_context.graphicsQueue);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_isSwapchainOk = false;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            Vk::CheckResult(result, "Failed to present swapchain image to queue!");
        }

        ++m_frameIndex;
        m_FIF = (m_FIF + 1) % Vk::FRAMES_IN_FLIGHT;

        #ifdef ENGINE_PROFILE
        FrameMark;
        #endif
    }

    bool RenderManager::HandleEvents()
    {
        SDL_Event event = {};

        while ((m_isSwapchainOk ? SDL_PollEvent(&event) : SDL_WaitEvent(&event)))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);

            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                return true;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                if (event.window.data1 > 0 && event.window.data2 > 0)
                {
                    const glm::ivec2 newWindowSize = {event.window.data1, event.window.data2};

                    if (m_window.size != newWindowSize)
                    {
                        m_window.size = newWindowSize;
                        Resize();
                    }
                }
                break;

            case SDL_EVENT_KEY_DOWN:
                switch (event.key.scancode)
                {
                case SDL_SCANCODE_F1:
                    if (!SDL_SetWindowRelativeMouseMode(m_window.handle, !SDL_GetWindowRelativeMouseMode(m_window.handle)))
                    {
                        Logger::Error("SDL_SetWindowRelativeMouseMode Failed: {}\n", SDL_GetError());
                    }
                    break;

                case SDL_SCANCODE_F2:
                    m_scene->camera.isEnabled = !m_scene->camera.isEnabled;
                    break;

                default:
                    break;
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                m_window.inputs.SetMousePosition(glm::vec2(event.motion.xrel, event.motion.yrel));
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                m_window.inputs.SetMouseScroll(glm::vec2(event.wheel.x, event.wheel.y));
                break;

            case SDL_EVENT_GAMEPAD_ADDED:
                if (m_window.inputs.GetGamepad() == nullptr)
                {
                    m_window.inputs.FindGamepad();
                }
                break;

            case SDL_EVENT_GAMEPAD_REMOVED:
                if (m_window.inputs.GetGamepad() != nullptr && event.gdevice.which == m_window.inputs.GetGamepadID())
                {
                    if (const auto gamepad = m_window.inputs.GetGamepad(); gamepad != nullptr)
                    {
                        SDL_CloseGamepad(gamepad);
                    }

                    m_window.inputs.FindGamepad();
                }
                break;

            default:
                continue;
            }
        }

        return false;
    }

    void RenderManager::Resize()
    {
        if (!m_swapchain.IsSurfaceValid(m_window.size, m_context))
        {
            m_isSwapchainOk = false;
            return;
        }

        m_swapchain.RecreateSwapChain(m_context, m_graphicsCmdBufferAllocator);

        m_taa.ResetHistory();

        m_isSwapchainOk = true;

        Render();
    }

    void RenderManager::Init()
    {
        Logger::Debug("Initializing Dear ImGui [Version = {}]\n", ImGui::GetVersion());

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForVulkan(m_window.handle);

        auto& io = ImGui::GetIO();

        io.BackendRendererName = "Rachit's Dear ImGui Backend (Vulkan)";
        io.BackendFlags       |= ImGuiBackendFlags_RendererHasVtxOffset;
        io.ConfigFlags        |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

        Vk::ImmediateSubmit
        (
            m_context.device,
            m_context.graphicsQueue,
            m_graphicsCmdBufferAllocator,
            [&] (const Vk::CommandBuffer& cmdBuffer)
            {
                u8* pixels = nullptr;
                s32 width  = 0;
                s32 height = 0;

                io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

                const usize count = 4ull * static_cast<usize>(width) * static_cast<usize>(height);

                const auto fontID = m_modelManager.textureManager.AddTexture
                (
                    m_context.allocator,
                    m_deletionQueues[m_FIF],
                    Vk::ImageUpload{
                        .type   = Vk::ImageUploadType::RAW,
                        .flags  = Vk::ImageUploadFlags::None,
                        .source = Vk::ImageUploadRawMemory{
                            .name   = "DearImGui/Font",
                            .width  = static_cast<u32>(width),
                            .height = static_cast<u32>(height),
                            .format = VK_FORMAT_R8G8B8A8_UNORM,
                            .data   = std::vector(pixels, pixels + count),
                        }
                    }
                );

                constexpr auto HILBERT_SEQUENCE = Maths::GenerateHilbertSequence<AO::VBGTAO::Occlusion::GTAO_HILBERT_LEVEL>();

                // A bit hacky but what can you do :(
                const auto HILBERT_BEGIN = reinterpret_cast<const u8*>(HILBERT_SEQUENCE.data() + 0);
                const auto HILBERT_END   = reinterpret_cast<const u8*>(HILBERT_SEQUENCE.data() + HILBERT_SEQUENCE.size());

                m_vbgtao.hilbertLUT = m_modelManager.textureManager.AddTexture
                (
                    m_context.allocator,
                    m_deletionQueues[m_FIF],
                    Vk::ImageUpload{
                        .type   = Vk::ImageUploadType::RAW,
                        .flags  = Vk::ImageUploadFlags::None,
                        .source = Vk::ImageUploadRawMemory{
                            .name   = "VBGTAO/HilbertLUT",
                            .width  = AO::VBGTAO::Occlusion::GTAO_HILBERT_WIDTH,
                            .height = AO::VBGTAO::Occlusion::GTAO_HILBERT_WIDTH,
                            .format = VK_FORMAT_R16_UINT,
                            .data   = std::vector(HILBERT_BEGIN, HILBERT_END)
                        }
                    }
                );

                m_modelManager.Update
                (
                    cmdBuffer,
                    m_context.device,
                    m_context.allocator,
                    m_megaSet,
                    m_deletionQueues[m_FIF]
                );

                m_megaSet.Update(m_context.device);

                io.Fonts->SetTexID(static_cast<ImTextureID>(m_modelManager.textureManager.GetTexture(fontID).descriptorID));
            }
        );

        m_globalDeletionQueue.PushDeletor([&] ()
        {
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
        });
    }

    RenderManager::~RenderManager()
    {
        Vk::CheckResult(vkDeviceWaitIdle(m_context.device), "Device failed to idle!");

        for (auto& deletionQueue : m_deletionQueues)
        {
            deletionQueue.FlushQueue();
        }

        m_globalDeletionQueue.FlushQueue();
    }
}