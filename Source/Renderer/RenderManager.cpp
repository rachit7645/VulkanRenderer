#include "RenderManager.h"

#include <limits>

#include "RenderConstants.h"
#include "Util/Log.h"

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
        // Draw triangle
        vkCmdDraw
        (
            m_vkContext->commandBuffers[currentFrame],
            3,
            1,
            0,
            0
        );
    }

    void RenderManager::BeginFrame()
    {
        // Wait for previous frame to be finished
        WaitForFrame();

        // Reset command buffer
        vkResetCommandBuffer(m_vkContext->commandBuffers[currentFrame], 0);

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
                m_vkContext->commandBuffers[currentFrame],
                &beginInfo
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR
            (
                "Failed to begin recording commands! [CommandBuffer={}]\n",
                reinterpret_cast<void*>(m_vkContext->commandBuffers[currentFrame])
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
            m_vkContext->commandBuffers[currentFrame],
            &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE
        );

        // Bind pipeline
        vkCmdBindPipeline
        (
            m_vkContext->commandBuffers[currentFrame],
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
            m_vkContext->commandBuffers[currentFrame],
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
            m_vkContext->commandBuffers[currentFrame],
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
            &m_vkContext->inFlightFences[currentFrame],
            VK_TRUE,
            std::numeric_limits<u64>::max()
        );

        // Get image index
        AcquireSwapChainImage();

        // Reset fence
        vkResetFences(m_vkContext->device, 1, &m_vkContext->inFlightFences[currentFrame]);
    }

    void RenderManager::AcquireSwapChainImage()
    {
        // Query
        vkAcquireNextImageKHR
        (
            m_vkContext->device,
            m_vkContext->swapChain,
            std::numeric_limits<u64>::max(),
            m_vkContext->imageAvailableSemaphores[currentFrame],
            VK_NULL_HANDLE,
            &m_imageIndex
        );
    }

    void RenderManager::SubmitQueue()
    {
        // Waiting semaphores
        VkSemaphore waitSemaphores[] = {m_vkContext->imageAvailableSemaphores[currentFrame]};
        // Signal semaphores
        VkSemaphore signalSemaphores[] = {m_vkContext->renderFinishedSemaphores[currentFrame]};
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
            .pCommandBuffers      = &m_vkContext->commandBuffers[currentFrame],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = signalSemaphores
        };

        // Submit queue
        if (vkQueueSubmit(
                m_vkContext->graphicsQueue,
                1,
                &submitInfo,
                m_vkContext->inFlightFences[currentFrame]
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR
            (
                "Failed to submit draw command buffer! [CommandBuffer={}] [Queue={}]\n",
                reinterpret_cast<void*>(m_vkContext->commandBuffers[currentFrame]),
                reinterpret_cast<void*>(m_vkContext->graphicsQueue)
            );
        }
    }

    void RenderManager::Present()
    {
        // Swap chains
        VkSwapchainKHR swapChains[] = {m_vkContext->swapChain};
        // Signal semaphores
        VkSemaphore signalSemaphores[] = {m_vkContext->renderFinishedSemaphores[currentFrame]};

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
        auto result = vkQueuePresentKHR(m_vkContext->graphicsQueue, &presentInfo);

        // Check swapchain
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            // Swap chain is out of date, rebuild
            m_vkContext->RecreateSwapChain(m_window);
        }
        else if (result != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("Failed to acquire swap chain image! [result={:X}]\n", static_cast<int>(result));
        }

        // Set frame index
        currentFrame = (currentFrame + 1) % Vk::MAX_FRAMES_IN_FLIGHT;
    }

    void RenderManager::EndFrame()
    {
        // End render pass
        vkCmdEndRenderPass(m_vkContext->commandBuffers[currentFrame]);

        // Finish recording command buffer
        if (vkEndCommandBuffer(m_vkContext->commandBuffers[currentFrame]) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR
            (
                "Failed to record command buffer! [handle={}]\n",
                reinterpret_cast<void*>(m_vkContext->commandBuffers[currentFrame])
            );
        }

        // Submit queue
        SubmitQueue();
    }

    void RenderManager::WaitForLogicalDevice()
    {
        // Wait
        vkDeviceWaitIdle(m_vkContext->device);
    }
}