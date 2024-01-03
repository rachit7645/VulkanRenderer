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

#include "ForwardPass.h"
#include "Util/Log.h"
#include "Renderer/RenderConstants.h"
#include "Util/Maths.h"
#include "Vulkan/Util.h"
#include "Externals/ImGui.h"

namespace Renderer::RenderPasses
{
    ForwardPass::ForwardPass(const std::shared_ptr<Vk::Context>& context, VkExtent2D extent)
    {
        for (auto&& cmdBuffer : cmdBuffers)
        {
            cmdBuffer = Vk::CommandBuffer(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        }

        InitData(context, extent);

        Logger::Info("{}\n", "Created forward pass!");
    }

    void ForwardPass::Recreate(const std::shared_ptr<Vk::Context>& context, VkExtent2D extent)
    {
        DestroyData(context);
        InitData(context, extent);

        Logger::Info("{}\n", "Recreated forward pass!");
    }

    void ForwardPass::Render(usize FIF, const Renderer::FreeCamera& camera, const Models::Model& model)
    {
        auto& currentCmdBuffer    = cmdBuffers[FIF];
        auto& currentPushConstant = pipeline.pushConstants[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(0);

        // Transition for rendering
        images[FIF].TransitionLayout
        (
            currentCmdBuffer,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );

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

        vkCmdBeginRendering(currentCmdBuffer.handle, &renderInfo);

        pipeline.Bind(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

        VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(m_renderSize.x),
            .height   = static_cast<f32>(m_renderSize.y),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewport
        (
            currentCmdBuffer.handle,
            0,
            1,
            &viewport
        );

        VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {m_renderSize.x, m_renderSize.y}
        };

        vkCmdSetScissor
        (
            currentCmdBuffer.handle,
            0,
            1,
            &scissor
        );

        auto& sceneUBO = pipeline.sceneUBOs[FIF];

        Pipelines::ForwardPipeline::SceneBuffer sceneBuffer =
        {
            .projection = glm::perspective(
                camera.FOV,
                static_cast<f32>(m_renderSize.x) /
                static_cast<f32>(m_renderSize.y),
                PLANES.x,
                PLANES.y
            ),
            .view = camera.GetViewMatrix(),
            .cameraPos = {camera.position, 1.0f},
            .dirLight = {
                .position  = {-30.0f, -30.0f, -10.0f, 1.0f},
                .color     = {1.0f,   0.956f, 0.898f, 1.0f},
                .intensity = {3.5f,   3.5f,   3.5f,   1.0f}
            }
        };

        // Flip projection since GLM assumes OpenGL conventions
        sceneBuffer.projection[1][1] *= -1;

        std::memcpy(sceneUBO.allocInfo.pMappedData, &sceneBuffer, sizeof(sceneBuffer));

        static glm::vec3 s_position = {};
        static glm::vec3 s_rotation = {};
        static glm::vec3 s_scale    = {0.25f, 0.25f, 0.25f};

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Mesh"))
            {
                // Transform modifiers
                ImGui::DragFloat3("Position", &s_position[0]);
                ImGui::DragFloat3("Rotation", &s_rotation[0]);
                ImGui::DragFloat3("Scale",    &s_scale[0]);
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        currentPushConstant.transform = Maths::CreateTransformationMatrix
        (
            s_position,
            s_rotation,
            s_scale
        );

        currentPushConstant.normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(currentPushConstant.transform))));

        pipeline.LoadPushConstants
        (
            currentCmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(currentPushConstant),
            reinterpret_cast<void*>(&currentPushConstant)
        );

        // Scene specific descriptor sets
        std::array<VkDescriptorSet, 2> sceneDescriptorSets =
        {
            pipeline.GetSceneUBOData().setMap[FIF][0],
            pipeline.GetSamplerData().setMap[FIF][0],
        };

        pipeline.BindDescriptors
        (
            currentCmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            sceneDescriptorSets
        );

        for (const auto& mesh : model.meshes)
        {
            mesh.vertexBuffer.Bind(currentCmdBuffer);

            // Mesh-specific descriptors
            std::array<VkDescriptorSet, 1> meshDescriptorSets = {pipeline.materialMap[FIF][mesh.material]};

            pipeline.BindDescriptors
            (
                currentCmdBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                2,
                meshDescriptorSets
            );

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

        vkCmdEndRendering(currentCmdBuffer.handle);

        // Transition to shader readable layout
        images[FIF].TransitionLayout
        (
            currentCmdBuffer,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        currentCmdBuffer.EndRecording();
    }

    void ForwardPass::InitData(const std::shared_ptr<Vk::Context>& context, VkExtent2D extent)
    {
        m_renderSize = {extent.width, extent.height};

        auto colorFormat = GetColorFormat(context->physicalDevice);
        auto depthFormat = Vk::DepthBuffer::GetDepthFormat(context->physicalDevice);

        pipeline = Pipelines::ForwardPipeline
        (
            context,
            colorFormat,
            depthFormat,
            extent
        );

        depthBuffer = Vk::DepthBuffer(context, extent);

        Vk::ImmediateSubmit(context, [&] (const Vk::CommandBuffer& cmdBuffer)
        {
            for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
            {
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

                // Transition for shader reading (this skips having extra logic for frame 1)
                images[i].TransitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

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
        });
    }

    VkFormat ForwardPass::GetColorFormat(VkPhysicalDevice physicalDevice)
    {
        return Vk::FindSupportedFormat
        (
            physicalDevice,
            std::array<VkFormat, 4>{VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
        );
    }

    void ForwardPass::Destroy(const std::shared_ptr<Vk::Context>& context)
    {
        Logger::Debug("{}\n", "Destroying forward pass!");
        DestroyData(context);
    }

    void ForwardPass::DestroyData(const std::shared_ptr<Vk::Context>& context)
    {
        for (auto&& imageView : imageViews)
        {
            imageView.Destroy(context->device);
        }

        for (auto&& image : images)
        {
            image.Destroy(context->allocator);
        }

        depthBuffer.Destroy(context->device, context->allocator);
        pipeline.Destroy(context);

        images     = {};
        imageViews = {};
    }
}