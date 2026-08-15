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

#include "RenderManager.h"

#include "AO/VBAO/HilbertLookupTable.h"
#include "Engine/Inputs.h"
#include "Externals/ImGui.h"
#include "Util/Log.h"
#include "Util/Threads.h"
#include "Util/Files.h"
#include "Util/Span.h"
#include "Vulkan/DebugUtils.h"
#include "Vulkan/ImmediateSubmit.h"
#include "Vulkan/Util.h"

#ifdef ENGINE_PROFILE
#include "Externals/Tracy.h"
#endif

namespace Renderer
{
    RenderManager::RenderManager(Scratch::Allocator& scratchAllocator)
        : m_executor{Util::GetWorkerThreadCount(), std::make_shared<TaskFlow::WorkerInterface>()},
          m_context{m_window.handle, scratchAllocator},
          m_renderConfig{m_context},
          m_swapchain{m_window.size, m_context, scratchAllocator, m_executor},
          m_graphicsCmdBufferAllocator{m_context.device, *m_context.queueFamilies.graphicsFamily},
          m_graphicsTimeline{m_context.device},
          m_megaSet{m_context},
          m_geometryBuffer{m_context, m_stagingPool},
          m_samplers{m_context, m_megaSet, m_textureManager},
          m_toneMap{m_context.formatHelper, m_megaSet, m_pipelineManager, m_framebufferManager},
          m_depth{m_context.formatHelper, m_megaSet, m_pipelineManager, m_framebufferManager},
          m_imGui{m_swapchain, m_megaSet, m_pipelineManager},
          m_skybox{m_context.formatHelper, m_megaSet, m_pipelineManager},
          m_bloom{m_context.device, m_context.formatHelper, m_megaSet, m_pipelineManager, m_framebufferManager},
          m_pointShadow{m_context.formatHelper, m_megaSet, m_pipelineManager, m_framebufferManager},
          m_gBuffer{m_context.formatHelper, m_megaSet, m_pipelineManager, m_framebufferManager},
          m_lighting{m_context.formatHelper, m_megaSet, m_pipelineManager, m_framebufferManager},
          m_shadowRT{m_megaSet, m_pipelineManager, m_framebufferManager},
          m_taa{m_context.formatHelper, m_megaSet, m_pipelineManager, m_framebufferManager},
          m_spotShadow{m_context.formatHelper, m_megaSet, m_pipelineManager, m_framebufferManager},
          m_debug{m_context.device, m_context.allocator, m_context.formatHelper, m_swapchain, m_pipelineManager, m_framebufferManager, m_stagingPool},
          m_culling{m_context.device, m_context.allocator, m_pipelineManager},
          m_vbao{m_megaSet, m_pipelineManager, m_framebufferManager},
          m_tiledLighting{m_megaSet, m_pipelineManager, m_framebufferManager},
          m_exposure{m_context.formatHelper, m_megaSet, m_pipelineManager, m_framebufferManager},
          m_iblGenerator{m_context.device, m_context.allocator, m_context.formatHelper, m_megaSet, m_pipelineManager},
          m_meshBuffer{m_context.device, m_context.allocator},
          m_indirectBuffer{m_context.device, m_context.allocator},
          m_exposureBuffer{m_context.device, m_context.allocator},
          m_sceneBuffer{m_context.device, m_context.allocator, m_renderConfig}
    {
        if (m_renderConfig.multiQueue.isSupported)
        {
            m_asyncCompute = RenderManager::AsyncComputeData
            {
                .cmdBufferAllocator = Vk::CommandBufferAllocator(m_context.device, *m_context.queueFamilies.computeFamily),
                .timeline           = Vk::ComputeTimeline(m_context.device)
            };
        }

        InitImGui(scratchAllocator);

        m_frameCounter.Reset();

        m_globalDeletionQueue.Push([&] ()
        {
            m_sceneBuffer.Destroy(m_context.allocator);
            m_exposureBuffer.Destroy(m_context.allocator);
            m_tiledLightIndexBuffer.Destroy(m_context.allocator);
            m_indirectBuffer.Destroy(m_context.allocator);
            m_meshBuffer.Destroy(m_context.allocator);

            m_iblGenerator.Destroy(m_context.allocator);
            m_culling.Destroy(m_context.allocator);
            m_debug.Destroy(m_context.allocator, m_stagingPool);
            m_shadowRT.Destroy(m_context.allocator);
            m_imGui.Destroy(m_context.allocator);

            m_megaSet.Destroy(m_context.device);
            m_framebufferManager.Destroy(m_context.device, m_context.allocator);
            m_geometryBuffer.Destroy(m_context.allocator, m_stagingPool);
            m_textureManager.Destroy(m_context.device, m_context.allocator);
            m_pipelineManager.Destroy(m_context.device);
            m_imageDownloader.Destroy(m_context.allocator);
            m_stagingPool.Destroy(m_context.allocator);

            m_graphicsTimeline.Destroy(m_context.device);
            m_swapchain.Destroy(m_context.device);
            m_graphicsCmdBufferAllocator.Destroy(m_context.device);

            if (m_accelerationStructure.has_value())
            {
                m_accelerationStructure->Destroy(m_context.device, m_context.allocator);
            }

            if (m_asyncCompute.has_value())
            {
                m_asyncCompute->cmdBufferAllocator.Destroy(m_context.device);
                m_asyncCompute->timeline.Destroy(m_context.device);
            }

            m_renderConfig.Destroy(m_context.device);

            m_context.Destroy();
            m_window.Destroy();
        });
    }

