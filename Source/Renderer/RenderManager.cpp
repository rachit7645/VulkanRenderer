/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
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
        // Yoy
        InitImGui();
        CreateSyncObjects();

        m_swapPass.pipeline.WriteImageDescriptors(m_context->device, m_forwardPass.imageViews);
        m_forwardPass.pipeline.WriteMaterialDescriptors(m_context->device, m_model.GetMaterials());

        m_frameCounter.Reset();
    }

    void RenderManager::Render()
    {
        if (!m_swapPass.swapchain.IsSwapchainValid())
        {
            vkDeviceWaitIdle(m_context->device);

            m_swapPass.Recreate(m_window, m_context);
            m_forwardPass.Recreate(m_context, m_swapPass.swapchain.extent);

            m_swapPass.pipeline.WriteImageDescriptors(m_context->device, m_forwardPass.imageViews);
            m_forwardPass.pipeline.WriteMaterialDescriptors(m_context->device, m_model.GetMaterials());

            // Skip drawing this frame
            return;
        }

        BeginFrame();
        Update();

        m_forwardPass.Render(m_currentFIF, m_camera, m_model);
        m_swapPass.Render(m_currentFIF);

        SubmitQueue();
        m_swapPass.Present(m_context, m_currentFIF);

        EndFrame();
    }

    void RenderManager::Update()
    {
        m_frameCounter.Update();
        m_camera.Update(m_frameCounter.frameDelta);
        Engine::Inputs::GetInstance().ImGuiDisplay();
    }

    void RenderManager::BeginFrame()
    {
        m_currentFIF = (m_currentFIF + 1) % Vk::FRAMES_IN_FLIGHT;

        vkWaitForFences
        (
            m_context->device,
            1,
            &inFlightFences[m_currentFIF],
            VK_TRUE,
            std::numeric_limits<u64>::max()
        );
        // Un-signal the fence
        vkResetFences(m_context->device, 1, &inFlightFences[m_currentFIF]);

        m_swapPass.swapchain.AcquireSwapChainImage(m_context->device, m_currentFIF);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(m_window->handle);
        ImGui::NewFrame();
    }

    void RenderManager::EndFrame()
    {
    }

    void RenderManager::SubmitQueue()
    {
        VkSemaphoreSubmitInfo waitSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = m_swapPass.swapchain.imageAvailableSemaphores[m_currentFIF],
            .value       = 0,
            .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        };

        std::array<VkCommandBufferSubmitInfo, 2> cmdBufferInfos =
        {
            VkCommandBufferSubmitInfo
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = m_forwardPass.cmdBuffers[m_currentFIF].handle,
                .deviceMask    = 1
            },
            VkCommandBufferSubmitInfo
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = m_swapPass.cmdBuffers[m_currentFIF].handle,
                .deviceMask    = 1
            }
        };

        VkSemaphoreSubmitInfo signalSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = m_swapPass.swapchain.renderFinishedSemaphores[m_currentFIF],
            .value       = 0,
            .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        };

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

        Vk::CheckResult(vkQueueSubmit2(
            m_context->graphicsQueue,
            1,
            &submitInfo,
            inFlightFences[m_currentFIF]),
            "Failed to submit queue!"
        );
    }

    void RenderManager::InitImGui()
    {
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
            .CheckVkResultFn       = &Vk::CheckResult,
            .MinAllocationSize     = 0
        };

        Logger::Info("Initializing Dear ImGui [version = {}]\n", ImGui::GetVersion());
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplSDL2_InitForVulkan(m_window->handle);
        ImGui_ImplVulkan_Init(&imguiInitInfo, VK_NULL_HANDLE);

        ImGui_ImplVulkan_CreateFontsTexture();
    }

    void RenderManager::CreateSyncObjects()
    {
        VkFenceCreateInfo fenceInfo =
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
        {
            Vk::CheckResult(vkCreateFence(
                m_context->device,
                &fenceInfo,
                nullptr,
                &inFlightFences[i]),
                "Failed to create in flight fences!"
            );
        }

        Logger::Info("{}\n", "Created synchronisation objects!");
    }

    RenderManager::~RenderManager()
    {
        vkDeviceWaitIdle(m_context->device);

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();

        m_swapPass.Destroy(m_context);
        m_forwardPass.Destroy(m_context);

        m_model.Destroy(m_context->device, m_context->allocator);

        for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
        {
            vkDestroyFence(m_context->device, inFlightFences[i], nullptr);
        }

        m_context->Destroy();
    }
}