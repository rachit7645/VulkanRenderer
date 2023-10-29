#include "RenderManager.h"

#include <limits>

#include "RenderConstants.h"
#include "Util/Log.h"

namespace Renderer
{
    RenderManager::RenderManager(const std::shared_ptr<Engine::Window>& window)
        : m_vkContext(std::make_unique<Vk::Context>(window->handle))
    {
    }

    void RenderManager::Render()
    {
        // Draw triangle
        vkCmdDraw
        (
            m_vkContext->commandBuffer,
            3,
            1,
            0,
            0
        );
    }

    void RenderManager::BeginFrame()
    {
        // Wait for previous frame to be finished (we only have 1 frame in flight rn)
        WaitForFrame();
        // Reset command buffer
        vkResetCommandBuffer(m_vkContext->commandBuffer, 0);

        // Begin info
        VkCommandBufferBeginInfo beginInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .pInheritanceInfo = nullptr
        };

        // Begin recording commands
        if (vkBeginCommandBuffer(m_vkContext->commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR
            (
                "Failed to begin recording commands! [CommandBuffer={}]\n",
                reinterpret_cast<void*>(m_vkContext->commandBuffer)
            );
        }
    }

    void RenderManager::BeginRenderPass()
    {
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

        // Get image index
        AcquireSwapChainImage();

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
            m_vkContext->commandBuffer,
            &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE
        );

        // Bind pipeline
        vkCmdBindPipeline
        (
             m_vkContext->commandBuffer,
             VK_PIPELINE_BIND_POINT_GRAPHICS,
             m_vkContext->pipeline
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
            m_vkContext->commandBuffer,
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
            m_vkContext->commandBuffer,
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
            &m_vkContext->inFlight,
            VK_TRUE,
            std::numeric_limits<u64>::max()
        );

        // Reset fence
        vkResetFences(m_vkContext->device, 1, &m_vkContext->inFlight);
    }

    void RenderManager::AcquireSwapChainImage()
    {
        // Query
        vkAcquireNextImageKHR
        (
            m_vkContext->device,
            m_vkContext->swapChain,
            std::numeric_limits<u64>::max(),
            m_vkContext->imageAvailable,
            VK_NULL_HANDLE,
            &m_imageIndex
        );
    }

    void RenderManager::SubmitQueue()
    {
        // Pipeline stage flags
        VkPipelineStageFlags waitStages = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        // Queue submit info
        VkSubmitInfo submitInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = 1,
            .pWaitSemaphores      = &m_vkContext->imageAvailable,
            .pWaitDstStageMask    = &waitStages,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &m_vkContext->commandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = &m_vkContext->renderFinished
        };

        // Submit queue
        if (vkQueueSubmit(
                m_vkContext->graphicsQueue,
                1,
                &submitInfo,
                m_vkContext->inFlight
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR
            (
                "Failed to submit draw command buffer! [CommandBuffer={}] [Queue={}]\n",
                reinterpret_cast<void*>(m_vkContext->commandBuffer),
                reinterpret_cast<void*>(m_vkContext->graphicsQueue)
            );
        }
    }

    void RenderManager::Present()
    {
        // Presentation info
        VkPresentInfoKHR presentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext              = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &m_vkContext->renderFinished,
            .swapchainCount     = 1,
            .pSwapchains        = &m_vkContext->swapChain,
            .pImageIndices      = &m_imageIndex,
            .pResults           = nullptr
        };

        // Present
        vkQueuePresentKHR(m_vkContext->graphicsQueue, &presentInfo);
    }

    void RenderManager::EndRenderPass()
    {
        // End render pass
        vkCmdEndRenderPass(m_vkContext->commandBuffer);
    }

    void RenderManager::EndFrame()
    {
        // Finish recording command buffer
        if (vkEndCommandBuffer(m_vkContext->commandBuffer) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR
            (
                "Failed to record command buffer! [handle={}]\n",
                reinterpret_cast<void*>(m_vkContext->commandBuffer)
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