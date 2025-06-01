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
#include "Vulkan/DebugUtils.h"
#include "Vulkan/ImmediateSubmit.h"
#include "Engine/Inputs.h"
#include "Externals/ImGui.h"
#include "Externals/Tracy.h"

namespace Renderer
{
    RenderManager::RenderManager()
        : m_context(m_window.handle),
          m_cmdBufferAllocator(m_context.device, m_context.queueFamilies),
          m_swapchain(m_window.size, m_context, m_cmdBufferAllocator),
          m_timeline(m_context.device),
          m_formatHelper(m_context.physicalDevice),
          m_megaSet(m_context),
          m_modelManager(m_context.device, m_context.allocator),
          m_postProcess(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_depth(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_imGui(m_context, m_swapchain, m_megaSet, m_modelManager.textureManager),
          m_skybox(m_context, m_formatHelper, m_megaSet, m_modelManager.textureManager),
          m_bloom(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_pointShadow(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_spotShadow(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_gBuffer(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_lighting(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_shadowRT(m_context, m_cmdBufferAllocator, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_taa(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_culling(m_context),
          m_vbgtao(m_context, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_iblGenerator(m_context, m_formatHelper, m_megaSet, m_modelManager.textureManager),
          m_meshBuffer(m_context.device, m_context.allocator),
          m_indirectBuffer(m_context.device, m_context.allocator),
          m_sceneBuffer(m_context.device, m_context.allocator)
    {
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
            m_spotShadow.Destroy(m_context.device);
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

            m_timeline.Destroy(m_context.device);
            m_swapchain.Destroy(m_context.device);
            m_cmdBufferAllocator.Destroy(m_context.device);
            m_context.Destroy();
        });

        // ImGui Yoy
        InitImGui();

        m_frameCounter.Reset();
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

        const auto cmdBuffer = m_cmdBufferAllocator.AllocateCommandBuffer(m_FIF, m_context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

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
            m_timeline,
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
            m_culling
        );

        m_spotShadow.Render
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
            m_indirectBuffer
        );

        m_vbgtao.Execute
        (
            m_FIF,
            m_frameIndex,
            cmdBuffer,
            m_context,
            m_framebufferManager,
            m_sceneBuffer,
            m_megaSet,
            m_modelManager,
            m_deletionQueues[m_FIF]
        );

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
            m_accelerationStructure
        );

        m_lighting.Render
        (
            m_FIF,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager.textureManager,
            m_sceneBuffer,
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
            m_scene->iblMaps
        );

        m_taa.Render
        (
            m_frameIndex,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager.textureManager
        );

        m_bloom.Render
        (
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager.textureManager
        );

        m_postProcess.Render
        (
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager.textureManager
        );

        m_swapchain.Blit(cmdBuffer, m_framebufferManager);

        m_imGui.Render
        (
            m_FIF,
            m_context.device,
            m_context.allocator,
            cmdBuffer,
            m_megaSet,
            m_modelManager.textureManager,
            m_swapchain,
            m_deletionQueues[m_FIF]
        );

        cmdBuffer.EndRecording();

        SubmitQueue();
        EndFrame();
    }

    void RenderManager::WaitForTimeline()
    {
        Vk::BeginLabel(m_context.graphicsQueue, "Graphics Queue", {0.1137f, 0.7176f, 0.7490, 1.0f});

        // Frame indices 0 to Vk::FRAMES_IN_FLIGHT - 1 do not need to wait for anything
        if (m_frameIndex >= Vk::FRAMES_IN_FLIGHT)
        {
            m_timeline.WaitForStage
            (
                m_frameIndex - Vk::FRAMES_IN_FLIGHT,
                Vk::Timeline::TIMELINE_STAGE_RENDER_FINISHED,
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

        m_timeline.AcquireImageToTimeline
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

        m_cmdBufferAllocator.ResetPool(m_FIF, m_context.device);
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
                if (ImGui::BeginMenu("Reload"))
                {
                    if (ImGui::Button("Reload Scene"))
                    {
                        m_config = Engine::Config();

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

                ImGui::Separator();

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

            ImGui::EndMainMenuBar();
        }
    }

    void RenderManager::SubmitQueue()
    {
        const VkSemaphoreSubmitInfo waitSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = m_timeline.semaphore,
            .value       = m_timeline.GetTimelineValue(m_frameIndex, Vk::Timeline::TIMELINE_STAGE_SWAPCHAIN_IMAGE_ACQUIRED),
            .stageMask   = VK_PIPELINE_STAGE_2_NONE,
            .deviceIndex = 0
        };

        const VkSemaphoreSubmitInfo signalSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = m_timeline.semaphore,
            .value       = m_timeline.GetTimelineValue(m_frameIndex, Vk::Timeline::TIMELINE_STAGE_RENDER_FINISHED),
            .stageMask   = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .deviceIndex = 0
        };

        const auto cmdBufferInfos = m_cmdBufferAllocator.GetGraphicsQueueSubmits(m_FIF);

        const VkSubmitInfo2 submitInfo =
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
            VK_NULL_HANDLE),
            "Failed to submit queue!"
        );
    }

    void RenderManager::EndFrame()
    {
        m_timeline.TimelineToRenderFinished
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

        Vk::EndLabel(m_context.graphicsQueue);

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

        Vk::CheckResult(vkWaitForFences(
            m_context.device,
            m_swapchain.presentFences.size(),
            m_swapchain.presentFences.data(),
            VK_TRUE,
            std::numeric_limits<u64>::max()),
            "Failed to wait for fences!"
        );

        m_swapchain.RecreateSwapChain(m_context, m_cmdBufferAllocator);

        m_taa.ResetHistory();

        m_isSwapchainOk = true;

        Render();
    }

    void RenderManager::InitImGui()
    {
        Logger::Debug("Initializing Dear ImGui [Version = {}]\n", ImGui::GetVersion());

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForVulkan(m_window.handle);

        auto& io = ImGui::GetIO();

        io.BackendRendererName = "Rachit_DearImGui_Backend_Vulkan";
        io.BackendFlags       |= ImGuiBackendFlags_RendererHasVtxOffset;
        io.ConfigFlags        |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

        Vk::ImmediateSubmit
        (
            m_context.device,
            m_context.graphicsQueue,
            m_cmdBufferAllocator,
            [&] (const Vk::CommandBuffer& cmdBuffer)
            {
                // Font data
                u8* pixels = nullptr;
                s32 width  = 0;
                s32 height = 0;

                io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

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
                            .data   = pixels
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