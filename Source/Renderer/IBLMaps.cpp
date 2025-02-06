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

#include "IBLMaps.h"

#include <stb/stb_image.h>

#include "Util/Log.h"
#include "Renderer/RenderConstants.h"
#include "Vulkan/DebugUtils.h"
#include "BRDF/Pipeline.h"
#include "Converter/Pipeline.h"

namespace Renderer
{
    constexpr auto HDR_MAP = "industrial_sunset_puresky_4k.hdr";

    constexpr VkExtent2D BRDF_LUT_SIZE = {1024, 1024};
    constexpr VkExtent2D SKYBOX_SIZE   = {2048, 2048};

    IBLMaps::IBLMaps
    (
        const Vk::Context& context,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
    {
        stbi_set_flip_vertically_on_load(true);

        hdrMapID = textureManager.AddTexture
        (
            megaSet,
            context.device,
            context.allocator,
            Engine::Files::GetAssetPath("GFX/IBL/", HDR_MAP)
        );

        stbi_set_flip_vertically_on_load(false);
    }

    void IBLMaps::Generate
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        const Vk::GeometryBuffer& geometryBuffer,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
    {
        auto cmdBuffer = Vk::CommandBuffer
        (
            context.device,
            context.commandPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY
        );

        megaSet.Update(context.device);

        Vk::BeginLabel(context.graphicsQueue, "IBLMaps::Generate", {0.9215f, 0.8470f, 0.0274f, 1.0f});

        cmdBuffer.Reset(0);
        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        CreateBRDFLUT
        (
            cmdBuffer,
            context,
            formatHelper,
            megaSet,
            textureManager
        );

        CreateCubeMap
        (
            cmdBuffer,
            context,
            formatHelper,
            geometryBuffer,
            megaSet,
            textureManager
        );

        cmdBuffer.EndRecording();

        VkFence renderFence = VK_NULL_HANDLE;

        // Submit
        {
            const VkFenceCreateInfo fenceCreateInfo =
            {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0
            };

            vkCreateFence(context.device, &fenceCreateInfo, nullptr, &renderFence);

            VkCommandBufferSubmitInfo cmdBufferInfo =
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = cmdBuffer.handle,
                .deviceMask    = 0
            };

            const VkSubmitInfo2 submitInfo =
            {
                .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext                    = nullptr,
                .flags                    = 0,
                .waitSemaphoreInfoCount   = 0,
                .pWaitSemaphoreInfos      = nullptr,
                .commandBufferInfoCount   = 1,
                .pCommandBufferInfos      = &cmdBufferInfo,
                .signalSemaphoreInfoCount = 0,
                .pSignalSemaphoreInfos    = nullptr
            };

            Vk::CheckResult(vkQueueSubmit2(
                context.graphicsQueue,
                1,
                &submitInfo,
                renderFence),
                "Failed to submit IBL command buffer!"
            );

            Vk::CheckResult(vkWaitForFences(
                context.device,
                1,
                &renderFence,
                VK_TRUE,
                std::numeric_limits<u64>::max()),
                "Error while waiting for IBL generation!"
            );
        }

        Vk::EndLabel(context.graphicsQueue);

        // Clean
        {
            vkDestroyFence(context.device, renderFence, nullptr);

            m_deletionQueue.FlushQueue();

            cmdBuffer.Free(context.device, context.commandPool);
        }
    }

