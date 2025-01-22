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

#include "Util/Log.h"
#include "Vulkan/Util.h"
#include "Engine/Inputs.h"
#include "Externals/ImGui.h"

namespace Renderer
{
    RenderManager::RenderManager(const std::shared_ptr<Engine::Window>& window)
        : m_window(window),
          m_context(m_window),
          m_modelManager(m_context),
          m_megaSet(m_context.device, m_context.physicalDeviceLimits),
          m_swapPass(*window, m_context, m_megaSet, m_modelManager.textureManager),
          m_forwardPass(m_context, m_megaSet, m_modelManager.textureManager, m_swapPass.swapchain.extent)
    {
        m_deletionQueue.PushDeletor([&] ()
        {
            m_modelManager.Destroy(m_context.device, m_context.allocator);
            m_megaSet.Destroy(m_context.device);

            m_swapPass.Destroy(m_context.device, m_context.commandPool);
            m_forwardPass.Destroy(m_context.device, m_context.commandPool);

            m_context.Destroy();
        });

        m_renderObjects.emplace_back(RenderObject(
            m_modelManager.AddModel(m_context, m_megaSet, "Sponza/glTF/Sponza.gltf"),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 1.0f, 1.0f)
        ));

        m_renderObjects.emplace_back(RenderObject(
            m_modelManager.AddModel(m_context, m_megaSet, "Cottage/Cottage.gltf"),
            glm::vec3(50.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 1.0f, 1.0f)
        ));

        // ImGui Yoy
        InitImGui();
        CreateSyncObjects();

        m_swapPass.pipeline.WriteColorAttachmentDescriptor(m_context.device, m_megaSet, m_forwardPass.imageView);
        m_frameCounter.Reset();
    }

    void RenderManager::Render()
    {
        if (!m_swapPass.swapchain.IsSwapchainValid())
        {
            // Skip drawing this frame
            Reset();
            return;
        }

        BeginFrame();

        Update();

        m_forwardPass.Render
        (
            m_currentFIF,
            m_megaSet,
            m_modelManager,
            m_camera,
            m_renderObjects
        );

        m_swapPass.Render(m_megaSet, m_currentFIF);

        SubmitQueue();
        m_swapPass.Present(m_context.graphicsQueue, m_currentFIF);

        EndFrame();
    }

    void RenderManager::Update()
    {
        m_frameCounter.Update();
        m_camera.Update(m_frameCounter.frameDelta);

        Engine::Inputs::Get().ImGuiDisplay();
    }

    void RenderManager::BeginFrame()
    {
        m_currentFIF = (m_currentFIF + 1) % Vk::FRAMES_IN_FLIGHT;

        Vk::CheckResult(vkWaitForFences(
            m_context.device,
            1,
            &inFlightFences[m_currentFIF],
            VK_TRUE,
            std::numeric_limits<u64>::max()),
            "Failed to wait for fence!"
        );

        Vk::CheckResult(vkResetFences(
            m_context.device,
            1,
            &inFlightFences[m_currentFIF]),
            "Unable to reset fence!"
        );

        #ifdef ENGINE_DEBUG
        VkDebugUtilsLabelEXT label =
        {
            .sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext      = nullptr,
            .pLabelName = "Graphics Queue",
            .color      = {}
        };

        vkQueueBeginDebugUtilsLabelEXT(m_context.graphicsQueue, &label);
        #endif

        m_swapPass.swapchain.AcquireSwapChainImage(m_context.device, m_currentFIF);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void RenderManager::EndFrame()
    {
        #ifdef ENGINE_DEBUG
        vkQueueEndDebugUtilsLabelEXT(m_context.graphicsQueue);
        #endif
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
            m_context.graphicsQueue,
            1,
            &submitInfo,
            inFlightFences[m_currentFIF]),
            "Failed to submit queue!"
        );
    }

    void RenderManager::Reset()
    {
        vkDeviceWaitIdle(m_context.device);

        m_swapPass.Recreate(*m_window, m_context, m_megaSet, m_modelManager.textureManager);
        m_forwardPass.Recreate(m_context, m_swapPass.swapchain.extent);

        m_swapPass.pipeline.WriteColorAttachmentDescriptor(m_context.device, m_megaSet, m_forwardPass.imageView);
    }

    void RenderManager::InitImGui()
    {
        auto swapchainFormat = m_swapPass.swapchain.imageFormat;

        ImGui_ImplVulkan_InitInfo imguiInitInfo =
        {
            .Instance                    = m_context.instance,
            .PhysicalDevice              = m_context.physicalDevice,
            .Device                      = m_context.device,
            .QueueFamily                 = m_context.queueFamilies.graphicsFamily.value_or(0),
            .Queue                       = m_context.graphicsQueue,
            .DescriptorPool              = VK_NULL_HANDLE,
            .RenderPass                  = VK_NULL_HANDLE,
            .MinImageCount               = std::max(Vk::FRAMES_IN_FLIGHT, 2ULL),
            .ImageCount                  = std::max(Vk::FRAMES_IN_FLIGHT, 2ULL),
            .MSAASamples                 = VK_SAMPLE_COUNT_1_BIT,
            .PipelineCache               = VK_NULL_HANDLE,
            .Subpass                     = 0,
            .DescriptorPoolSize          = (1 << 4) * Vk::FRAMES_IN_FLIGHT,
            .UseDynamicRendering         = true,
            .PipelineRenderingCreateInfo = {
                .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .pNext                   = nullptr,
                .viewMask                = 0,
                .colorAttachmentCount    = 1,
                .pColorAttachmentFormats = &swapchainFormat,
                .depthAttachmentFormat   = VK_FORMAT_UNDEFINED,
                .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
            },
            .Allocator                   = nullptr,
            .CheckVkResultFn             = &Vk::CheckResult,
            .MinAllocationSize           = 0
        };

        Logger::Info("Initializing Dear ImGui [version = {}]\n", ImGui::GetVersion());
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForVulkan(m_window->handle);
        ImGui_ImplVulkan_Init(&imguiInitInfo);

        ImGui_ImplVulkan_CreateFontsTexture();

        m_deletionQueue.PushDeletor([&] ()
        {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplSDL3_Shutdown();
        });
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
                m_context.device,
                &fenceInfo,
                nullptr,
                &inFlightFences[i]),
                "Failed to create in flight fences!"
            );

            #ifdef ENGINE_DEBUG
            auto name = fmt::format("RenderManager/InFlightFence{}", i);

            VkDebugUtilsObjectNameInfoEXT nameInfo =
            {
                .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext        = nullptr,
                .objectType   = VK_OBJECT_TYPE_FENCE,
                .objectHandle = std::bit_cast<u64>(inFlightFences[i]),
                .pObjectName  = name.c_str()
            };

            vkSetDebugUtilsObjectNameEXT(m_context.device, &nameInfo);
            #endif
        }

        m_deletionQueue.PushDeletor([&] ()
        {
            for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
            {
                vkDestroyFence(m_context.device, inFlightFences[i], nullptr);
            }
        });

        Logger::Info("{}\n", "Created synchronisation objects!");
    }

    RenderManager::~RenderManager()
    {
        vkDeviceWaitIdle(m_context.device);

        m_deletionQueue.FlushQueue();
    }
}