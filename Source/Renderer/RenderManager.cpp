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
    RenderManager::RenderManager()
        : m_context(m_window.handle),
          m_swapchain(m_window.size, m_context),
          m_formatHelper(m_context.physicalDevice),
          m_megaSet(m_context.device, m_context.physicalDeviceLimits),
          m_modelManager(m_context, m_formatHelper),
          m_iblMaps(m_context, m_megaSet, m_modelManager.textureManager),
          m_postProcessPass(m_context, m_swapchain, m_megaSet, m_modelManager.textureManager),
          m_forwardPass(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_depthPass(m_context, m_formatHelper, m_framebufferManager),
          m_imGuiPass(m_context, m_swapchain, m_megaSet, m_modelManager.textureManager),
          m_skyboxPass(m_context, m_formatHelper, m_megaSet, m_modelManager.textureManager),
          m_bloomPass(m_context, m_formatHelper, m_framebufferManager, m_megaSet, m_modelManager.textureManager),
          m_shadowPass(m_context, m_formatHelper, m_framebufferManager),
          m_pointShadowPass(m_context, m_formatHelper, m_framebufferManager),
          m_meshBuffer(m_context.device, m_context.allocator),
          m_indirectBuffer(m_context.device, m_context.allocator),
          m_sceneBuffer(m_context.device, m_context.allocator),
          m_lightsBuffer(m_context.device, m_context.allocator)
    {
        m_deletionQueue.PushDeletor([&] ()
        {
            m_lightsBuffer.Destroy(m_context.allocator);
            m_sceneBuffer.Destroy(m_context.allocator);
            m_indirectBuffer.Destroy(m_context.allocator);
            m_meshBuffer.Destroy(m_context.allocator);

            m_pointShadowPass.Destroy(m_context.device, m_context.allocator, m_context.commandPool);
            m_shadowPass.Destroy(m_context.device, m_context.allocator, m_context.commandPool);
            m_bloomPass.Destroy(m_context.device, m_context.commandPool);
            m_skyboxPass.Destroy(m_context.device, m_context.commandPool);
            m_imGuiPass.Destroy(m_context.device, m_context.allocator, m_context.commandPool);
            m_depthPass.Destroy(m_context.device, m_context.commandPool);
            m_forwardPass.Destroy(m_context.device, m_context.commandPool);
            m_postProcessPass.Destroy(m_context.device, m_context.commandPool);

            m_megaSet.Destroy(m_context.device);
            m_framebufferManager.Destroy(m_context.device, m_context.allocator);
            m_modelManager.Destroy(m_context.device, m_context.allocator);

            m_swapchain.Destroy(m_context.device);
            m_context.Destroy();
        });

        m_sun =
        {
            .position       = {-30.0f, -30.0f, -10.0f},
            .color          = {1.0f,   0.956f, 0.898f},
            .intensity      = {2.5f,   2.5f,   2.5f}
        };

        m_pointLights =
        {
            Objects::PointLight
            {
                .position    = {4.0f, 20.0f,  -3.0f},
                .color       = {0.0f, 0.945f,  0.945f},
                .intensity   = {1.0f, 7.0f,    5.0f},
                .attenuation = {1.0f, 0.022f,  0.0019f}
            },
            Objects::PointLight
            {
                .position    = {27.0f, 15.0f, -3.0f},
                .color       = {0.0f,  0.031f, 1.0f},
                .intensity   = {1.0f,  6.0f,   10.0f},
                .attenuation = {1.0f,  0.027f, 0.0028f}
            }
        };

        m_spotLights =
        {
            Objects::SpotLight
            {
                .position    = {22.0f, 10.0f,  6.0f},
                .color       = {1.0f,  0.0f,   0.0f},
                .intensity   = {10.0f, 1.0f,   1.0f},
                .attenuation = {1.0f,  0.007f, 0.0002f},
                .direction   = {-1.0f, 0.0f,  -0.3f},
                .cutOff      = {10.0f, 30.0f}
            },
            Objects::SpotLight
            {
                .position    = {62.0f,  2.0f,  -2.0f},
                .color       = {0.941f, 0.0f,   1.0f},
                .intensity   = {10.0f,  10.0f,  10.0f},
                .attenuation = {1.0f,   0.022f, 0.0019f},
                .direction   = {-1.0f,  0.3f,  -0.1f},
                .cutOff      = {10.0f,  50.0f}
            }
        };

        m_renderObjects.emplace_back
        (
            m_modelManager.AddModel(m_context, m_megaSet, "Sponza/glTF/SponzaC.gltf"),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 1.0f, 1.0f)
        );

        m_renderObjects.emplace_back
        (
            m_modelManager.AddModel(m_context, m_megaSet, "Cottage/CottageC.gltf"),
            glm::vec3(50.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 1.0f, 1.0f)
        );

        m_renderObjects.emplace_back
        (
            m_modelManager.AddModel(m_context, m_megaSet, "EnvTest/glTF-IBL/EnvironmentTestC.gltf"),
            glm::vec3(-50.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 1.0f, 1.0f)
        );

        /*m_renderObjects.emplace_back(
            m_modelManager.AddModel(m_context, m_megaSet, "Bistro/BistroC.gltf"),
            glm::vec3(0.0f, 0.0f, -150.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 1.0f, 1.0f)
        );*/

        // ImGui Yoy
        InitImGui();
        CreateSyncObjects();

        m_framebufferManager.Update(m_context, m_formatHelper, m_megaSet, m_swapchain.extent);
        m_modelManager.Update(m_context);

        m_iblMaps.Generate
        (
            m_context,
            m_formatHelper,
            m_modelManager.geometryBuffer,
            m_megaSet,
            m_modelManager.textureManager
        );

        m_frameCounter.Reset();
    }

    void RenderManager::Render()
    {
        WaitForFences();
        AcquireSwapchainImage();

        // Swapchain is not ok, wait for resize event
        if (!m_isSwapchainOk)
        {
            return;
        }

        BeginFrame();
        Update();

        m_depthPass.Render
        (
            m_currentFIF,
            m_framebufferManager,
            m_modelManager.geometryBuffer,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer
        );

        m_shadowPass.Render
        (
            m_currentFIF,
            m_framebufferManager,
            m_modelManager.geometryBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_camera,
            m_sun
        );

        m_pointShadowPass.Render
        (
            m_currentFIF,
            m_framebufferManager,
            m_modelManager.geometryBuffer,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_pointLights
        );

        m_forwardPass.Render
        (
            m_currentFIF,
            m_framebufferManager,
            m_megaSet,
            m_modelManager.geometryBuffer,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_iblMaps,
            m_modelManager.textureManager,
            m_shadowPass.cascadeBuffer
        );

        m_skyboxPass.Render
        (
            m_currentFIF,
            m_framebufferManager,
            m_modelManager.geometryBuffer,
            m_sceneBuffer,
            m_iblMaps,
            m_modelManager.textureManager,
            m_megaSet
        );

        m_bloomPass.Render
        (
            m_currentFIF,
            m_framebufferManager,
            m_megaSet
        );

        m_postProcessPass.Render
        (
            m_currentFIF,
            m_swapchain,
            m_megaSet,
            m_framebufferManager
        );

        m_imGuiPass.Render
        (
            m_currentFIF,
            m_context,
            m_swapchain,
            m_megaSet
        );

        SubmitQueue();
        EndFrame();
    }

    void RenderManager::Update()
    {
        m_frameCounter.Update();
        m_camera.Update(m_frameCounter.frameDelta);

        Engine::Inputs::Get().ImGuiDisplay();

        m_modelManager.ImGuiDisplay();
        m_framebufferManager.ImGuiDisplay();

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

            if (ImGui::BeginMenu("Render Objects"))
            {
                for (usize i = 0; i < m_renderObjects.size(); ++i)
                {
                    if (ImGui::TreeNode(fmt::format("[{}]", i).c_str()))
                    {
                        auto& renderObject = m_renderObjects[i];

                        ImGui::Text("ModelID: %llu", renderObject.modelID);

                        ImGui::Separator();

                        ImGui::DragFloat3("Position", &renderObject.position[0], 1.0f,                      0.0f, 0.0f, "%.2f");
                        ImGui::DragFloat3("Rotation", &renderObject.rotation[0], glm::radians(1.0f), 0.0f, 0.0f, "%.2f");
                        ImGui::DragFloat3("Scale",    &renderObject.scale[0],    1.0f,                      0.0f, 0.0f, "%.2f");

                        ImGui::TreePop();
                    }

                    ImGui::Separator();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Light"))
            {
                if (ImGui::BeginMenu("Directional"))
                {
                    if (ImGui::TreeNode("Sun"))
                    {
                        ImGui::DragFloat3("Position",  &m_sun.position[0],  1.0f, 0.0f, 0.0f, "%.2f");
                        ImGui::ColorEdit3("Color",     &m_sun.color[0]);
                        ImGui::DragFloat3("Intensity", &m_sun.intensity[0], 0.5f, 0.0f, 0.0f, "%.2f");

                        ImGui::TreePop();
                    }

                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::BeginMenu("Point"))
                {
                    for (usize i = 0; i < m_pointLights.size(); ++i)
                    {
                        if (ImGui::TreeNode(fmt::format("[{}]", i).c_str()))
                        {
                            auto& light = m_pointLights[i];

                            ImGui::DragFloat3("Position",    &light.position[0],    1.0f, 0.0f, 0.0f, "%.2f");
                            ImGui::ColorEdit3("Color",       &light.color[0]);
                            ImGui::DragFloat3("Intensity",   &light.intensity[0],   0.5f, 0.0f, 0.0f, "%.2f");
                            ImGui::DragFloat3("Attenuation", &light.attenuation[0], 1.0f, 0.0f, 0.0f, "%.4f");

                            ImGui::TreePop();
                        }

                        ImGui::Separator();
                    }

                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::BeginMenu("Spot"))
                {
                    for (usize i = 0; i < m_spotLights.size(); ++i)
                    {
                        if (ImGui::TreeNode(fmt::format("[{}]", i).c_str()))
                        {
                            auto& light = m_spotLights[i];

                            ImGui::DragFloat3("Position",    &light.position[0],    1.0f,   0.0f, 0.0f, "%.2f");
                            ImGui::ColorEdit3("Color",       &light.color[0]);
                            ImGui::DragFloat3("Intensity",   &light.intensity[0],   0.5f,   0.0f, 0.0f, "%.2f");
                            ImGui::DragFloat3("Attenuation", &light.attenuation[0], 1.0f,   0.0f, 1.0f, "%.4f");
                            ImGui::DragFloat3("Direction",   &light.direction[0],   0.05f, -1.0f, 1.0f, "%.2f");

                            ImGui::DragFloat2("Cut Off", &light.cutOff[0], glm::radians(1.0f), 0.0f, std::numbers::pi, "%.2f");

                            ImGui::TreePop();
                        }

                        ImGui::Separator();
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        m_sun.shadowMapIndex = m_framebufferManager.GetFramebufferView("ShadowCascadesView").descriptorIndex;

        m_lightsBuffer.WriteDirLights(m_currentFIF, {&m_sun, 1});
        m_lightsBuffer.WritePointLights(m_currentFIF, m_pointLights);
        m_lightsBuffer.WriteSpotLights(m_currentFIF, m_spotLights);

        const Scene scene =
        {
            .projection = Maths::CreateProjectionReverseZ(
                m_camera.FOV,
                static_cast<f32>(m_swapchain.extent.width) /
                static_cast<f32>(m_swapchain.extent.height),
                PLANES.x,
                PLANES.y
            ),
            .view        = m_camera.GetViewMatrix(),
            .cameraPos   = m_camera.position,
            .dirLights   = m_lightsBuffer.dirLightBuffers[m_currentFIF].deviceAddress,
            .pointLights = m_lightsBuffer.pointLightBuffers[m_currentFIF].deviceAddress,
            .spotLights  = m_lightsBuffer.spotLightBuffers[m_currentFIF].deviceAddress
        };

        m_sceneBuffer.WriteScene(m_currentFIF, scene);
        m_meshBuffer.LoadMeshes(m_currentFIF, m_modelManager, m_renderObjects);
        m_indirectBuffer.WriteDrawCalls(m_currentFIF, m_modelManager, m_renderObjects);
    }

    void RenderManager::WaitForFences()
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
    }

    void RenderManager::AcquireSwapchainImage()
    {
        if (m_isSwapchainOk)
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
        }
    }

    void RenderManager::BeginFrame()
    {
        Vk::BeginLabel(m_context.graphicsQueue, "Graphics Queue", {0.1137f, 0.7176f, 0.7490, 1.0f});

        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void RenderManager::EndFrame()
    {
        const auto result = m_swapchain.Present(m_context.graphicsQueue, m_currentFIF);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_isSwapchainOk = false;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            Vk::CheckResult(result, "Failed to present swapchain image to queue!");
        }

        Vk::EndLabel(m_context.graphicsQueue);
    }

    void RenderManager::SubmitQueue()
    {
        const VkSemaphoreSubmitInfo signalSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = m_swapchain.renderFinishedSemaphores[m_currentFIF],
            .value       = 0,
            .stageMask   = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .deviceIndex = 0
        };

        const VkSemaphoreSubmitInfo waitSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = m_swapchain.imageAvailableSemaphores[m_currentFIF],
            .value       = 0,
            .stageMask   = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            .deviceIndex = 0
        };

        const std::array cmdBufferInfos =
        {
            VkCommandBufferSubmitInfo
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = m_depthPass.cmdBuffers[m_currentFIF].handle,
                .deviceMask    = 1
            },
            VkCommandBufferSubmitInfo
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = m_shadowPass.cmdBuffers[m_currentFIF].handle,
                .deviceMask    = 1
            },
            VkCommandBufferSubmitInfo
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = m_pointShadowPass.cmdBuffers[m_currentFIF].handle,
                .deviceMask    = 1
            },
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
                .commandBuffer = m_skyboxPass.cmdBuffers[m_currentFIF].handle,
                .deviceMask    = 1
            },
            VkCommandBufferSubmitInfo
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = m_bloomPass.cmdBuffers[m_currentFIF].handle,
                .deviceMask    = 1
            },
            VkCommandBufferSubmitInfo
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = m_postProcessPass.cmdBuffers[m_currentFIF].handle,
                .deviceMask    = 1
            },
            VkCommandBufferSubmitInfo
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = m_imGuiPass.cmdBuffers[m_currentFIF].handle,
                .deviceMask    = 1
            }
        };

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
            inFlightFences[m_currentFIF]),
            "Failed to submit queue!"
        );
    }

    void RenderManager::Resize()
    {
        Vk::CheckResult(vkDeviceWaitIdle(m_context.device), "Device failed to idle!");

        if (!m_swapchain.IsSurfaceValid(m_window.size, m_context))
        {
            m_isSwapchainOk = false;
            return;
        }

        m_swapchain.RecreateSwapChain(m_context);

        m_framebufferManager.Update(m_context, m_formatHelper, m_megaSet, m_swapchain.extent);
        m_megaSet.Update(m_context.device);

        m_isSwapchainOk = true;

        Render();
    }

    bool RenderManager::HandleEvents()
    {
        auto& inputs = Engine::Inputs::Get();

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

            default:
                break;
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                inputs.SetMousePosition(glm::vec2(event.motion.xrel, event.motion.yrel));
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                inputs.SetMouseScroll(glm::vec2(event.wheel.x, event.wheel.y));
                break;

            case SDL_EVENT_GAMEPAD_ADDED:
                if (inputs.GetGamepad() == nullptr)
                {
                    inputs.FindGamepad();
                }
                break;

            case SDL_EVENT_GAMEPAD_REMOVED:
                if (inputs.GetGamepad() != nullptr && event.gdevice.which == inputs.GetGamepadID())
                {
                    SDL_CloseGamepad(inputs.GetGamepad());
                    inputs.FindGamepad();
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
        m_imGuiPass.SetupBackend(m_context, m_megaSet, m_modelManager.textureManager);

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

        m_deletionQueue.PushDeletor([&] ()
        {
            ImGui::GetIO().BackendRendererUserData = nullptr;

            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
        });
    }

    void RenderManager::CreateSyncObjects()
    {
        const VkFenceCreateInfo fenceInfo =
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

            Vk::SetDebugName(m_context.device, inFlightFences[i], fmt::format("RenderManager/InFlightFence{}", i));
        }

        m_deletionQueue.PushDeletor([&] ()
        {
            for (const auto fence : inFlightFences)
            {
                vkDestroyFence(m_context.device, fence, nullptr);
            }
        });

        Logger::Info("{}\n", "Created synchronisation objects!");
    }

    RenderManager::~RenderManager()
    {
        Vk::CheckResult(vkDeviceWaitIdle(m_context.device), "Device failed to idle!");

        m_deletionQueue.FlushQueue();
    }
}