    void RenderManager::Render(Scratch::Allocator& scratchAllocator)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        WaitForTimeline();

        if (!m_isSwapchainOk)
        {
            // Swapchain is not ok, wait for resize event
            return;
        }

        AcquireSwapchainImage();
        BeginFrame();

        if (m_renderConfig.multiQueue.isEnabled)
        {
            RenderMultiQueue(scratchAllocator);
        }
        else
        {
            RenderGraphicsQueueOnly(scratchAllocator);
        }

        EndFrame();
    }

    void RenderManager::WaitForTimeline()
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        if (m_frameIndex < Vk::FRAMES_IN_FLIGHT)
        {
            return;
        }

        m_graphicsTimeline.WaitForStage
        (
            m_frameIndex - Vk::FRAMES_IN_FLIGHT,
            Vk::GraphicsTimeline::Stage::RenderFinished,
            m_context.device
        );
    }

    void RenderManager::AcquireSwapchainImage()
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

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
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        m_deletionQueues[m_FIF].Flush();

        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        m_graphicsCmdBufferAllocator.ResetPool(m_FIF, m_context.device);

        if (m_renderConfig.multiQueue.isEnabled)
        {
            m_asyncCompute->cmdBufferAllocator.ResetPool(m_FIF, m_context.device);
        }
    }

    void RenderManager::RenderGraphicsQueueOnly(Scratch::Allocator& scratchAllocator)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const auto cmdBuffer = m_graphicsCmdBufferAllocator.AllocateCommandBuffer(m_FIF, m_context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            GBufferGeneration(cmdBuffer, scratchAllocator);
            Occlusion(cmdBuffer, m_sceneBuffer.graphicsBuffers, "SceneDepthView", "GNormalView", scratchAllocator);
            TraceRays(cmdBuffer);
            Lighting(cmdBuffer, scratchAllocator);
        cmdBuffer.EndRecording();

        const VkSemaphoreSubmitInfo waitSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = m_graphicsTimeline.semaphore,
            .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::Stage::SwapchainImageAcquired),
            .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        };

        const VkSemaphoreSubmitInfo signalSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = m_graphicsTimeline.semaphore,
            .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::Stage::RenderFinished),
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

    void RenderManager::RenderMultiQueue(Scratch::Allocator& scratchAllocator)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        auto barrierWriter = Vk::ScratchBarrierWriter(scratchAllocator);

        // GBuffer Generation
        {
            const auto gBufferGenerationCmdBuffer = m_graphicsCmdBufferAllocator.AllocateCommandBuffer(m_FIF, m_context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            gBufferGenerationCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

            GBufferGeneration(gBufferGenerationCmdBuffer, scratchAllocator);

            Vk::BeginLabel(gBufferGenerationCmdBuffer, "Graphics -> Async Compute | Release", {0.6726f, 0.6538f, 0.4518f, 1.0f});

            const auto& sceneDepth             = m_framebufferManager.GetFramebuffer("SceneDepth");
            const auto& sceneDepthAsyncCompute = m_framebufferManager.GetFramebuffer("SceneDepthAsyncCompute");
            const auto& gNormal                = m_framebufferManager.GetFramebuffer("GNormal");
            const auto& gNormalAsyncCompute    = m_framebufferManager.GetFramebuffer("GNormalAsyncCompute");
            const auto& depthMipChain          = m_framebufferManager.GetFramebuffer("VBAO/DepthMipChain");
            const auto& depthDifferences       = m_framebufferManager.GetFramebuffer("VBAO/DepthDifferences");
            const auto& noisyAO                = m_framebufferManager.GetFramebuffer("VBAO/NoisyAO");
            const auto& occlusion              = m_framebufferManager.GetFramebuffer("VBAO/Occlusion");
            const auto& hilbertLUT                = m_textureManager.GetTexture(m_vbao.hilbertLUT);

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
            .Execute(gBufferGenerationCmdBuffer);

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
                .srcOffset      = {.x = 0, .y = 0, .z = 0},
                .dstSubresource = {
                    .aspectMask     = sceneDepthAsyncCompute.image.aspect,
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                },
                .dstOffset      = {.x = 0, .y = 0, .z = 0},
                .extent         = {.width = sceneDepth.image.width, .height = sceneDepth.image.height, .depth = 1}
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

            vkCmdCopyImage2(gBufferGenerationCmdBuffer.handle, &depthCopyInfo);

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
                .srcOffset      = {.x = 0, .y = 0, .z = 0},
                .dstSubresource = {
                    .aspectMask     = gNormalAsyncCompute.image.aspect,
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                },
                .dstOffset      = {.x = 0, .y = 0, .z = 0},
                .extent         = {.width = gNormal.image.width, .height = gNormal.image.height, .depth = 1}
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

            vkCmdCopyImage2(gBufferGenerationCmdBuffer.handle, &normalCopyInfo);

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
            .Execute(gBufferGenerationCmdBuffer);

            Vk::EndLabel(gBufferGenerationCmdBuffer);

            gBufferGenerationCmdBuffer.EndRecording();

            const VkSemaphoreSubmitInfo swapchainImageAcquireWaitSemaphoreInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext       = nullptr,
                .semaphore   = m_graphicsTimeline.semaphore,
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::Stage::SwapchainImageAcquired),
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
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::Stage::GBufferGenerationComplete),
                .stageMask   = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
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

        // Async Compute
        {
            const auto asyncComputeCmdBuffer = m_asyncCompute->cmdBufferAllocator.AllocateCommandBuffer(m_FIF, m_context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            asyncComputeCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

            Vk::BeginLabel(asyncComputeCmdBuffer, "Graphics -> Async Compute | Acquire", {0.6726f, 0.6538f, 0.4518f, 1.0f});

            const auto& sceneDepthAsyncCompute = m_framebufferManager.GetFramebuffer("SceneDepthAsyncCompute");
            const auto& gNormalAsyncCompute    = m_framebufferManager.GetFramebuffer("GNormalAsyncCompute");
            const auto& depthMipChain          = m_framebufferManager.GetFramebuffer("VBAO/DepthMipChain");
            const auto& depthDifferences       = m_framebufferManager.GetFramebuffer("VBAO/DepthDifferences");
            const auto& noisyAO                = m_framebufferManager.GetFramebuffer("VBAO/NoisyAO");
            const auto& occlusion              = m_framebufferManager.GetFramebuffer("VBAO/Occlusion");
            const auto& hilbertLUT                = m_textureManager.GetTexture(m_vbao.hilbertLUT);

            barrierWriter
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
            .Execute(asyncComputeCmdBuffer);

            Vk::EndLabel(asyncComputeCmdBuffer);

            Occlusion(asyncComputeCmdBuffer, *m_sceneBuffer.computeBuffers, "SceneDepthAsyncComputeView", "GNormalAsyncComputeView", scratchAllocator);

            Vk::BeginLabel(asyncComputeCmdBuffer, "Async Compute -> Graphics | Release", {0.6726f, 0.6538f, 0.4518f, 1.0f});

            barrierWriter
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
            .Execute(asyncComputeCmdBuffer);

            Vk::EndLabel(asyncComputeCmdBuffer);

            asyncComputeCmdBuffer.EndRecording();

            const VkSemaphoreSubmitInfo gBufferGenerationAsyncComputeWaitSemaphoreInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext       = nullptr,
                .semaphore   = m_graphicsTimeline.semaphore,
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::Stage::GBufferGenerationComplete),
                .stageMask   = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
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
                .semaphore   = m_asyncCompute->timeline.semaphore,
                .value       = m_asyncCompute->timeline.GetTimelineValue(m_frameIndex, Vk::ComputeTimeline::Stage::AsyncComputeFinished),
                .stageMask   = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
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

        // Ray Dispatch
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
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::Stage::GBufferGenerationComplete),
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
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::Stage::RayDispatch),
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

        // Lighting
        {
            const auto lightingCmdBuffer = m_graphicsCmdBufferAllocator.AllocateCommandBuffer(m_FIF, m_context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            lightingCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

            Vk::BeginLabel(lightingCmdBuffer, "Async Compute -> Graphics | Acquire", {0.6726f, 0.6538f, 0.4518f, 1.0f});

            const auto& sceneDepthAsyncCompute = m_framebufferManager.GetFramebuffer("SceneDepthAsyncCompute");
            const auto& gNormalAsyncCompute    = m_framebufferManager.GetFramebuffer("GNormalAsyncCompute");
            const auto& depthMipChain          = m_framebufferManager.GetFramebuffer("VBAO/DepthMipChain");
            const auto& depthDifferences       = m_framebufferManager.GetFramebuffer("VBAO/DepthDifferences");
            const auto& noisyAO                = m_framebufferManager.GetFramebuffer("VBAO/NoisyAO");
            const auto& occlusion              = m_framebufferManager.GetFramebuffer("VBAO/Occlusion");
            const auto& hilbertLUT                = m_textureManager.GetTexture(m_vbao.hilbertLUT);

            barrierWriter
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
                    .levelCount     = hilbertLUT.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = hilbertLUT.image.arrayLayers
                }
            )
            .Execute(lightingCmdBuffer);

            Vk::EndLabel(lightingCmdBuffer);

            Lighting(lightingCmdBuffer, scratchAllocator);

            lightingCmdBuffer.EndRecording();

            const VkSemaphoreSubmitInfo asyncComputeWaitSemaphoreInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext       = nullptr,
                .semaphore   = m_asyncCompute->timeline.semaphore,
                .value       = m_asyncCompute->timeline.GetTimelineValue(m_frameIndex, Vk::ComputeTimeline::Stage::AsyncComputeFinished),
                .stageMask   = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .deviceIndex = 0
            };

            const VkSemaphoreSubmitInfo rayDispatchWaitSemaphoreInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext       = nullptr,
                .semaphore   = m_graphicsTimeline.semaphore,
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::Stage::RayDispatch),
                .stageMask   = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
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
                .value       = m_graphicsTimeline.GetTimelineValue(m_frameIndex, Vk::GraphicsTimeline::Stage::RenderFinished),
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

    void RenderManager::GBufferGeneration(const Vk::CommandBuffer& cmdBuffer, Scratch::Allocator& scratchAllocator)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        if (auto& layout = m_swapchain.imageLayouts[m_swapchain.imageIndex]; layout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            const auto& swapchainImage = m_swapchain.images[m_swapchain.imageIndex];

            swapchainImage.Barrier
            (
                cmdBuffer,
                Vk::ImageBarrier{
                   .srcStageMask    = VK_PIPELINE_STAGE_2_NONE,
                   .srcAccessMask   = VK_ACCESS_2_NONE,
                   .dstStageMask    = VK_PIPELINE_STAGE_2_NONE,
                   .dstAccessMask   = VK_ACCESS_2_NONE,
                   .oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED,
                   .newLayout       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                   .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                   .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                   .baseMipLevel    = 0,
                   .levelCount      = swapchainImage.mipLevels,
                   .baseArrayLayer  = 0,
                   .layerCount      = swapchainImage.arrayLayers
               }
            );

            layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        Update(cmdBuffer, scratchAllocator);

        if (m_sceneEditor.scene->haveRenderObjectsChanged && m_accelerationStructure.has_value())
        {
            m_deletionQueues[m_FIF].Push([device = m_context.device, allocator = m_context.allocator, as = *m_accelerationStructure] () mutable
            {
                as.Destroy(device, allocator);
            });

            m_accelerationStructure = std::nullopt;
        }

        if (!m_accelerationStructure.has_value())
        {
            m_accelerationStructure = Vk::AccelerationStructure{};

            m_accelerationStructure->BuildBottomLevelAS
            (
                m_frameIndex,
                cmdBuffer,
                m_context,
                m_geometryBuffer,
                m_modelManager,
                m_sceneEditor.scene->renderObjects,
                scratchAllocator,
                m_deletionQueues[m_FIF]
            );
        }

        m_accelerationStructure->TryCompactBottomLevelAS
        (
            cmdBuffer,
            m_context,
            m_graphicsTimeline,
            scratchAllocator,
            m_deletionQueues[m_FIF]
        );

        m_accelerationStructure->BuildTopLevelAS
        (
            m_FIF,
            cmdBuffer,
            m_context,
            m_modelManager,
            m_sceneEditor.scene->renderObjects,
            scratchAllocator,
            m_deletionQueues[m_FIF]
        );

        m_pointShadow.Render
        (
            m_FIF,
            m_frameIndex,
            cmdBuffer,
            m_pipelineManager,
            m_framebufferManager,
            m_megaSet,
            m_geometryBuffer,
            m_textureManager,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_samplers,
            m_culling,
            scratchAllocator
        );

        m_spotShadow.Render
        (
            m_FIF,
            m_frameIndex,
            cmdBuffer,
            m_pipelineManager,
            m_framebufferManager,
            m_megaSet,
            m_geometryBuffer,
            m_textureManager,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_samplers,
            m_culling,
            scratchAllocator
        );

        m_depth.Render
        (
            m_FIF,
            m_frameIndex,
            cmdBuffer,
            m_pipelineManager,
            m_framebufferManager,
            m_megaSet,
            m_geometryBuffer,
            m_textureManager,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_samplers,
            m_culling,
            scratchAllocator
        );

        m_gBuffer.Render
        (
            m_FIF,
            m_frameIndex,
            cmdBuffer,
            m_pipelineManager,
            m_framebufferManager,
            m_megaSet,
            m_geometryBuffer,
            m_textureManager,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_samplers,
            scratchAllocator
        );
    }

    void RenderManager::Occlusion
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Buffers::SceneBuffer::Buffers& sceneBuffers,
        const std::string_view sceneDepthID,
        const std::string_view gNormalID,
        Scratch::Allocator& scratchAllocator
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        m_vbao.Execute
        (
            m_FIF,
            m_frameIndex,
            sceneDepthID,
            gNormalID,
            cmdBuffer,
            m_renderConfig,
            m_pipelineManager,
            m_framebufferManager,
            m_megaSet,
            sceneBuffers,
            m_samplers,
            m_textureManager,
            scratchAllocator
        );
    }

    void RenderManager::TraceRays(const Vk::CommandBuffer& cmdBuffer)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        bool canPerformRayDispatch = false;

        if (m_accelerationStructure.has_value())
        {
            canPerformRayDispatch = m_accelerationStructure->topLevelASes[m_FIF].handle != VK_NULL_HANDLE;
        }

        if (canPerformRayDispatch)
        {
            m_shadowRT.TraceRays
            (
                m_FIF,
                m_frameIndex,
                cmdBuffer,
                m_renderConfig,
                m_context,
                m_megaSet,
                m_geometryBuffer,
                m_textureManager,
                m_pipelineManager,
                m_framebufferManager,
                m_sceneBuffer,
                m_meshBuffer,
                m_samplers,
                m_sceneEditor.scene->camera,
                *m_accelerationStructure,
                m_stagingPool,
                m_deletionQueues[m_FIF]
            );
        }
        else
        {
            Vk::BeginLabel(cmdBuffer, "Raytraced Shadows", glm::vec4(0.4196f, 0.2488f, 0.6588f, 1.0f));

            m_shadowRT.Clear(cmdBuffer, m_framebufferManager);

            Vk::EndLabel(cmdBuffer);
        }
    }

    void RenderManager::Lighting(const Vk::CommandBuffer& cmdBuffer, Scratch::Allocator& scratchAllocator)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        m_tiledLighting.Execute
        (
            m_FIF,
            cmdBuffer,
            m_context,
            m_pipelineManager,
            m_framebufferManager,
            m_megaSet,
            m_textureManager,
            m_sceneBuffer,
            m_samplers,
            m_tiledLightIndexBuffer,
            m_deletionQueues[m_FIF]
        );

        const auto iblMaps = m_iblGenerator.GetIBLMaps(m_sceneEditor.scene->iblMapsID);

        m_lighting.Render
        (
            m_FIF,
            cmdBuffer,
            m_pipelineManager,
            m_framebufferManager,
            m_megaSet,
            m_sceneBuffer,
            m_tiledLightIndexBuffer,
            m_samplers,
            iblMaps,
            m_textureManager
        );

        m_skybox.Render
        (
            m_FIF,
            cmdBuffer,
            m_pipelineManager,
            m_framebufferManager,
            m_megaSet,
            m_geometryBuffer,
            m_sceneBuffer,
            m_samplers,
            iblMaps,
            m_textureManager,
            scratchAllocator
        );

        AntiAliasing(cmdBuffer, scratchAllocator);

        m_exposure.Execute
        (
            m_FIF,
            cmdBuffer,
            m_pipelineManager,
            m_framebufferManager,
            m_megaSet,
            m_textureManager,
            m_exposureBuffer,
            m_samplers,
            m_frameCounter,
            scratchAllocator
        );

        m_bloom.Render
        (
            cmdBuffer,
            m_pipelineManager,
            m_framebufferManager,
            m_megaSet,
            m_textureManager,
            m_samplers
        );

        m_toneMap.Render
        (
            cmdBuffer,
            m_pipelineManager,
            m_framebufferManager,
            m_megaSet,
            m_textureManager,
            m_samplers
        );

        BlitToSwapchain(cmdBuffer, scratchAllocator);

        m_debug.Render
        (
            m_FIF,
            m_frameIndex,
            cmdBuffer,
            m_context,
            m_pipelineManager,
            m_framebufferManager,
            m_swapchain,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_tiledLightIndexBuffer,
            m_stagingPool,
            scratchAllocator,
            m_deletionQueues[m_FIF]
        );

        m_imGui.Render
        (
            m_FIF,
            cmdBuffer,
            m_context,
            m_pipelineManager,
            m_swapchain,
            m_samplers,
            m_megaSet,
            m_stagingPool,
            m_textureManager,
            m_executor,
            scratchAllocator,
            m_deletionQueues[m_FIF]
        );
    }

    void RenderManager::AntiAliasing(const Vk::CommandBuffer& cmdBuffer, Scratch::Allocator& scratchAllocator)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        switch (m_renderConfig.antiAliasingMode)
        {
        case RenderConfig::AntiAliasingMode::None:
        {
            Vk::BeginLabel(cmdBuffer, "Blit To Resolved", glm::vec4(0.4098f, 0.2843f, 0.7529f, 1.0f));

            const auto& sceneColorView = m_framebufferManager.GetFramebufferView("SceneColorView");
            const auto& resolvedView   = m_framebufferManager.GetFramebufferView("ResolvedSceneColorView");

            const auto& sceneColor = m_framebufferManager.GetFramebuffer(sceneColorView.framebuffer);
            const auto& resolved   = m_framebufferManager.GetFramebuffer(resolvedView.framebuffer);

            auto barrierWriter = Vk::ScratchBarrierWriter(scratchAllocator);

            barrierWriter
            .WriteImageBarrier(
                sceneColor.image,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                    .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .baseMipLevel   = 0,
                    .levelCount     = sceneColor.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = sceneColor.image.arrayLayers
                }
            )
            .WriteImageBarrier(
                resolved.image,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                    .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .baseMipLevel   = 0,
                    .levelCount     = resolved.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = resolved.image.arrayLayers
                }
            )
            .Execute(cmdBuffer);

            const VkImageBlit2 blitRegion =
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                .pNext = nullptr,
                .srcSubresource = {
                    .aspectMask     = sceneColor.image.aspect,
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = sceneColor.image.arrayLayers
                },
                .srcOffsets = {
                    {.x = 0, .y = 0, .z = 0},
                    {.x = static_cast<s32>(sceneColor.image.width), .y = static_cast<s32>(sceneColor.image.height), .z = 1}
                },
                .dstSubresource = {
                    .aspectMask     = resolved.image.aspect,
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = resolved.image.arrayLayers
                },
                .dstOffsets = {
                    {.x = 0, .y = 0, .z = 0},
                    {.x = static_cast<s32>(resolved.image.width), .y = static_cast<s32>(resolved.image.height), .z = 1}
                }
            };

            const VkBlitImageInfo2 blitImageInfo =
            {
                .sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                .pNext          = nullptr,
                .srcImage       = sceneColor.image.handle,
                .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .dstImage       = resolved.image.handle,
                .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .regionCount    = 1,
                .pRegions       = &blitRegion,
                .filter         = VK_FILTER_LINEAR
            };

            vkCmdBlitImage2(cmdBuffer.handle, &blitImageInfo);

            barrierWriter
            .WriteImageBarrier(
                sceneColor.image,
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
                    .levelCount     = sceneColor.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = sceneColor.image.arrayLayers
                }
            )
            .WriteImageBarrier(
                resolved.image,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .baseMipLevel   = 0,
                    .levelCount     = resolved.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = resolved.image.arrayLayers
                }
            )
            .Execute(cmdBuffer);

            Vk::EndLabel(cmdBuffer);

            break;
        }

        case RenderConfig::AntiAliasingMode::TAA:
        {
            m_taa.Render
            (
                m_frameIndex,
                cmdBuffer,
                m_pipelineManager,
                m_framebufferManager,
                m_megaSet,
                m_textureManager,
                m_samplers,
                scratchAllocator
            );

            break;
        }

        #ifdef ENGINE_DLSS
        case RenderConfig::AntiAliasingMode::DLSS:
        {
            m_DLSS.Evaluate
            (
                m_frameIndex,
                cmdBuffer,
                m_framebufferManager,
                m_frameCounter,
                m_renderConfig.DLSSConfig
            );

            break;
        }
        #endif

        default:
            Logger::Error("{}\n", "Unknown anti-aliasing mode!");
        }
    }

    void RenderManager::BlitToSwapchain(const Vk::CommandBuffer& cmdBuffer, Scratch::Allocator& scratchAllocator)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const auto& finalColor     = m_framebufferManager.GetFramebuffer("FinalColor");
        const auto& swapchainImage = m_swapchain.images[m_swapchain.imageIndex];

        Vk::BeginLabel(cmdBuffer, "Blit To Swapchain", glm::vec4(0.4098f, 0.2843f, 0.7529f, 1.0f));

        auto barrierWriter = Vk::ScratchBarrierWriter(scratchAllocator);

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
            .sType          = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .pNext          = nullptr,
            .srcSubresource = {
                .aspectMask     = finalColor.image.aspect,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = finalColor.image.arrayLayers
            },
            .srcOffsets     = {
                {.x = 0, .y = 0, .z = 0},
                {.x = static_cast<s32>(finalColor.image.width), .y = static_cast<s32>(finalColor.image.height), .z = 1}
            },
            .dstSubresource = {
                .aspectMask     = swapchainImage.aspect,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = swapchainImage.arrayLayers
            },
            .dstOffsets     = {
                {.x = 0, .y = 0, .z = 0},
                {.x = static_cast<s32>(swapchainImage.width), .y = static_cast<s32>(swapchainImage.height), .z = 1}
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

    void RenderManager::Update(const Vk::CommandBuffer& cmdBuffer, Scratch::Allocator& scratchAllocator)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        m_frameCounter.Update();

        m_renderConfig.Update();

        m_pipelineManager.Update
        (
            m_context.device,
            scratchAllocator,
            m_executor,
            m_deletionQueues[m_FIF]
        );

        m_stagingPool.Update(m_context.allocator);

        m_framebufferManager.Update
        (
            cmdBuffer,
            m_context,
            m_swapchain,
            m_renderConfig,
            m_megaSet,
            scratchAllocator,
            m_deletionQueues[m_FIF]
        );

        #ifdef ENGINE_DLSS
        if (m_renderConfig.antiAliasingMode == RenderConfig::AntiAliasingMode::DLSS)
        {
            m_renderConfig.DLSSConfig.UpdateDLSSFeature
            (
                cmdBuffer,
                glm::vk_cast(m_swapchain.extent),
                m_deletionQueues[m_FIF]
            );
        }
        #endif

        const bool sceneChanged = m_sceneEditor.Update
        (
            m_frameCounter,
            m_inputs,
            m_modelManager,
            m_iblGenerator
        );

        if (sceneChanged)
        {
            m_taa.ResetHistory();
            m_exposure.ResetLuminance();

            #ifdef ENGINE_DLSS
            m_renderConfig.DLSSConfig.resetNeeded = true;
            #endif
        }

        m_samplers.Update
        (
            m_context,
            m_framebufferManager.renderExtent,
            m_framebufferManager.displayExtent,
            m_megaSet,
            m_textureManager,
            m_deletionQueues[m_FIF]
        );

        m_iblGenerator.Update
        (
            cmdBuffer,
            m_pipelineManager,
            m_context,
            m_samplers,
            m_geometryBuffer,
            m_textureManager,
            m_megaSet,
            m_stagingPool,
            m_imageDownloader,
            m_executor,
            scratchAllocator,
            m_deletionQueues[m_FIF]
        );

        m_modelManager.Update
        (
            cmdBuffer,
            m_context,
            m_megaSet,
            m_stagingPool,
            m_geometryBuffer,
            m_textureManager,
            scratchAllocator,
            m_executor,
            m_deletionQueues[m_FIF]
        );

        m_geometryBuffer.Update
        (
            cmdBuffer,
            m_context,
            m_stagingPool,
            scratchAllocator,
            m_deletionQueues[m_FIF]
        );

        m_sceneBuffer.Write
        (
            m_FIF,
            m_frameIndex,
            m_context.allocator,
            m_framebufferManager.renderExtent,
            m_framebufferManager.displayExtent,
            *m_sceneEditor.scene,
            m_renderConfig,
            m_modelManager
        );

        m_indirectBuffer.ComputeDrawCount(m_modelManager, m_sceneEditor.scene->renderObjects);

        m_textureManager.Update
        (
            m_context.device,
            cmdBuffer,
            m_megaSet,
            scratchAllocator
        );

        m_imageDownloader.Update
        (
            m_frameIndex,
            cmdBuffer,
            m_context,
            m_graphicsTimeline,
            scratchAllocator,
            m_executor
        );

        m_meshBuffer.LoadMeshes
        (
            m_frameIndex,
            m_context.allocator,
            m_modelManager,
            m_textureManager,
            scratchAllocator,
            m_sceneEditor.scene->renderObjects
        );

        ImGuiDisplay(scratchAllocator);
    }

    void RenderManager::ImGuiDisplay(Scratch::Allocator& scratchAllocator)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        m_inputs.ImGuiDisplay();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Renderer"))
            {
                m_modelManager.ImGuiDisplay();
                m_geometryBuffer.ImGuiDisplay(scratchAllocator);
                m_textureManager.ImGuiDisplay();
                m_framebufferManager.ImGuiDisplay();
                m_megaSet.ImGuiDisplay();
                m_pipelineManager.ImGuiDisplay();

                if (ImGui::CollapsingHeader("Memory"))
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

                    if (ImGui::BeginTable("##DeviceMemoryTable", 7, ImGuiTableFlags_Borders))
                    {
                        ImGui::TableSetupColumn("Heap");
                        ImGui::TableSetupColumn("Used");
                        ImGui::TableSetupColumn("Allocated");
                        ImGui::TableSetupColumn("Available");
                        ImGui::TableSetupColumn("Budget");
                        ImGui::TableSetupColumn("Allocation Count");
                        ImGui::TableSetupColumn("Block Count");

                        ImGui::TableSetupScrollFreeze(0, 0);

                        ImGui::TableHeadersRow();

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("Total");
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%llu", usedBytes);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%llu", allocatedBytes);
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%llu", budgetBytes - usedBytes);
                        ImGui::TableSetColumnIndex(4);
                        ImGui::Text("%llu", budgetBytes);
                        ImGui::TableSetColumnIndex(5);
                        ImGui::Text("%llu", allocationCount);
                        ImGui::TableSetColumnIndex(6);
                        ImGui::Text("%llu", blockCount);

                        for (usize i = 0; i < budgets.size(); ++i)
                        {
                            if (budgets[i].budget == 0)
                            {
                                continue;
                            }

                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("#%llu", i);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%llu", budgets[i].usage);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%llu", budgets[i].statistics.allocationBytes);
                            ImGui::TableSetColumnIndex(3);
                            ImGui::Text("%llu", budgets[i].budget - budgets[i].usage);
                            ImGui::TableSetColumnIndex(4);
                            ImGui::Text("%llu", budgets[i].budget);
                            ImGui::TableSetColumnIndex(5);
                            ImGui::Text("%u", budgets[i].statistics.allocationCount);
                            ImGui::TableSetColumnIndex(6);
                            ImGui::Text("%u", budgets[i].statistics.blockCount);
                        }

                        ImGui::EndTable();
                    }
                }

                if (ImGui::CollapsingHeader("Queues"))
                {
                    if (ImGui::BeginTable("##DeviceQueueTable", 3, ImGuiTableFlags_Borders))
                    {
                        ImGui::TableSetupColumn("Queue");
                        ImGui::TableSetupColumn("Family Index");
                        ImGui::TableSetupColumn("Handle");

                        ImGui::TableSetupScrollFreeze(0, 0);

                        ImGui::TableHeadersRow();

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("Graphics");
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%u", *m_context.queueFamilies.graphicsFamily);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%p", reinterpret_cast<void*>(m_context.graphicsQueue));

                        if (m_renderConfig.multiQueue.isSupported)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("Compute");
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%u", *m_context.queueFamilies.computeFamily);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%p", reinterpret_cast<void*>(m_context.computeQueue));
                        }

                        ImGui::EndTable();
                    }
                }

                if (ImGui::CollapsingHeader("Device"))
                {
                    ImGui::Text("Device | %s", m_context.physicalDeviceName.c_str());
                }

                if (ImGui::CollapsingHeader("Swapchain"))
                {
                    ImGui::Text("Format       | %s",       string_VkFormat(m_swapchain.surfaceFormat.format));
                    ImGui::Text("Color Space  | %s",       string_VkColorSpaceKHR(m_swapchain.surfaceFormat.colorSpace));
                    ImGui::Text("Present Mode | %s",       string_VkPresentModeKHR(m_swapchain.presentMode));
                    ImGui::Text("Extent       | [%u, %u]", m_swapchain.extent.width, m_swapchain.extent.height);
                    ImGui::Text("Image Count  | %llu",     m_swapchain.images.size());
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Engine"))
            {
                if (ImGui::CollapsingHeader("Scratch Allocator"))
                {
                    ImGui::Text("Allocated Last Frame | %llu bytes", scratchAllocator.bytesUsedBeforeReset);
                    ImGui::Text("Peak Memory Usage    | %llu bytes", scratchAllocator.peakMemoryUsage);
                }

                if (ImGui::CollapsingHeader("Debug/Fonts"))
                {
                    ImGui::Text("EN: The Legend of Zelda: Breath of The Wild");
                    ImGui::Text("JP: ゼルダの伝説　ブレス オブ ザ ワイルド");
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void RenderManager::EndFrame()
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

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

    bool RenderManager::HandleEvents(Scratch::Allocator& scratchAllocator)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        m_inputs.Reset();

        SDL_Event event = {};

        while ((m_isSwapchainOk ? SDL_PollEvent(&event) : SDL_WaitEvent(&event)))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);

            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                return true;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            {
                if (event.window.data1 > 0 && event.window.data2 > 0)
                {
                    const glm::ivec2 newWindowSize = {event.window.data1, event.window.data2};

                    if (m_window.size != newWindowSize)
                    {
                        m_window.size = newWindowSize;

                        Resize(scratchAllocator);
                    }
                }

                break;
            }

            case SDL_EVENT_WINDOW_MINIMIZED:
                Resize(scratchAllocator);
                break;

            case SDL_EVENT_KEY_DOWN:
            {
                switch (event.key.scancode)
                {
                case SDL_SCANCODE_F1:
                {
                    if (!SDL_SetWindowRelativeMouseMode(m_window.handle, !SDL_GetWindowRelativeMouseMode(m_window.handle)))
                    {
                        Logger::Warning("SDL_SetWindowRelativeMouseMode Failed: {}\n", SDL_GetError());
                    }

                    break;
                }

                case SDL_SCANCODE_F2:
                    m_sceneEditor.scene->camera.isEnabled = !m_sceneEditor.scene->camera.isEnabled;
                    break;

                case SDL_SCANCODE_F11:
                    m_window.ToggleFullScreen();
                    break;

                default:
                    break;
                }

                break;
            }

            case SDL_EVENT_MOUSE_MOTION:
                m_inputs.SetRelativeMouseMovement(glm::vec2(event.motion.xrel, event.motion.yrel));
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                m_inputs.SetMouseScroll(glm::vec2(event.wheel.x, event.wheel.y));
                break;

            case SDL_EVENT_GAMEPAD_ADDED:
            {
                if (m_inputs.gamepad == nullptr)
                {
                    m_inputs.FindGamepad();
                }

                break;
            }

            case SDL_EVENT_GAMEPAD_REMOVED:
            {
                if (m_inputs.gamepad != nullptr && event.gdevice.which == m_inputs.GetGamepadID())
                {
                    SDL_CloseGamepad(m_inputs.gamepad);

                    m_inputs.FindGamepad();
                }

                break;
            }

            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            {
                switch (event.gbutton.button)
                {
                case SDL_GAMEPAD_BUTTON_DPAD_UP:
                    m_window.ToggleFullScreen();
                    break;

                case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
                    m_sceneEditor.scene->camera.isEnabled = !m_sceneEditor.scene->camera.isEnabled;
                    break;

                default:
                    break;
                }

                break;
            }

            default:
                continue;
            }
        }

        return false;
    }

    void RenderManager::Resize(Scratch::Allocator& scratchAllocator)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        if (!m_swapchain.IsSurfaceValid(m_window.size, m_context))
        {
            m_isSwapchainOk = false;
            return;
        }

        m_swapchain.RecreateSwapChain(m_context, scratchAllocator, m_executor);

        m_taa.ResetHistory();
        m_exposure.ResetLuminance();

        m_isSwapchainOk = true;

        Render(scratchAllocator);
    }

    void RenderManager::InitImGui(Scratch::Allocator& scratchAllocator)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForVulkan(m_window.handle);

        auto& io    = ImGui::GetIO();
        auto& style = ImGui::GetStyle();

        io.BackendRendererName = "Rachit's Dear ImGui Backend (Vulkan)";
        io.BackendFlags       |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
        io.ConfigFlags        |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

        const f32 scale = SDL_GetWindowDisplayScale(m_window.handle);

        style.FontScaleDpi = scale;
        style.ScaleAllSizes(scale);

        io.Fonts->AddFontDefaultVector();

        ImFontConfig config;
        config.MergeMode = true;

        io.Fonts->AddFontFromFileTTF
        (
            Files::GetAssetPath("Fonts/", "NotoCJK/NotoSansCJKjp-Regular.otf").c_str(),
            0.0f,
            &config
        );

        ImPlot::CreateContext();

        // TODO: Move this to frame 0
        Vk::ImmediateSubmit
        (
            m_context.device,
            m_context.graphicsQueue,
            m_graphicsCmdBufferAllocator,
            [&] (const Vk::CommandBuffer& cmdBuffer)
            {
                m_vbao.hilbertLUT = m_textureManager.LoadTexture
                (
                    m_context.device,
                    m_context.allocator,
                    m_stagingPool,
                    m_executor,
                    m_deletionQueues[m_FIF],
                    Vk::ImageUpload{
                        .type   = Vk::ImageUploadType::RAW,
                        .flags  = Vk::ImageUploadFlags::None,
                        .source = Vk::ImageUploadRawMemory{
                            .name   = "VBAO/HilbertLUT",
                            .width  = AO::VBAO::Occlusion::VBAO_HILBERT_WIDTH,
                            .height = AO::VBAO::Occlusion::VBAO_HILBERT_WIDTH,
                            .format = VK_FORMAT_R16_UINT,
                            .data   = Util::ToBytes(AO::VBAO::HILBERT_SEQUENCE)
                        }
                    }
                );

                m_textureManager.ForceUpdate
                (
                    m_vbao.hilbertLUT,
                    m_context.device,
                    cmdBuffer,
                    m_megaSet,
                    scratchAllocator
                );
            }
        );

        m_globalDeletionQueue.Push([&] ()
        {
            ImPlot::DestroyContext();

            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
        });
    }

    RenderManager::~RenderManager()
    {
        Vk::CheckResult(vkDeviceWaitIdle(m_context.device), "Device failed to idle!");

        m_executor.wait_for_all();

        for (auto& deletionQueue : m_deletionQueues)
        {
            deletionQueue.Flush();
        }

        m_globalDeletionQueue.Flush();
    }
}