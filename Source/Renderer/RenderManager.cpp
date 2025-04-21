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
#include "Engine/Inputs.h"
#include "Externals/ImGui.h"
#include "Util/Maths.h"

namespace Renderer
{
    RenderManager::RenderManager(const Engine::Config& config)
        : m_context(m_window.handle),
          m_cmdBufferAllocator(m_context.device, m_context.queueFamilies),
          m_swapchain(m_window.size, m_context, m_cmdBufferAllocator),
          m_timeline(m_context.device),
          m_formatHelper(m_context.physicalDevice),
          m_megaSet(m_context),
          m_modelManager(m_context, m_formatHelper),
          m_postProcessPass(m_context, m_swapchain, m_megaSet, m_modelManager.textureManager),
          m_depthPass(m_context, m_formatHelper, m_framebufferManager),
          m_imGuiPass(m_context, m_swapchain, m_megaSet, m_modelManager.textureManager),
          m_skyboxPass(m_context, m_formatHelper, m_megaSet, m_modelManager.textureManager),
          m_bloomPass(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_pointShadowPass(m_context, m_formatHelper, m_framebufferManager),
          m_spotShadowPass(m_context, m_formatHelper, m_framebufferManager),
          m_gBufferPass(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_lightingPass(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_xegtaoPass(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_shadowRTPass(m_context, m_cmdBufferAllocator, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_taaPass(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_cullingDispatch(m_context),
          m_meshBuffer(m_context.device, m_context.allocator),
          m_indirectBuffer(m_context.device, m_context.allocator),
          m_sceneBuffer(m_context.device, m_context.allocator),
          m_lightsBuffer(m_context.device, m_context.allocator),
          m_scene(config, m_context, m_formatHelper, m_cmdBufferAllocator, m_modelManager, m_megaSet)
    {
        m_deletionQueue.PushDeletor([&] ()
        {
            m_lightsBuffer.Destroy(m_context.allocator);
            m_sceneBuffer.Destroy(m_context.allocator);
            m_indirectBuffer.Destroy(m_context.allocator);
            m_meshBuffer.Destroy(m_context.allocator);

            m_cullingDispatch.Destroy(m_context.device, m_context.allocator);
            m_taaPass.Destroy(m_context.device);
            m_shadowRTPass.Destroy(m_context.device, m_context.allocator);
            m_xegtaoPass.Destroy(m_context.device);
            m_lightingPass.Destroy(m_context.device);
            m_gBufferPass.Destroy(m_context.device);
            m_spotShadowPass.Destroy(m_context.device);
            m_pointShadowPass.Destroy(m_context.device);
            m_bloomPass.Destroy(m_context.device);
            m_skyboxPass.Destroy(m_context.device);
            m_imGuiPass.Destroy(m_context.device, m_context.allocator);
            m_depthPass.Destroy(m_context.device);
            m_postProcessPass.Destroy(m_context.device);

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

        m_framebufferManager.Update(m_context, m_formatHelper, m_cmdBufferAllocator, m_megaSet, m_swapchain.extent);
        m_modelManager.Update(m_context, m_cmdBufferAllocator);

        m_accelerationStructure.BuildBottomLevelAS(m_context, m_cmdBufferAllocator, m_modelManager, m_scene.renderObjects);

        m_megaSet.Update(m_context.device);

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
        Update();

        const auto cmdBuffer = m_cmdBufferAllocator.AllocateCommandBuffer(m_currentFIF, m_context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        m_accelerationStructure.BuildTopLevelAS
        (
            m_currentFIF,
            cmdBuffer,
            m_context.device,
            m_context.allocator,
            m_scene.renderObjects
        );

        m_pointShadowPass.Render
        (
            m_currentFIF,
            cmdBuffer,
            m_framebufferManager,
            m_modelManager.geometryBuffer,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_lightsBuffer,
            m_cullingDispatch
        );

        m_spotShadowPass.Render
        (
            m_currentFIF,
            cmdBuffer,
            m_framebufferManager,
            m_modelManager.geometryBuffer,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_lightsBuffer,
            m_cullingDispatch
        );

        m_depthPass.Render
        (
            m_currentFIF,
            m_frameIndex,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager.geometryBuffer,
            m_sceneBuffer,
            m_meshBuffer,
            m_sceneData,
            m_indirectBuffer,
            m_cullingDispatch
        );

        m_gBufferPass.Render
        (
            m_currentFIF,
            m_frameIndex,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager.geometryBuffer,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer
        );

        m_xegtaoPass.Render
        (
            m_currentFIF,
            m_frameIndex,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_sceneBuffer
        );

        m_shadowRTPass.Render
        (
            m_currentFIF,
            m_frameIndex,
            cmdBuffer,
            m_megaSet,
            m_framebufferManager,
            m_sceneBuffer,
            m_accelerationStructure
        );

        m_lightingPass.Render
        (
            m_currentFIF,
            m_frameIndex,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_sceneBuffer,
            m_scene.iblMaps
        );

        m_skyboxPass.Render
        (
            m_currentFIF,
            m_frameIndex,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_modelManager.geometryBuffer,
            m_sceneBuffer,
            m_scene.iblMaps
        );

        m_taaPass.Render
        (
            m_currentFIF,
            m_frameIndex,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet
        );

        m_bloomPass.Render
        (
            m_currentFIF,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet
        );

        m_postProcessPass.Render
        (
            m_currentFIF,
            cmdBuffer,
            m_framebufferManager,
            m_megaSet,
            m_swapchain
        );

        m_imGuiPass.Render
        (
            m_currentFIF,
            m_context.device,
            m_context.allocator,
            cmdBuffer,
            m_megaSet,
            m_swapchain
        );

        cmdBuffer.EndRecording();

        SubmitQueue();
        EndFrame();
    }

    void RenderManager::Update()
    {
        m_frameCounter.Update();

        m_scene.Update
        (
            m_frameCounter,
            m_window.inputs,
            m_context,
            m_formatHelper,
            m_cmdBufferAllocator,
            m_modelManager,
            m_megaSet
        );

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

        m_sceneData.previousMatrices = m_sceneData.currentMatrices;

        const auto projection = Maths::CreateInfiniteProjectionReverseZ
        (
            m_scene.camera.FOV,
            static_cast<f32>(m_swapchain.extent.width) /
            static_cast<f32>(m_swapchain.extent.height),
            Renderer::NEAR_PLANE
        );

        auto jitter = Renderer::JITTER_SAMPLES[m_frameIndex % JITTER_SAMPLE_COUNT];
        jitter     -= glm::vec2(0.5f);
        jitter     /= glm::vec2(m_swapchain.extent.width, m_swapchain.extent.height);

        auto jitteredProjection = projection;

        jitteredProjection[2][0] += jitter.x;
        jitteredProjection[2][1] += jitter.y;

        const auto view = m_scene.camera.GetViewMatrix();

        m_sceneData.currentMatrices  =
        {
            .projection         = projection,
            .inverseProjection  = glm::inverse(jitteredProjection),
            .jitteredProjection = jitteredProjection,
            .view               = view,
            .inverseView        = glm::inverse(view),
            .normalView         = Maths::CreateNormalMatrix(view)
        };

        m_sceneData.cameraPosition = m_scene.camera.position;
        m_sceneData.nearPlane      = Renderer::NEAR_PLANE;
        m_sceneData.farPlane       = Renderer::FAR_PLANE; // There isn't actually a far plane right now lol

        const auto lightsBuffer = m_lightsBuffer.buffers[m_currentFIF].deviceAddress;

        m_sceneData.commonLight         = lightsBuffer + 0;
        m_sceneData.dirLights           = lightsBuffer + m_lightsBuffer.GetDirLightOffset();
        m_sceneData.pointLights         = lightsBuffer + m_lightsBuffer.GetPointLightOffset();
        m_sceneData.shadowedPointLights = lightsBuffer + m_lightsBuffer.GetShadowedPointLightOffset();
        m_sceneData.spotLights          = lightsBuffer + m_lightsBuffer.GetSpotLightOffset();
        m_sceneData.shadowedSpotLights  = lightsBuffer + m_lightsBuffer.GetShadowedSpotLightOffset();

        m_lightsBuffer.WriteLights(m_currentFIF, m_context.allocator, {&m_scene.sun, 1}, m_scene.pointLights, m_scene.spotLights);
        m_sceneBuffer.WriteScene(m_currentFIF, m_context.allocator, m_sceneData);

        m_meshBuffer.LoadMeshes(m_currentFIF, m_context.allocator, m_modelManager, m_scene.renderObjects);
        m_indirectBuffer.WriteDrawCalls(m_currentFIF, m_context.allocator, m_modelManager, m_scene.renderObjects);
    }

    void RenderManager::WaitForTimeline()
    {
        // Frame indices 0 to Vk::FRAMES_IN_FLIGHT - 1 do not need to wait for anything
        if (m_frameIndex >= Vk::FRAMES_IN_FLIGHT)
        {
            m_timeline.WaitForStage(m_frameIndex - Vk::FRAMES_IN_FLIGHT, Vk::Timeline::TIMELINE_STAGE_RENDER_FINISHED, m_context.device);
        }

        m_currentFIF = (m_currentFIF + 1) % Vk::FRAMES_IN_FLIGHT;
    }

    void RenderManager::AcquireSwapchainImage()
    {
        const auto result = m_swapchain.AcquireSwapChainImage(m_context.device, m_currentFIF);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_isSwapchainOk = false;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            Vk::CheckResult(result, "Failed to acquire swapchain image!");
        }

        m_timeline.AcquireImageToTimeline(m_frameIndex, m_context.graphicsQueue, m_swapchain.imageAvailableSemaphores[m_currentFIF]);
    }

    void RenderManager::BeginFrame()
    {
        Vk::BeginLabel(m_context.graphicsQueue, "Graphics Queue", {0.1137f, 0.7176f, 0.7490, 1.0f});

        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        m_cmdBufferAllocator.ResetPool(m_currentFIF, m_context.device);
    }

    void RenderManager::EndFrame()
    {
        m_timeline.TimelineToRenderFinished(m_frameIndex, m_context.graphicsQueue, m_swapchain.renderFinishedSemaphores[m_swapchain.imageIndex]);

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

        const auto cmdBufferInfos = m_cmdBufferAllocator.GetGraphicsQueueSubmits(m_currentFIF);

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

        m_timeline.WaitForStage(m_frameIndex - 1, Vk::Timeline::TIMELINE_STAGE_RENDER_FINISHED, m_context.device);

        m_swapchain.RecreateSwapChain(m_context, m_cmdBufferAllocator);

        m_framebufferManager.Update(m_context, m_formatHelper, m_cmdBufferAllocator, m_megaSet, m_swapchain.extent);
        m_megaSet.Update(m_context.device);

        m_taaPass.ResetHistory();

        m_isSwapchainOk = true;

        Render();
    }

    bool RenderManager::HandleEvents()
    {
        SDL_Event event;
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
                m_scene.camera.isEnabled = !m_scene.camera.isEnabled;
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

    void RenderManager::InitImGui()
    {
        Logger::Info("Initializing Dear ImGui [version = {}]\n", ImGui::GetVersion());
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForVulkan(m_window.handle);

        auto& io = ImGui::GetIO();

        io.BackendRendererName = "Rachit_DearImGui_Backend_Vulkan";
        io.BackendFlags       |= ImGuiBackendFlags_RendererHasVtxOffset;
        io.ConfigFlags        |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

        // Load font
        {
            // Font data
            u8* pixels = nullptr;
            s32 width  = 0;
            s32 height = 0;

            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

            const auto fontID = m_modelManager.textureManager.AddTexture
            (
                m_megaSet,
                m_context.device,
                m_context.allocator,
                "DearImGuiFont",
                {pixels, width * (height * 4ull)},
                {width, height},
                VK_FORMAT_R8G8B8A8_UNORM
            );

            io.Fonts->SetTexID(static_cast<ImTextureID>(fontID));
        }

        m_deletionQueue.PushDeletor([&] ()
        {
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
        });
    }

    RenderManager::~RenderManager()
    {
        Vk::CheckResult(vkDeviceWaitIdle(m_context.device), "Device failed to idle!");

        m_deletionQueue.FlushQueue();
    }
}