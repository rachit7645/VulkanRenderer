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

#include "ForwardPass.h"
#include "Vulkan/Builders/RenderPassBuilder.h"
#include "Util/Log.h"
#include "Vulkan/Builders/SubpassBuilder.h"
#include "Renderer/RenderConstants.h"
#include "Util/Maths.h"
#include "Vulkan/Util.h"

namespace Renderer::RenderPasses
{
    // Color format
    constexpr VkFormat COLOR_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

    ForwardPass::ForwardPass(const std::shared_ptr<Vk::Context>& context, VkExtent2D swapchainExtent)
    {
        // Create render pass
        CreateRenderPass(context->device, context->physicalDevice);
        // Create pipeline
        pipeline = Pipelines::ForwardPipeline(context, renderPass, swapchainExtent);
        // Create command buffers
        for (auto&& cmdBuffer : cmdBuffers)
        {
            cmdBuffer = Vk::CommandBuffer(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        }
        // Init forward pass data
        InitData(context, swapchainExtent);
        // Log
        Logger::Info("{}\n", "Created forward pass!");
    }

    void ForwardPass::Recreate(const std::shared_ptr<Vk::Context>& context, VkExtent2D swapchainExtent)
    {
        // Destroy old data
        DestroyData(context->device);
        // Init forward pass data
        InitData(context, swapchainExtent);
        // Log
        Logger::Info("{}\n", "Recreated forward pass!");
    }

    void ForwardPass::Render(usize FIF, const Renderer::FreeCamera& camera, const Models::Model& model)
    {
        // Get current resources
        auto& currentCmdBuffer    = cmdBuffers[FIF];
        auto& currentFramebuffer  = framebuffers[FIF];
        auto& currentPushConstant = pipeline.pushConstants[FIF];

        // Begin recording
        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(0);

        // Transition to color attachment
        images[FIF].TransitionLayout
        (
            currentCmdBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );

        // Set clear state
        renderPass.ResetClearValues();
        renderPass.SetClearValue(Renderer::CLEAR_COLOR);
        renderPass.SetClearValue(1.0f, 0x0);
        // Begin renderpass
        renderPass.BeginRenderPass
        (
            currentCmdBuffer,
            currentFramebuffer,
            {.offset = {0, 0}, .extent = {currentFramebuffer.size.x, currentFramebuffer.size.y}},
            VK_SUBPASS_CONTENTS_INLINE
        );

        // Bind pipeline
        pipeline.pipeline.Bind(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

        // Viewport
        VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(currentFramebuffer.size.x),
            .height   = static_cast<f32>(currentFramebuffer.size.y),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        // Set viewport
        vkCmdSetViewport
        (
            currentCmdBuffer.handle,
            0,
            1,
            &viewport
        );

        // Scissor
        VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {currentFramebuffer.size.x, currentFramebuffer.size.y}
        };

        // Set scissor
        vkCmdSetScissor
        (
            currentCmdBuffer.handle,
            0,
            1,
            &scissor
        );

        // Get shared UBO
        auto& sceneUBO = pipeline.sceneUBOs[FIF];

        // Shared UBO data
        Pipelines::ForwardPipeline::SceneBuffer sceneBuffer =
        {
            // Projection matrix
            .projection = glm::perspective(
                camera.FOV,
                static_cast<f32>(currentFramebuffer.size.x) /
                static_cast<f32>(currentFramebuffer.size.y),
                PLANES.x,
                PLANES.y
            ),
            // View Matrix
            .view = camera.GetViewMatrix()
        };

        // Flip projection
        sceneBuffer.projection[1][1] *= -1;

        // Load UBO data
        std::memcpy(sceneUBO.mappedPtr, &sceneBuffer, sizeof(sceneBuffer));

        // Data
        static glm::vec3 s_position = {};
        static glm::vec3 s_rotation = {};
        static glm::vec3 s_scale    = {0.25f, 0.25f, 0.25f};

        // Render ImGui
        if (ImGui::BeginMainMenuBar())
        {
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
        currentPushConstant.transform = Maths::CreateModelMatrix<glm::mat4>
        (
            s_position,
            s_rotation,
            s_scale
        );

        // Create normal matrix
        currentPushConstant.normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(currentPushConstant.transform))));

        // Load push constants
        pipeline.pipeline.LoadPushConstants
        (
            currentCmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(currentPushConstant),
            reinterpret_cast<void*>(&currentPushConstant)
        );

        // Get scene descriptors
        std::array<VkDescriptorSet, 2> sceneDescriptorSets =
        {
            pipeline.GetSceneUBOData().setMap[FIF][0],
            pipeline.GetSamplerData().setMap[FIF][0],
        };

