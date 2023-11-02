/*
 * Copyright 2023 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "RenderManager.h"

#include "RenderConstants.h"
#include "Util/Log.h"
#include "Util/Maths.h"
#include "RenderPipeline.h"

namespace Renderer
{
    RenderManager::RenderManager(std::shared_ptr<Engine::Window> window)
        : m_window(std::move(window)),
          m_vkContext(std::make_shared<Vk::Context>(m_window->handle)),
          m_renderPipeline(std::make_shared<RenderPipeline>(m_vkContext))
    {
    }

    void RenderManager::Render()
    {
        // Make sure swap chain is valid
        if (!IsSwapchainValid()) return;

        // Begin
        BeginFrame();
        // Update
        Update();

        // Bind buffer
        m_vkContext->vertexBuffer->BindBuffer(m_vkContext->commandBuffers[m_currentFrame]);

        // Load
        vkCmdPushConstants
        (
            m_vkContext->commandBuffers[m_currentFrame],
            m_renderPipeline->pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(decltype(m_renderPipeline->pushConstants[m_currentFrame])),
            reinterpret_cast<void*>(&m_renderPipeline->pushConstants[m_currentFrame])
        );

        // Draw triangle
        vkCmdDrawIndexed
        (
            m_vkContext->commandBuffers[m_currentFrame],
            m_vkContext->vertexBuffer->indexCount,
            1,
            0,
            0,
            0
        );

        // End
        EndFrame();
        // Present
        Present();
    }

    void RenderManager::Update()
    {
        // Get current push constant
        auto& pushConstant = m_renderPipeline->pushConstants[m_currentFrame];

        // Staring time
        static auto startTime = std::chrono::high_resolution_clock::now();
        // Current time
        auto currentTime = std::chrono::high_resolution_clock::now();
        // Durations
        f32 time = std::chrono::duration<f32, std::chrono::seconds::period>(currentTime - startTime).count();

        // View Matrix
        auto view = glm::lookAt
        (
            glm::vec3(0.0f, 0.0f, 2.5f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        // Projection matrix
        auto proj = glm::perspective
        (
            FOV,
            static_cast<f32>(m_vkContext->swapChainExtent.width) / static_cast<f32>(m_vkContext->swapChainExtent.height),
            PLANES.x,
            PLANES.y
        );
        // Flip projection
        proj[1][1] *= -1;

        // Create model matrix
        pushConstant.modelMatrix = Maths::CreateModelMatrix<glm::mat4>
        (
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 100.0f * time * glm::radians(90.0f), 0.0f),
            glm::vec3(1.4f, 1.4f, 1.4f)
        );

        pushConstant.modelMatrix = proj * view * pushConstant.modelMatrix; // FIXME: Currently cheating here lmao
    }

    void RenderManager::BeginFrame()
    {
        // Wait for previous frame to be finished
        WaitForFrame();

        // Reset command buffer
        vkResetCommandBuffer(m_vkContext->commandBuffers[m_currentFrame], 0);

        // Begin info
        VkCommandBufferBeginInfo beginInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .pInheritanceInfo = nullptr
        };

        // Begin recording commands
        if (vkBeginCommandBuffer(
                m_vkContext->commandBuffers[m_currentFrame],
                &beginInfo
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR
            (
                "Failed to begin recording commands! [CommandBuffer={}]\n",
                reinterpret_cast<void*>(m_vkContext->commandBuffers[m_currentFrame])
            );
        }

        // Clear color (weird ahh notation)
        VkClearValue clearValue =
        {.color =
            {.float32 =
                {
                    Renderer::CLEAR_COLOR.r,
                    Renderer::CLEAR_COLOR.g,
                    Renderer::CLEAR_COLOR.b,
                    Renderer::CLEAR_COLOR.a
                }
            }
        };

        // Render pass begin info
        VkRenderPassBeginInfo renderPassInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext           = nullptr,
            .renderPass      = m_vkContext->renderPass,
            .framebuffer     = m_vkContext->swapChainFrameBuffers[m_imageIndex],
            .renderArea      = {
                .offset = {0, 0},
                .extent = m_vkContext->swapChainExtent
            },
            .clearValueCount = 1,
            .pClearValues    = &clearValue
        };

        // Begin render pass
        vkCmdBeginRenderPass
        (
            m_vkContext->commandBuffers[m_currentFrame],
            &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE
        );

        // Bind pipeline
        vkCmdBindPipeline
        (
            m_vkContext->commandBuffers[m_currentFrame],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_renderPipeline->pipeline
        );

        // Viewport
        VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(m_vkContext->swapChainExtent.width),
            .height   = static_cast<f32>(m_vkContext->swapChainExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        // Set viewport
        vkCmdSetViewport
        (
            m_vkContext->commandBuffers[m_currentFrame],
            0,
            1,
            &viewport
        );

        // Scissor
        VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = m_vkContext->swapChainExtent
        };

        // Set scissor
        vkCmdSetScissor
        (
            m_vkContext->commandBuffers[m_currentFrame],
            0,
            1,
            &scissor
        );
    }

    void RenderManager::WaitForFrame()
    {
        // Wait for previous frame
        vkWaitForFences
        (
            m_vkContext->device,
            1,
            &m_vkContext->inFlightFences[m_currentFrame],
            VK_TRUE,
            std::numeric_limits<u64>::max()
        );

        // Get image index
        AcquireSwapChainImage();

        // Reset fence
        vkResetFences(m_vkContext->device, 1, &m_vkContext->inFlightFences[m_currentFrame]);
    }

    void RenderManager::AcquireSwapChainImage()
    {
        // Query
        m_swapchainStatus[0] = vkAcquireNextImageKHR
        (
            m_vkContext->device,
            m_vkContext->swapChain,
            std::numeric_limits<u64>::max(),
            m_vkContext->imageAvailableSemaphores[m_currentFrame],
            VK_NULL_HANDLE,
            &m_imageIndex
        );
    }

    void RenderManager::SubmitQueue()
    {
        // Waiting semaphores
        VkSemaphore waitSemaphores[] = {m_vkContext->imageAvailableSemaphores[m_currentFrame]};
        // Signal semaphores
        VkSemaphore signalSemaphores[] = {m_vkContext->renderFinishedSemaphores[m_currentFrame]};
        // Pipeline stage flags
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        // Queue submit info
        VkSubmitInfo submitInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = 1,
            .pWaitSemaphores      = waitSemaphores,
            .pWaitDstStageMask    = waitStages,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &m_vkContext->commandBuffers[m_currentFrame],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = signalSemaphores
        };

        // Submit queue
        if (vkQueueSubmit(
                m_vkContext->graphicsQueue,
                1,
                &submitInfo,
                m_vkContext->inFlightFences[m_currentFrame]
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR
            (
                "Failed to submit draw command buffer! [CommandBuffer={}] [Queue={}]\n",
                reinterpret_cast<void*>(m_vkContext->commandBuffers[m_currentFrame]),
                reinterpret_cast<void*>(m_vkContext->graphicsQueue)
            );
        }
    }

    void RenderManager::Present()
    {
        // Swap chains
        VkSwapchainKHR swapChains[] = {m_vkContext->swapChain};
        // Signal semaphores
        VkSemaphore signalSemaphores[] = {m_vkContext->renderFinishedSemaphores[m_currentFrame]};

        // Presentation info
        VkPresentInfoKHR presentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext              = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = signalSemaphores,
            .swapchainCount     = 1,
            .pSwapchains        = swapChains,
            .pImageIndices      = &m_imageIndex,
            .pResults           = nullptr
        };

        // Present
        m_swapchainStatus[1] = vkQueuePresentKHR(m_vkContext->graphicsQueue, &presentInfo);

        // Set frame index
        m_currentFrame = (m_currentFrame + 1) % Vk::MAX_FRAMES_IN_FLIGHT;
    }

    void RenderManager::EndFrame()
    {
        // End render pass
        vkCmdEndRenderPass(m_vkContext->commandBuffers[m_currentFrame]);

        // Finish recording command buffer
        if (vkEndCommandBuffer(m_vkContext->commandBuffers[m_currentFrame]) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR
            (
                "Failed to record command buffer! [handle={}]\n",
                reinterpret_cast<void*>(m_vkContext->commandBuffers[m_currentFrame])
            );
        }

        // Submit queue
        SubmitQueue();
    }

    bool RenderManager::IsSwapchainValid()
    {
        // Flag
        bool toRecreate = false;

        // Loop
        for (auto status : m_swapchainStatus)
        {
            // Check swapchain
            if (status == VK_ERROR_OUT_OF_DATE_KHR || status == VK_SUBOPTIMAL_KHR)
            {
                // We'll have to recreate the swap buffers now
                toRecreate = true;
            }
            else if (status != VK_SUCCESS)
            {
                // Log
                LOG_ERROR("Swap chain validation failed! [status={:X}]\n", static_cast<int>(status));
            }
        }

        // If we have to recreate
        if (toRecreate)
        {
            // Recreate
            m_vkContext->RecreateSwapChain(m_window);
            // Reset
            m_swapchainStatus = {VK_SUCCESS, VK_SUCCESS};
            // Return
            return false;
        }

        // No issues
        return true;
    }

    void RenderManager::WaitForLogicalDevice()
    {
        // Wait
        vkDeviceWaitIdle(m_vkContext->device);
    }
}