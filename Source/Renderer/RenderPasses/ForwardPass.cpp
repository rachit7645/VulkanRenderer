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
#include "Util/Log.h"
#include "Renderer/RenderConstants.h"
#include "Util/Maths.h"
#include "Vulkan/Util.h"
#include "Externals/ImGui.h"

namespace Renderer::RenderPasses
{
    ForwardPass::ForwardPass(const std::shared_ptr<Vk::Context>& context, VkExtent2D extent)
        : pipeline(context, GetColorFormat(context->physicalDevice), Vk::DepthBuffer::GetDepthFormat(context->physicalDevice), extent)
    {
        // Create command buffers
        for (auto&& cmdBuffer : cmdBuffers)
        {
            cmdBuffer = Vk::CommandBuffer(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        }
        // Init forward pass data
        InitData(context, extent);
        // Log
        Logger::Info("{}\n", "Created forward pass!");
    }

    void ForwardPass::Recreate(const std::shared_ptr<Vk::Context>& context, VkExtent2D extent)
    {
        // Destroy old data
        DestroyData(context->device, context->allocator);
        // Init forward pass data
        InitData(context, extent);
        // Log
        Logger::Info("{}\n", "Recreated forward pass!");
    }

    void ForwardPass::Render(usize FIF, const Renderer::FreeCamera& camera, const Models::Model& model)
    {
        // Get current resources
        auto& currentCmdBuffer    = cmdBuffers[FIF];
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

        // Color attachment info
        VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = imageViews[FIF].handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {{{
                Renderer::CLEAR_COLOR.r,
                Renderer::CLEAR_COLOR.g,
                Renderer::CLEAR_COLOR.b,
                Renderer::CLEAR_COLOR.a
            }}}
        };

        // Depth attachment info
        VkRenderingAttachmentInfo depthAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = depthBuffer.depthImageView.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp            = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue         = {.depthStencil = {1.0f, 0x0}}
        };

        VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {0, 0},
                .extent = {m_renderSize.x, m_renderSize.y}
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = &depthAttachmentInfo,
            .pStencilAttachment   = nullptr
        };

        // Begin rendering
        vkCmdBeginRendering(currentCmdBuffer.handle, &renderInfo);

        // Bind pipeline
        pipeline.Bind(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

        // Viewport
        VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(m_renderSize.x),
            .height   = static_cast<f32>(m_renderSize.y),
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
            .extent = {m_renderSize.x, m_renderSize.y}
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
                static_cast<f32>(m_renderSize.x) /
                static_cast<f32>(m_renderSize.y),
                PLANES.x,
                PLANES.y
            ),
            // View Matrix
            .view = camera.GetViewMatrix(),
            // Camera position
            .cameraPos = {camera.position, 1.0f},
            // Directional light
            .dirLight = {
                .position  = {-30.0f, -30.0f, -10.0f, 1.0f},
                .color     = {1.0f,   0.956f, 0.898f, 1.0f},
                .intensity = {3.5f,   3.5f,   3.5f,   1.0f}
            }
        };
        // Flip projection
        sceneBuffer.projection[1][1] *= -1;

        // Load UBO data
        std::memcpy(sceneUBO.allocInfo.pMappedData, &sceneBuffer, sizeof(sceneBuffer));

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
        currentPushConstant.transform = Maths::CreateTransformationMatrix
        (
            s_position,
            s_rotation,
            s_scale
        );

        // Create normal matrix
        currentPushConstant.normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(currentPushConstant.transform))));

        // Load push constants
        pipeline.LoadPushConstants
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
        pipeline.BindDescriptors
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
            mesh.vertexBuffer.Bind(currentCmdBuffer);

            // Get mesh descriptors
            std::array<VkDescriptorSet, 1> meshDescriptorSets =
            {
                pipeline.materialMap[FIF][mesh.material]
            };

            // Bind
            pipeline.BindDescriptors
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

        // End render
        vkCmdEndRendering(currentCmdBuffer.handle);

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

    void ForwardPass::InitData(const std::shared_ptr<Vk::Context>& context, VkExtent2D extent)
    {
        // Set render area
        m_renderSize = {extent.width, extent.height};

        // Create depth buffer
        depthBuffer = Vk::DepthBuffer(context, extent);

        // Get color format
        auto colorFormat = GetColorFormat(context->physicalDevice);
        // Log
        Logger::Debug("Using format \"{}\" for color attachment!\n", string_VkFormat(colorFormat));

        // Create framebuffer resources
        for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
        {
            // Create image
            images[i] = Vk::Image
            (
                context,
                m_renderSize.x,
                m_renderSize.y,
                1,
                colorFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );

            // Transition
            Vk::ImmediateSubmit(context, [&](const Vk::CommandBuffer& cmdBuffer)
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
                VK_IMAGE_ASPECT_COLOR_BIT,
                0,
                1,
                0,
                1
            );
        }
    }

    VkFormat ForwardPass::GetColorFormat(VkPhysicalDevice physicalDevice)
    {
        // Return
        return Vk::FindSupportedFormat
        (
            physicalDevice,
            std::array<VkFormat, 4>{VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
        );
    }

    void ForwardPass::Destroy(VkDevice device, VmaAllocator allocator)
    {
        // Log
        Logger::Debug("{}\n", "Destroying forward pass!");
        // Destroy data
        DestroyData(device, allocator);
    }

    void ForwardPass::DestroyData(VkDevice device, VmaAllocator allocator)
    {
        // Destroy image views
        for (auto&& imageView : imageViews)
        {
            imageView.Destroy(device);
        }

        // Destroy images
        for (auto&& image : images)
        {
            image.Destroy(allocator);
        }

        // Destroy depth buffer
        depthBuffer.Destroy(device, allocator);
        // Destroy pipeline
        pipeline.Destroy(device, allocator);

        // Clear
        images.fill({});
        imageViews.fill({});
    }
}