        // Bind
        pipeline.pipeline.BindDescriptors
        (
            currentCmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            sceneDescriptorSets
        );

        // Loop over meshes
        for (const auto& mesh : model.meshes)
        {
            // Bind buffer
            mesh.vertexBuffer.BindBuffer(currentCmdBuffer);

            // Get mesh descriptors
            std::array<VkDescriptorSet, 1> meshDescriptorSets =
            {
                pipeline.materialMap[FIF][mesh.material]
            };

            // Bind
            pipeline.pipeline.BindDescriptors
            (
                currentCmdBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                2,
                meshDescriptorSets
            );

            // Draw triangle
            vkCmdDrawIndexed
            (
                currentCmdBuffer.handle,
                mesh.vertexBuffer.indexCount,
                1,
                0,
                0,
                0
            );
        }

        // Render ImGui
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), currentCmdBuffer.handle);

        // End render pass
        renderPass.EndRenderPass(currentCmdBuffer);

        // Transition back
        images[FIF].TransitionLayout
        (
            currentCmdBuffer,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        // End recording
        currentCmdBuffer.EndRecording();
    }

    void ForwardPass::InitData(const std::shared_ptr<Vk::Context>& context, VkExtent2D swapchainExtent)
    {
        // Create framebuffer data
        InitFramebuffers(context, swapchainExtent);
    }

    void ForwardPass::InitFramebuffers(const std::shared_ptr<Vk::Context>& context, VkExtent2D swapchainExtent)
    {
        // Create depth buffer (TODO: it's shared between FIFs, must ask later)
        depthBuffer = Vk::DepthBuffer(context, swapchainExtent);

        // Create framebuffer resources
        for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
        {
            // Create image
            images[i] = Vk::Image
            (
                context,
                swapchainExtent.width,
                swapchainExtent.height,
                COLOR_FORMAT,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );

            // Transition
            Vk::SingleTimeCmdBuffer(context, [&] (const Vk::CommandBuffer& cmdBuffer)
            {
                images[i].TransitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            });

            // Create image view
            imageViews[i] = Vk::ImageView
            (
                context->device,
                images[i],
                VK_IMAGE_VIEW_TYPE_2D,
                images[i].format,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            // Framebuffer attachments
            std::array<Vk::ImageView, 2> attachments = {imageViews[i], depthBuffer.depthImageView};
            // Create framebuffer
            framebuffers[i] = Vk::Framebuffer
            (
                context->device,
                renderPass,
                attachments,
                glm::uvec2(swapchainExtent.width, swapchainExtent.height),
                1
            );
        }
    }

    void ForwardPass::CreateRenderPass(VkDevice device, VkPhysicalDevice physicalDevice)
    {
        // Build
        renderPass = Vk::Builders::RenderPassBuilder::Create(device)
                    .AddAttachment(
                        COLOR_FORMAT,
                        VK_SAMPLE_COUNT_1_BIT,
                        VK_ATTACHMENT_LOAD_OP_CLEAR,
                        VK_ATTACHMENT_STORE_OP_STORE,
                        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                        VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .AddAttachment(
                        depthBuffer.GetDepthFormat(physicalDevice),
                        VK_SAMPLE_COUNT_1_BIT,
                        VK_ATTACHMENT_LOAD_OP_CLEAR,
                        VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                        VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .AddSubpass(
                        Vk::Builders::SubpassBuilder::Create()
                        .AddColorReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                        .AddDepthReference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                        .SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS)
                        .AddDependency(
                            VK_SUBPASS_EXTERNAL,
                            0,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                            0,
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                        .Build())
                    .Build();
    }

    void ForwardPass::Destroy(VkDevice device)
    {
        // Log
        Logger::Debug("{}\n", "Destroying forward pass!");
        // Destroy data
        DestroyData(device);
        // Destroy pipeline
        pipeline.Destroy(device);
        // Destroy renderpass
        renderPass.Destroy(device);
    }

    void ForwardPass::DestroyData(VkDevice device)
    {
        // Destroy image views
        for (auto&& imageView : imageViews)
        {
            imageView.Destroy(device);
        }

        // Destroy images
        for (auto&& image : images)
        {
            image.Destroy(device);
        }

        // Destroy framebuffers
        for (auto&& framebuffer : framebuffers)
        {
            framebuffer.Destroy(device);
        }

        // Destroy depth buffer
        depthBuffer.Destroy(device);

        // Clear
        images.fill({});
        imageViews.fill({});
        framebuffers.fill({});
    }
}