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
          m_modelManager(m_context, m_formatHelper),
          m_megaSet(m_context.device, m_context.physicalDeviceLimits),
          m_postProcessPass(m_context, m_swapchain, m_megaSet, m_modelManager.textureManager),
          m_forwardPass(m_context, m_formatHelper, m_megaSet, m_modelManager.textureManager, m_swapchain.extent),
          m_depthPass(m_context, m_formatHelper, m_megaSet, m_swapchain.extent),
          m_imGuiPass(m_context, m_swapchain, m_megaSet, m_modelManager.textureManager),
          m_meshBuffer(m_context.device, m_context.allocator),
          m_indirectBuffer(m_context.device, m_context.allocator),
          m_sceneBuffer(m_context.device, m_context.allocator)
    {
        m_deletionQueue.PushDeletor([&] ()
        {
            m_sceneBuffer.Destroy(m_context.allocator);
            m_indirectBuffer.Destroy(m_context.allocator);
            m_meshBuffer.Destroy(m_context.allocator);

            m_imGuiPass.Destroy(m_context.device, m_context.allocator, m_context.commandPool);
            m_depthPass.Destroy(m_context.device, m_context.commandPool);
            m_forwardPass.Destroy(m_context.device, m_context.commandPool);
            m_postProcessPass.Destroy(m_context.device, m_context.commandPool);

            m_megaSet.Destroy(m_context.device);
            m_modelManager.Destroy(m_context.device, m_context.allocator);

            m_swapchain.Destroy(m_context.device);
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

        m_renderObjects.emplace_back(RenderObject(
            m_modelManager.AddModel(m_context, m_megaSet, "Cube/Cube.gltf"),
            glm::vec3(-50.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(5.0f, 5.0f, 5.0f)
        ));

        // ImGui Yoy
        InitImGui();
        CreateSyncObjects();

        m_modelManager.Update(m_context);
        m_megaSet.Update(m_context.device);

        m_postProcessPass.pipeline.WriteColorAttachmentIndex(m_context.device, m_megaSet, m_forwardPass.colorAttachmentView);
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
            m_modelManager.geometryBuffer,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer
        );

        m_forwardPass.Render
        (
            m_currentFIF,
            m_megaSet,
            m_modelManager.geometryBuffer,
            m_sceneBuffer,
            m_meshBuffer,
            m_indirectBuffer,
            m_depthPass.depthBuffer
        );

        m_postProcessPass.Render(m_currentFIF, m_swapchain, m_megaSet);

        m_imGuiPass.Render
        (
            m_currentFIF,
            m_context.device,
            m_context.allocator,
            m_swapchain,
            m_megaSet,
            m_modelManager.textureManager
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

                if (ImGui::BeginCombo("Heap", fmt::format("[{}]", m_heapIndex).c_str()))
                {
                    for (usize i = 0; i < budgets.size(); ++i)
                    {
                        const bool isSelected = (m_heapIndex == i);

                        if (ImGui::Selectable(fmt::format("[{}]", i).c_str(), isSelected))
                        {
                            m_heapIndex = i;
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    ImGui::EndCombo();
                }

                ImGui::Separator();
                ImGui::Text("Used                   | %llu", budgets[m_heapIndex].usage);
                ImGui::Text("Allocated              | %llu", budgets[m_heapIndex].statistics.allocationBytes);
                ImGui::Text("Available              | %llu", budgets[m_heapIndex].budget - budgets[m_heapIndex].usage);
                ImGui::Text("Budget                 | %llu", budgets[m_heapIndex].budget);
                ImGui::Text("Allocation Count       | %u",   budgets[m_heapIndex].statistics.allocationCount);
                ImGui::Text("Block Count            | %u",   budgets[m_heapIndex].statistics.blockCount);

                ImGui::Separator();
                ImGui::Text("Total Used             | %llu", usedBytes);
                ImGui::Text("Total Allocated        | %llu", allocatedBytes);
                ImGui::Text("Total Available        | %llu", budgetBytes - usedBytes);
                ImGui::Text("Total Budget           | %llu", budgetBytes);
                ImGui::Text("Total Allocation Count | %llu", allocationCount);
                ImGui::Text("Total Block Count      | %llu", blockCount);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Render Objects"))
            {
                if (ImGui::BeginCombo("Object", fmt::format("[{}]", m_renderObjectIndex).c_str()))
                {
                    for (usize i = 0; i < m_renderObjects.size(); ++i)
                    {
                        const bool isSelected = (m_renderObjectIndex == i);

                        if (ImGui::Selectable(fmt::format("[{}]", i).c_str(), isSelected))
                        {
                            m_renderObjectIndex = i;
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    ImGui::EndCombo();
                }

                ImGui::Separator();
                // Immutable
                ImGui::Text("ModelID: %llu", m_renderObjects[m_renderObjectIndex].modelID);
                // Mutable
                ImGui::DragFloat3("Position", &m_renderObjects[m_renderObjectIndex].position[0], 1.0f,               0.0f, 0.0f, "%.2f");
                ImGui::DragFloat3("Rotation", &m_renderObjects[m_renderObjectIndex].rotation[0], glm::radians(1.0f), 0.0f, 0.0f, "%.2f");
                ImGui::DragFloat3("Scale",    &m_renderObjects[m_renderObjectIndex].scale[0],    1.0f,               0.0f, 0.0f, "%.2f");

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Light"))
            {
                ImGui::DragFloat3("Position",  &m_sun.position[0],  1.0f, 0.0f, 0.0f, "%.2f");
                ImGui::ColorEdit3("Color",     &m_sun.color[0]);
                ImGui::DragFloat3("Intensity", &m_sun.intensity[0], 1.0f, 0.0f, 0.0f, "%.2f");

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        const Scene scene =
        {
            .projection = Maths::CreateProjectionReverseZ(
                m_camera.FOV,
                static_cast<f32>(m_swapchain.extent.width) /
                static_cast<f32>(m_swapchain.extent.height),
                PLANES.x,
                PLANES.y
            ),
            .view      = m_camera.GetViewMatrix(),
            .cameraPos = {m_camera.position, 1.0f},
            .dirLight  = m_sun
        };

        m_sceneBuffer.LoadScene(m_currentFIF, scene);
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
            .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        };

        const VkSemaphoreSubmitInfo waitSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = m_swapchain.imageAvailableSemaphores[m_currentFIF],
            .value       = 0,
            .stageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
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
                .commandBuffer = m_forwardPass.cmdBuffers[m_currentFIF].handle,
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
        vkDeviceWaitIdle(m_context.device);

        m_swapchain.RecreateSwapChain(m_window.size, m_context);

        m_forwardPass.Recreate(m_context, m_formatHelper, m_swapchain.extent);
        m_depthPass.Recreate(m_context, m_formatHelper, m_swapchain.extent);

        m_postProcessPass.pipeline.WriteColorAttachmentIndex(m_context.device, m_megaSet, m_forwardPass.colorAttachmentView);

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
                if (inputs.GetGamepad() == nullptr && event.gdevice.which == inputs.GetGamepadID())
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

        m_deletionQueue.PushDeletor([&] ()
        {
            ImGui_ImplSDL3_Shutdown();
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