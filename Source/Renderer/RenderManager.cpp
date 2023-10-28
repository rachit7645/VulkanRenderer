#include "RenderManager.h"

#include <limits>

#include "RenderConstants.h"
#include "Util/Log.h"

namespace Renderer
{
    RenderManager::RenderManager(const std::shared_ptr<Engine::Window>& window)
        : m_vkContext(std::make_shared<Vk::Context>(window->handle))
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
        auto imageIndex = AcquireSwapChainImage();

        // Render pass begin info
        VkRenderPassBeginInfo renderPassInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext           = nullptr,
            .renderPass      = m_vkContext->renderPass,
            .framebuffer     = m_vkContext->swapChainFrameBuffers[imageIndex],
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

    u32 RenderManager::AcquireSwapChainImage()
    {
        // Image index
        u32 imageIndex = 0;
        // Query
        vkAcquireNextImageKHR
        (
            m_vkContext->device,
            m_vkContext->swapChain,
            std::numeric_limits<u64>::max(),
            m_vkContext->imageAvailable,
            VK_NULL_HANDLE,
            &imageIndex
        );
        // Return
        return imageIndex;
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
    }
}