    void IBLMaps::CreateBRDFLUT
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
    {
        auto pipeline = BRDF::Pipeline(context, formatHelper);

        Vk::BeginLabel(cmdBuffer, "BRDF LUT Generation", {0.9215f, 0.0274f, 0.8588f, 1.0f});

        const auto brdfLut = Vk::Image
        (
            context.allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = formatHelper.brdfLutFormat,
                .extent                = {BRDF_LUT_SIZE.width, BRDF_LUT_SIZE.height, 1},
                .mipLevels             = 1,
                .arrayLayers           = 1,
                .samples               = VK_SAMPLE_COUNT_1_BIT,
                .tiling                = VK_IMAGE_TILING_OPTIMAL,
                .usage                 = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
                .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
            },
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        const auto brdfLutView = Vk::ImageView
        (
            context.device,
            brdfLut,
            VK_IMAGE_VIEW_TYPE_2D,
            brdfLut.format,
            {
                brdfLut.aspect,
                0,
                brdfLut.mipLevels,
                0,
                1
            }
        );

        brdfLut.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                brdfLut.aspect,
                0,
                brdfLut.mipLevels,
                0,
                1
            }
        );

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = brdfLutView.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {{{
                Renderer::CLEAR_COLOR.r,
                Renderer::CLEAR_COLOR.g,
                Renderer::CLEAR_COLOR.b,
                Renderer::CLEAR_COLOR.a
            }}}
        };

        const VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {0, 0},
                .extent = {brdfLut.width, brdfLut.height}
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        pipeline.Bind(cmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(brdfLut.width),
            .height   = static_cast<f32>(brdfLut.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {brdfLut.width, brdfLut.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        vkCmdDraw
        (
            cmdBuffer.handle,
            3,
            1,
            0,
            0
        );

        vkCmdEndRendering(cmdBuffer.handle);

        brdfLut.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = brdfLut.aspect,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        brdfLutID = textureManager.AddTexture(megaSet, context.device, "BRDF_LUT", {brdfLut, brdfLutView});

        Vk::EndLabel(cmdBuffer);

        m_deletionQueue.PushDeletor([context, pipeline] () mutable
        {
            pipeline.Destroy(context.device);
        });
    }

    void IBLMaps::CreateCubeMap
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        const Vk::GeometryBuffer& geometryBuffer,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
    {
        auto pipeline = Converter::Pipeline(context, formatHelper, megaSet, textureManager);
        megaSet.Update(context.device);

        Vk::BeginLabel(cmdBuffer, "Equirectangular To Cubemap Conversion", {0.2588f, 0.5294f, 0.9607f, 1.0f});

        auto skybox = Vk::Image
        (
            context.allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = formatHelper.textureFormatHDR,
                .extent                = {SKYBOX_SIZE.width, SKYBOX_SIZE.height, 1},
                .mipLevels             = static_cast<u32>(std::floor(std::log2(std::max(SKYBOX_SIZE.width, SKYBOX_SIZE.height)))) + 1,
                .arrayLayers           = 6,
                .samples               = VK_SAMPLE_COUNT_1_BIT,
                .tiling                = VK_IMAGE_TILING_OPTIMAL,
                .usage                 = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
                .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
            },
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        skybox.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                skybox.aspect,
                0,
                skybox.mipLevels,
                0,
                skybox.arrayLayers
            }
        );

        std::array<Vk::ImageView, 6> skyboxViews = {};

        const auto views = GetViewMatrices();

        for (usize i = 0; i < skyboxViews.size(); ++i)
        {
            Vk::BeginLabel(cmdBuffer, fmt::format("Face #{}", i), {0.9882f, 0.7294f, 0.0117f, 1.0f});

            skyboxViews[i] = Vk::ImageView
            (
                context.device,
                skybox,
                VK_IMAGE_VIEW_TYPE_2D,
                skybox.format,
                {
                    .aspectMask     = skybox.aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = static_cast<u32>(i),
                    .layerCount     = 1
                }
            );

            const VkRenderingAttachmentInfo colorAttachmentInfo =
            {
                .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext              = nullptr,
                .imageView          = skyboxViews[i].handle,
                .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .resolveMode        = VK_RESOLVE_MODE_NONE,
                .resolveImageView   = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue         = {{{
                    Renderer::CLEAR_COLOR.r,
                    Renderer::CLEAR_COLOR.g,
                    Renderer::CLEAR_COLOR.b,
                    Renderer::CLEAR_COLOR.a
                }}}
            };

            const VkRenderingInfo renderInfo =
            {
                .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .pNext                = nullptr,
                .flags                = 0,
                .renderArea           = {
                    .offset = {0, 0},
                    .extent = {skybox.width, skybox.height}
                },
                .layerCount           = 1,
                .viewMask             = 0,
                .colorAttachmentCount = 1,
                .pColorAttachments    = &colorAttachmentInfo,
                .pDepthAttachment     = nullptr,
                .pStencilAttachment   = nullptr
            };

            vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

            pipeline.Bind(cmdBuffer);

            const VkViewport viewport =
            {
                .x        = 0.0f,
                .y        = 0.0f,
                .width    = static_cast<f32>(skybox.width),
                .height   = static_cast<f32>(skybox.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };

            vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

            const VkRect2D scissor =
            {
                .offset = {0, 0},
                .extent = {skybox.width, skybox.height}
            };

            vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

            pipeline.pushConstant =
            {
                .positions    = geometryBuffer.cubeBuffer.deviceAddress,
                .projection   = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f),
                .view         = views[i],
                .samplerIndex = pipeline.samplerIndex,
                .textureIndex = textureManager.GetTextureID(hdrMapID)
            };

            pipeline.LoadPushConstants
            (
                cmdBuffer,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(Converter::PushConstant),
                &pipeline.pushConstant
            );

            std::array descriptorSets = {megaSet.descriptorSet.handle};
            pipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

            vkCmdDraw
            (
                cmdBuffer.handle,
                36,
                1,
                0,
                0
            );

            vkCmdEndRendering(cmdBuffer.handle);

            Vk::EndLabel(cmdBuffer);
        }

        Vk::BeginLabel(cmdBuffer, "Mipmap Generation", {0.4588f, 0.1294f, 0.9207f, 1.0f});

        skybox.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            {
                skybox.aspect,
                0,
                skybox.mipLevels,
                0,
                skybox.arrayLayers
            }
        );

        skybox.GenerateMipmaps(cmdBuffer);

        Vk::EndLabel(cmdBuffer);

        Vk::EndLabel(cmdBuffer);

        const auto skyboxView = Vk::ImageView
        (
            context.device,
            skybox,
            VK_IMAGE_VIEW_TYPE_CUBE,
            skybox.format,
            {
                .aspectMask     = skybox.aspect,
                .baseMipLevel   = 0,
                .levelCount     = skybox.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = skybox.arrayLayers
            }
        );

        skyboxID = textureManager.AddCubemap(megaSet, context.device, "Skybox", {skybox, skyboxView});
        megaSet.Update(context.device);

        m_deletionQueue.PushDeletor([context, skyboxViews, pipeline] () mutable
        {
            for (auto& view : skyboxViews)
            {
                view.Destroy(context.device);
            }

            pipeline.Destroy(context.device);
        });
    }

    std::array<glm::mat4, 6> IBLMaps::GetViewMatrices()
    {
        return
        {
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
        };
    }
}
