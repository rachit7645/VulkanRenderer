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

#include "Util/Log.h"
#include "Vulkan/Util.h"
#include "Engine/Inputs.h"
#include "Externals/ImGui.h"

namespace Renderer
{
    RenderManager::RenderManager(const std::shared_ptr<Engine::Window>& window)
        : m_window(window),
          m_context(std::make_shared<Vk::Context>(m_window)),
          m_swapPass(window, m_context),
          m_forwardPass(m_context, m_swapPass.swapchain.extent),
          m_model(m_context, "Sponza/sponza.glb")
    {
        // Init ImGui (Yoy)
        InitImGui();
        // Create fences
        CreateSyncObjects();

        // Bind model images to forward pass
        m_forwardPass.pipeline.WriteMaterialDescriptors(m_context->device, m_model.GetMaterials());
        // Bind forward pass color output to swap pipeline
        m_swapPass.pipeline.WriteImageDescriptors(m_context->device, m_forwardPass.imageViews);

        // Reset frame counter
        m_frameCounter.Reset();
    }

    void RenderManager::Render()
    {
        // Make sure swap chain is valid
        if (!m_swapPass.swapchain.IsSwapchainValid())
        {
            // Wait for gpu to finish
            vkDeviceWaitIdle(m_context->device);
            // Recreate resources
            m_swapPass.Recreate(m_window, m_context);
            m_forwardPass.Recreate(m_context, m_swapPass.swapchain.extent);
            // Re-bind descriptors
            m_swapPass.pipeline.WriteImageDescriptors(m_context->device, m_forwardPass.imageViews);
            // Skip drawing this frame
            return;
        }

        // Begin
        BeginFrame();
        // Update
        Update();

        // Render forward pass
        m_forwardPass.Render(m_currentFrame, m_camera, m_model);
        // Render swap pass
        m_swapPass.Render(m_currentFrame);

        // Submit queue
        SubmitQueue();
        // Present
        m_swapPass.Present(m_context, m_currentFrame);
        // End frame
        EndFrame();
    }

    void RenderManager::Update()
    {
        // Update frame counter
        m_frameCounter.Update();
        // Update camera
        m_camera.Update(m_frameCounter.frameDelta);
        // Input display
        Engine::Inputs::GetInstance().ImGuiDisplay();
    }

    void RenderManager::BeginFrame()
    {
        // Update current frame
        m_currentFrame = (m_currentFrame + 1) % Vk::FRAMES_IN_FLIGHT;

        // Wait for frame completion
        vkWaitForFences
        (
            m_context->device,
            1,
            &inFlightFences[m_currentFrame],
            VK_TRUE,
            std::numeric_limits<u64>::max()
        );
        // Get swapchain image
        m_swapPass.swapchain.AcquireSwapChainImage(m_context->device, m_currentFrame);
        // Reset fence
        vkResetFences(m_context->device, 1, &inFlightFences[m_currentFrame]);

        // Begin imgui
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(m_window->handle);
        ImGui::NewFrame();
    }

    void RenderManager::EndFrame()
    {
    }

    void RenderManager::SubmitQueue()
    {
        // Waiting semaphore
        VkSemaphoreSubmitInfo waitSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = m_swapPass.swapchain.imageAvailableSemaphores[m_currentFrame],
            .value       = 0,
            .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        };
        // Command buffer submit information
        std::array<VkCommandBufferSubmitInfo, 2> cmdBufferInfos =
        {
            VkCommandBufferSubmitInfo
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = m_forwardPass.cmdBuffers[m_currentFrame].handle,
                .deviceMask    = 1
            },
            VkCommandBufferSubmitInfo
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = m_swapPass.cmdBuffers[m_currentFrame].handle,
                .deviceMask    = 1
            }
        };
        // Signal semaphore
        VkSemaphoreSubmitInfo signalSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = m_swapPass.swapchain.renderFinishedSemaphores[m_currentFrame],
            .value       = 0,
            .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        };

        // Queue submit info
        VkSubmitInfo2 submitInfo =
        {
            .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext                    = nullptr,
            .flags                    = 0,
            .waitSemaphoreInfoCount   = 1,
            .pWaitSemaphoreInfos      = &waitSemaphoreInfo,
            .commandBufferInfoCount   = static_cast<u32>(cmdBufferInfos.size()),
            .pCommandBufferInfos      = cmdBufferInfos.data(),
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos    = &signalSemaphoreInfo
        };

        // Submit queue
        if (vkQueueSubmit2(
                m_context->graphicsQueue,
                1,
                &submitInfo,
                inFlightFences[m_currentFrame]
            ) != VK_SUCCESS)
        {
            // Log
            Logger::VulkanError("Failed to submit queue! [Queue={}]\n", std::bit_cast<void*>(m_context->graphicsQueue));
        }
    }

    void RenderManager::InitImGui()
    {
        // ImGui init info
        ImGui_ImplVulkan_InitInfo imguiInitInfo =
        {
            .Instance              = m_context->instance,
            .PhysicalDevice        = m_context->physicalDevice,
            .Device                = m_context->device,
            .QueueFamily           = m_context->queueFamilies.graphicsFamily.value_or(0),
            .Queue                 = m_context->graphicsQueue,
            .PipelineCache         = nullptr,
            .DescriptorPool        = m_context->descriptorPool,
            .Subpass               = 0,
            .MinImageCount         = Vk::FRAMES_IN_FLIGHT,
            .ImageCount            = Vk::FRAMES_IN_FLIGHT,
            .MSAASamples           = VK_SAMPLE_COUNT_1_BIT,
            .UseDynamicRendering   = true,
            .ColorAttachmentFormat = m_swapPass.swapchain.imageFormat,
            .Allocator             = nullptr,
            .CheckVkResultFn       = &Vk::CheckResult
        };

        // Init ImGui
        Logger::Info("Initializing Dear ImGui [version = {}]\n", ImGui::GetVersion());
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        // Init ImGui backend
        ImGui_ImplSDL2_InitForVulkan(m_window->handle);
        ImGui_ImplVulkan_Init(&imguiInitInfo, VK_NULL_HANDLE);
        // Upload fonts
        ImGui_ImplVulkan_CreateFontsTexture();
    }

    void RenderManager::CreateSyncObjects()
    {
        // Fence info
        VkFenceCreateInfo fenceInfo =
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        // For each frame in flight
        for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
        {
            if (vkCreateFence(
                    m_context->device,
                    &fenceInfo,
                    nullptr,
                    &inFlightFences[i]
                ) != VK_SUCCESS)
            {
                // Log
                Logger::VulkanError("{}\n", "Failed to create sync objects!");
            }
        }

        // Log
        Logger::Info("{}\n", "Created synchronisation objects!");
    }

    RenderManager::~RenderManager()
    {
        // Wait
        vkDeviceWaitIdle(m_context->device);

        // Destroy ImGui
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();

        // Destroy render passes
        m_swapPass.Destroy(m_context->device, m_context->allocator);
        m_forwardPass.Destroy(m_context->device, m_context->allocator);

        // Destroy mesh
        m_model.Destroy(m_context->device, m_context->allocator);

        // Destroy fences
        for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
        {
            // Destroy fences
            vkDestroyFence(m_context->device, inFlightFences[i], nullptr);
        }

        // Destroy vulkan context
        m_context->Destroy();
    }
}