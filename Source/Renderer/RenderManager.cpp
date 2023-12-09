/*
 *    Copyright 2023 Rachit Khandelwal
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

#include <vulkan/vk_enum_string_helper.h>

#include "RenderConstants.h"
#include "Pipelines/SwapPipeline.h"
#include "Util/Log.h"
#include "Util/Maths.h"
#include "Vulkan/Util.h"

namespace Renderer
{
    // Usings
    using Pipelines::SwapPipeline;

    RenderManager::RenderManager(std::shared_ptr<Engine::Window> window)
        : m_window(std::move(window)),
          m_vkContext(std::make_shared<Vk::Context>(m_window)),
          m_swapchain(std::make_shared<Vk::Swapchain>(m_window, m_vkContext)),
          m_swapPipeline(std::make_unique<Pipelines::SwapPipeline>(m_vkContext, m_swapchain)),
          m_model(std::make_unique<Models::Model>(m_vkContext, "Sponza/sponza.glb"))
    {
        // ImGui init info
        ImGui_ImplVulkan_InitInfo imguiInitInfo =
        {
            .Instance              = m_vkContext->vkInstance,
            .PhysicalDevice        = m_vkContext->physicalDevice,
            .Device                = m_vkContext->device,
            .QueueFamily           = m_vkContext->queueFamilies.graphicsFamily.value_or(0),
            .Queue                 = m_vkContext->graphicsQueue,
            .PipelineCache         = nullptr,
            .DescriptorPool        = m_vkContext->descriptorPool,
            .Subpass               = 0,
            .MinImageCount         = Vk::FRAMES_IN_FLIGHT,
            .ImageCount            = Vk::FRAMES_IN_FLIGHT,
            .MSAASamples           = VK_SAMPLE_COUNT_1_BIT,
            .UseDynamicRendering   = false,
            .ColorAttachmentFormat = {},
            .Allocator             = nullptr,
            .CheckVkResultFn       = &Vk::CheckResult
        };

        // Init ImGui
        Logger::Info("Initializing Dear ImGui version: {}\n", ImGui::GetVersion());
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        // Init ImGui backend
        ImGui_ImplSDL2_InitForVulkan(m_window->handle);
        ImGui_ImplVulkan_Init(&imguiInitInfo, m_swapchain->renderPass.handle);
        // Upload fonts
        ImGui_ImplVulkan_CreateFontsTexture();

        // Bind textures to pipeline
        m_swapPipeline->WriteImageDescriptors(m_vkContext->device, m_model->GetTextureViews());

        // Reset frame counter
        m_frameCounter.Reset();
    }

    void RenderManager::Render()
    {
        // Make sure swap chain is valid
        if (!IsSwapchainValid()) { return; }

        // Begin
        BeginFrame();
        // Update
        Update();

        // Get scene descriptors
        std::array<VkDescriptorSet, 2> sceneDescriptorSets =
        {
            m_swapPipeline->GetSharedUBOData().setMap[m_currentFrame][0],
            m_swapPipeline->GetSamplerData().setMap[m_currentFrame][0],
        };

        // Bind
        m_swapPipeline->pipeline.BindDescriptors
        (
            currentCmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            sceneDescriptorSets
        );

        // Load push constants
        m_swapPipeline->pipeline.LoadPushConstants
        (
            currentCmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(decltype(m_swapPipeline->pushConstants[m_currentFrame])),
            reinterpret_cast<void*>(&m_swapPipeline->pushConstants[m_currentFrame])
        );

        // Loop over meshes
        for (const auto& mesh : m_model->meshes)
        {
            // Bind buffer
            mesh.vertexBuffer.BindBuffer(currentCmdBuffer);

            // Get mesh descriptors
            std::array<VkDescriptorSet, 1> meshDescriptorSets =
            {
                m_swapPipeline->imageViewMap[m_currentFrame][mesh.texture.imageView]
            };

            // Bind
            m_swapPipeline->pipeline.BindDescriptors
            (
                currentCmdBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                2,
                meshDescriptorSets
            );

            // Draw triangle
            vkCmdDrawIndexed
            (
                currentCmdBuffer,
                mesh.vertexBuffer.indexCount,
                1,
                0,
                0,
                0
            );
        }

        // Render
        ImGui::Render();
        // Render ImGui
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), currentCmdBuffer);

        // End
        EndFrame();
        // Present
        Present();
    }

    void RenderManager::Update()
    {
        // Get current push constant
        auto& pushConstant = m_swapPipeline->pushConstants[m_currentFrame];

        // Update frame counter
        m_frameCounter.Update();

        // Data
        static glm::vec3 s_position = {};
        static glm::vec3 s_rotation = {};
        static glm::vec3 s_scale    = {0.5f, 0.5f, 0.5f};

        // Staring time
        static auto startTime = std::chrono::high_resolution_clock::now();
        // Current time
        auto currentTime = std::chrono::high_resolution_clock::now();
        // Duration
        f32 duration = std::chrono::duration<f32, std::chrono::seconds::period>(currentTime - startTime).count();
        // Update rotation
        s_rotation.y = (duration / 5.0f) * glm::radians(90.0f);

        // Render ImGui
        if (ImGui::BeginMainMenuBar())
        {
            // Profiler
            if (ImGui::BeginMenu("Profiler"))
            {
                // Frame stats
                ImGui::Text("FPS: %.2f", m_frameCounter.FPS);
                ImGui::Text("Frame time: %.2f ms", m_frameCounter.avgFrameTime);
                ImGui::EndMenu();
            }
            // Mesh
            if (ImGui::BeginMenu("Mesh"))
            {
                // Transform modifiers
                ImGui::DragFloat3("Position", &s_position[0]);
                ImGui::DragFloat3("Rotation", &s_rotation[0]);
                ImGui::DragFloat3("Scale",    &s_scale[0]);
                // End menu
                ImGui::EndMenu();
            }
            // End menu bar
            ImGui::EndMainMenuBar();
        }

        // Create model matrix
        pushConstant.transform = Maths::CreateModelMatrix<glm::mat4>
        (
            s_position,
            s_rotation,
            s_scale
        );

        // Create normal matrix
        pushConstant.normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(pushConstant.transform))));
    }

    void RenderManager::BeginFrame()
    {
        // Wait for previous frame to be finished
        WaitForFrame();
        // Get current command buffer
        currentCmdBuffer = m_vkContext->commandBuffers[m_currentFrame];

        // Reset command buffer
        vkResetCommandBuffer(currentCmdBuffer, 0);

        // Begin info
        VkCommandBufferBeginInfo beginInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .pInheritanceInfo = nullptr
        };

        // Begin recording commands
        if (vkBeginCommandBuffer(currentCmdBuffer, &beginInfo) != VK_SUCCESS)
        {
            // Log
            Logger::Error
            (
                "Failed to begin recording commands! [CmdBuffer={}]\n",
                reinterpret_cast<void*>(currentCmdBuffer)
            );
        }

        // Reset clear state
        m_swapchain->renderPass.ResetClearValues();
        // Color
        m_swapchain->renderPass.SetClearValue(Renderer::CLEAR_COLOR);
        // Depth + Stencil
        m_swapchain->renderPass.SetClearValue(1.0f, 0);

        // Begin render pass
        m_swapchain->renderPass.BeginRenderPass
        (
            currentCmdBuffer,
            m_swapchain->framebuffers[m_imageIndex],
            {.offset = {0, 0}, .extent = m_swapchain->extent},
            VK_SUBPASS_CONTENTS_INLINE
        );

        // Bind pipeline
        m_swapPipeline->pipeline.Bind(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

        // Viewport
        VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(m_swapchain->extent.width),
            .height   = static_cast<f32>(m_swapchain->extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        // Set viewport
        vkCmdSetViewport
        (
            currentCmdBuffer,
            0,
            1,
            &viewport
        );

        // Scissor
        VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = m_swapchain->extent
        };

        // Set scissor
        vkCmdSetScissor
        (
            currentCmdBuffer,
            0,
            1,
            &scissor
        );

        // Get shared UBO
        auto& sharedUBO = m_swapPipeline->sharedUBOs[m_currentFrame];

        // Shared UBO data
        SwapPipeline::SharedBuffer sharedBuffer =
        {
        // Projection matrix
            .proj = glm::perspective(
                FOV,
                static_cast<f32>(m_swapchain->extent.width) /
                static_cast<f32>(m_swapchain->extent.height),
                PLANES.x,
                PLANES.y
            ),
            // View Matrix
            .view = glm::lookAt(
                glm::vec3(0.0f, 0.0f, 5.0f),
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f)
            )
        };

        // Flip projection
        sharedBuffer.proj[1][1] *= -1;

        // Load UBO data
        std::memcpy(sharedUBO.mappedPtr, &sharedBuffer, sizeof(SwapPipeline::SharedBuffer));

        // Begin imgui
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(m_window->handle);
        ImGui::NewFrame();
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
            m_swapchain->handle,
            std::numeric_limits<u64>::max(),
            m_vkContext->imageAvailableSemaphores[m_currentFrame],
            VK_NULL_HANDLE,
            &m_imageIndex
        );
    }

    void RenderManager::EndFrame()
    {
        // End render pass
        vkCmdEndRenderPass(currentCmdBuffer);

        // Finish recording command buffer
        if (vkEndCommandBuffer(currentCmdBuffer) != VK_SUCCESS)
        {
            // Log
            Logger::Error
            (
                "Failed to record command buffer! [handle={}]\n",
                reinterpret_cast<void*>(currentCmdBuffer)
            );
        }

        // Submit queue
        SubmitQueue();
    }

    void RenderManager::SubmitQueue()
    {
        // Waiting semaphores
        std::array<VkSemaphore, 1> waitSemaphores = {m_vkContext->imageAvailableSemaphores[m_currentFrame]};
        // Signal semaphores
        std::array<VkSemaphore, 1> signalSemaphores = {m_vkContext->renderFinishedSemaphores[m_currentFrame]};
        // Pipeline stage flags
        std::array<VkPipelineStageFlags, 1> waitStages = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        // Queue submit info
        VkSubmitInfo submitInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = static_cast<u32>(waitSemaphores.size()),
            .pWaitSemaphores      = waitSemaphores.data(),
            .pWaitDstStageMask    = waitStages.data(),
            .commandBufferCount   = 1,
            .pCommandBuffers      = &currentCmdBuffer,
            .signalSemaphoreCount = static_cast<u32>(signalSemaphores.size()),
            .pSignalSemaphores    = signalSemaphores.data()
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
            Logger::Error
            (
                "Failed to submit command buffer! [CommandBuffer={}] [Queue={}]\n",
                reinterpret_cast<void*>(currentCmdBuffer),
                reinterpret_cast<void*>(m_vkContext->graphicsQueue)
            );
        }
    }

    void RenderManager::Present()
    {
        // Signal semaphores
        std::array<VkSemaphore, 1> signalSemaphores = {m_vkContext->renderFinishedSemaphores[m_currentFrame]};
        // Swap chains
        std::array<VkSwapchainKHR, 1> swapChains = {m_swapchain->handle};
        // Image indices
        std::array<u32, 1> imageIndices = {m_imageIndex};

        // Presentation info
        VkPresentInfoKHR presentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext              = nullptr,
            .waitSemaphoreCount = static_cast<u32>(signalSemaphores.size()),
            .pWaitSemaphores    = signalSemaphores.data(),
            .swapchainCount     = static_cast<u32>(swapChains.size()),
            .pSwapchains        = swapChains.data(),
            .pImageIndices      = imageIndices.data(),
            .pResults           = nullptr
        };

        // Present
        m_swapchainStatus[1] = vkQueuePresentKHR(m_vkContext->graphicsQueue, &presentInfo);

        // Set frame index
        m_currentFrame = (m_currentFrame + 1) % Vk::FRAMES_IN_FLIGHT;
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
                Logger::Error("[{}] Swap chain validation failed!\n", string_VkResult(status));
            }
        }

        // If we have to recreate
        if (toRecreate)
        {
            // Recreate
            m_swapchain->RecreateSwapChain(m_window, m_vkContext);
            // Reset status
            m_swapchainStatus = {VK_SUCCESS, VK_SUCCESS};
            // Return
            return false;
        }

        // No issues
        return true;
    }

    RenderManager::~RenderManager()
    {
        // Wait
        vkDeviceWaitIdle(m_vkContext->device);
        // Destroy ImGui
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        // Destroy pipeline
        m_swapPipeline->Destroy(m_vkContext->device);
        // Destroy mesh
        m_model->Destroy(m_vkContext->device);
        // Destroy swap chain
        m_swapchain->Destroy(m_vkContext->device);
        // Destroy vulkan context
        m_vkContext->Destroy();
    }
}