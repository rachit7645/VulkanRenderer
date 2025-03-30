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
#include "Convolution/Pipeline.h"
#include "BRDF/Pipeline.h"
#include "Converter/Pipeline.h"
#include "PreFilter/Pipeline.h"

namespace Renderer::IBL
{
    constexpr glm::uvec2 SKYBOX_SIZE     = {2048, 2048};
    constexpr glm::uvec2 IRRADIANCE_SIZE = {128,  128};
    constexpr glm::uvec2 PRE_FILTER_SIZE = {1024, 1024};
    constexpr glm::uvec2 BRDF_LUT_SIZE   = {1024, 1024};

    constexpr usize PREFILTER_MIPMAP_LEVELS = 5;
    constexpr usize PREFILTER_SAMPLE_COUNT  = 512;

    void IBLMaps::Generate
    (
        Vk::CommandBufferAllocator& cmdBufferAllocator,
        const std::string_view hdrMap,
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        const Vk::GeometryBuffer& geometryBuffer,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
    {
        const auto hdrMapAssetPath = Engine::Files::GetAssetPath("GFX/IBL/", hdrMap);

        if (!Engine::Files::Exists(hdrMapAssetPath))
        {
            return;
        }

        const auto cmdBuffer = cmdBufferAllocator.AllocateGlobalCommandBuffer(context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        const auto matrixBuffer = SetupMatrixBuffer(context);

        Vk::BeginLabel(context.graphicsQueue, "IBLMaps::Generate", {0.9215f, 0.8470f, 0.0274f, 1.0f});

        cmdBuffer.Reset(0);
        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        if (skyboxID.has_value())
        {
            textureManager.DestroyTexture(context.device, context.allocator, skyboxID.value());
            skyboxID = std::nullopt;
        }

        if (irradianceID.has_value())
        {
            textureManager.DestroyTexture(context.device, context.allocator, irradianceID.value());
            irradianceID = std::nullopt;
        }

        if (preFilterID.has_value())
        {
            textureManager.DestroyTexture(context.device, context.allocator, preFilterID.value());
            preFilterID = std::nullopt;
        }

        // HDRi Environment Maps are always flipped for some reason idk why
        stbi_set_flip_vertically_on_load(true);

        const auto hdrMapID = textureManager.AddTexture
        (
            megaSet,
            context.device,
            context.allocator,
            hdrMapAssetPath
        );

        stbi_set_flip_vertically_on_load(false);

        textureManager.Update(cmdBuffer);
        megaSet.Update(context.device);

        CreateCubeMap
        (
            cmdBuffer,
            context,
            formatHelper,
            geometryBuffer,
            matrixBuffer,
            megaSet,
            textureManager,
            hdrMapID
        );

        CreateIrradianceMap
        (
            cmdBuffer,
            context,
            formatHelper,
            geometryBuffer,
            matrixBuffer,
            megaSet,
            textureManager
        );

        CreatePreFilterMap
        (
            cmdBuffer,
            context,
            formatHelper,
            geometryBuffer,
            matrixBuffer,
            megaSet,
            textureManager
        );

        if (!brdfLutID.has_value())
        {
            CreateBRDFLUT
            (
                cmdBuffer,
                context,
                formatHelper,
                megaSet,
                textureManager
            );
        }

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

            cmdBufferAllocator.FreeGlobalCommandBuffer(cmdBuffer);

            textureManager.ClearUploads(context.allocator);
            textureManager.DestroyTexture(context.device, context.allocator, hdrMapID);
        }

        megaSet.Update(context.device);
    }

    Vk::Buffer IBLMaps::SetupMatrixBuffer(const Vk::Context& context)
    {
        const auto projection = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

        const std::array matrices =
        {
            projection * glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            projection * glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            projection * glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
            projection * glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
            projection * glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            projection * glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
        };

        auto matrixBuffer = Vk::Buffer
        (
            context.allocator,
            matrices.size() * sizeof(glm::mat4),
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy(matrixBuffer.allocationInfo.pMappedData, matrices.data(), matrices.size() * sizeof(glm::mat4));

        matrixBuffer.GetDeviceAddress(context.device);

        Vk::SetDebugName(context.device, matrixBuffer.handle, "IBLMaps/MatrixBuffer");

        std::memcpy
        (
            matrixBuffer.allocationInfo.pMappedData,
            matrices.data(),
            matrices.size() * sizeof(glm::mat4)
        );

        if (!(matrixBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                context.allocator,
                matrixBuffer.allocation,
                0,
                matrices.size() * sizeof(glm::mat4)),
                "Failed to flush allocation!"
            );
        }

        m_deletionQueue.PushDeletor([context, matrixBuffer] () mutable
        {
            matrixBuffer.Destroy(context.allocator);
        });

        return matrixBuffer;
    }

    void IBLMaps::CreateCubeMap
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        const Vk::GeometryBuffer& geometryBuffer,
        const Vk::Buffer& matrixBuffer,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager,
        u32 hdrMapID
    )
    {
        auto pipeline = Converter::Pipeline(context, formatHelper, megaSet, textureManager);

        Vk::BeginLabel(cmdBuffer, "Equirectangular To Cubemap Conversion", {0.2588f, 0.5294f, 0.9607f, 1.0f});

        const auto skybox = Vk::Image
        (
            context.allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = formatHelper.colorAttachmentFormatHDR,
                .extent                = {SKYBOX_SIZE.x, SKYBOX_SIZE.y, 1},
                .mipLevels             = static_cast<u32>(std::floor(std::log2(std::max(SKYBOX_SIZE.x, SKYBOX_SIZE.y)))) + 1,
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

        const auto renderView = Vk::ImageView
        (
            context.device,
            skybox,
            VK_IMAGE_VIEW_TYPE_CUBE,
            skybox.format,
            {
                .aspectMask     = skybox.aspect,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = skybox.arrayLayers
            }
        );

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = renderView.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {}
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
            .viewMask             = 0b00111111,
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
            .matrices     = matrixBuffer.deviceAddress,
            .samplerIndex = pipeline.samplerIndex,
            .textureIndex = hdrMapID
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(Converter::PushConstant),
            &pipeline.pushConstant
        );

        // Mega set
        const std::array descriptorSets = {megaSet.descriptorSet};
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

        skyboxID = textureManager.AddTexture(megaSet, context.device, "Skybox", {skybox, skyboxView});
        megaSet.Update(context.device);

        m_deletionQueue.PushDeletor([context, renderView, pipeline] () mutable
        {
            renderView.Destroy(context.device);
            pipeline.Destroy(context.device);
        });
    }

    void IBLMaps::CreateIrradianceMap
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        const Vk::GeometryBuffer& geometryBuffer,
        const Vk::Buffer& matrixBuffer,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
    {
        auto pipeline = Convolution::Pipeline(context, formatHelper, megaSet, textureManager);

        Vk::BeginLabel(cmdBuffer, "Irradiance Map Generation", {0.2988f, 0.2294f, 0.6607f, 1.0f});

        const auto irradiance = Vk::Image
        (
            context.allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = formatHelper.colorAttachmentFormatHDR,
                .extent                = {IRRADIANCE_SIZE.x, IRRADIANCE_SIZE.y, 1},
                .mipLevels             = 1,
                .arrayLayers           = 6,
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

        irradiance.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                irradiance.aspect,
                0,
                1,
                0,
                irradiance.arrayLayers
            }
        );

        const auto irradianceView = Vk::ImageView
        (
            context.device,
            irradiance,
            VK_IMAGE_VIEW_TYPE_CUBE,
            irradiance.format,
            {
                .aspectMask     = irradiance.aspect,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = irradiance.arrayLayers
            }
        );

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = irradianceView.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {}
        };

        const VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {0, 0},
                .extent = {irradiance.width, irradiance.height}
            },
            .layerCount           = 1,
            .viewMask             = 0b00111111,
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
            .width    = static_cast<f32>(irradiance.width),
            .height   = static_cast<f32>(irradiance.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {irradiance.width, irradiance.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        pipeline.pushConstant =
        {
            .positions    = geometryBuffer.cubeBuffer.deviceAddress,
            .matrices     = matrixBuffer.deviceAddress,
            .samplerIndex = pipeline.samplerIndex,
            .envMapIndex  = skyboxID.value()
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(Convolution::PushConstant),
            &pipeline.pushConstant
        );

        // Mega set
        const std::array descriptorSets = {megaSet.descriptorSet};
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

        irradiance.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                irradiance.aspect,
                0,
                1,
                0,
                irradiance.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);

        irradianceID = textureManager.AddTexture(megaSet, context.device, "Irradiance", {irradiance, irradianceView});

        m_deletionQueue.PushDeletor([context, pipeline] () mutable
        {
            pipeline.Destroy(context.device);
        });
    }

    void IBLMaps::CreatePreFilterMap
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        const Vk::GeometryBuffer& geometryBuffer,
        const Vk::Buffer& matrixBuffer,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
    {
        auto pipeline = PreFilter::Pipeline(context, formatHelper, megaSet, textureManager);

        Vk::BeginLabel(cmdBuffer, "PreFilter Map Generation", {0.2928f, 0.4794f, 0.6607f, 1.0f});

        const auto preFilter = Vk::Image
        (
            context.allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = formatHelper.colorAttachmentFormatHDR,
                .extent                = {PRE_FILTER_SIZE.x, PRE_FILTER_SIZE.y, 1},
                .mipLevels             = PREFILTER_MIPMAP_LEVELS,
                .arrayLayers           = 6,
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

        preFilter.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_NONE,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                preFilter.aspect,
                0,
                preFilter.mipLevels,
                0,
                preFilter.arrayLayers
            }
        );

        std::array<Vk::ImageView, PREFILTER_MIPMAP_LEVELS> preFilterViews = {};

        for (usize mip = 0; mip < preFilterViews.size(); ++mip)
        {
            Vk::BeginLabel(cmdBuffer, fmt::format("Mip #{}", mip), {0.5882f, 0.9294f, 0.2117f, 1.0f});

            const auto mipWidth  = static_cast<u32>(preFilter.width  * std::pow(0.5f, mip));
            const auto mipHeight = static_cast<u32>(preFilter.height * std::pow(0.5f, mip));

            const auto roughness = static_cast<f32>(mip) / static_cast<f32>(preFilter.mipLevels - 1);
            const auto sampleCount    = static_cast<u32>(std::floor(std::pow(2, (roughness * std::log2(PREFILTER_SAMPLE_COUNT)))));

            preFilterViews[mip] = Vk::ImageView
            (
                context.device,
                preFilter,
                VK_IMAGE_VIEW_TYPE_CUBE,
                preFilter.format,
                {
                    .aspectMask     = preFilter.aspect,
                    .baseMipLevel   = static_cast<u32>(mip),
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = preFilter.arrayLayers
                }
            );

            const VkRenderingAttachmentInfo colorAttachmentInfo =
            {
                .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext              = nullptr,
                .imageView          = preFilterViews[mip].handle,
                .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .resolveMode        = VK_RESOLVE_MODE_NONE,
                .resolveImageView   = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue         = {}
            };

            const VkRenderingInfo renderInfo =
            {
                .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .pNext                = nullptr,
                .flags                = 0,
                .renderArea           = {
                    .offset = {0, 0},
                    .extent = {mipWidth, mipHeight}
                },
                .layerCount           = 1,
                .viewMask             = 0b00111111,
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
                .width    = static_cast<f32>(mipWidth),
                .height   = static_cast<f32>(mipHeight),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };

            vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

            const VkRect2D scissor =
            {
                .offset = {0, 0},
                .extent = {mipWidth, mipHeight}
            };

            vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

            pipeline.pushConstant =
            {
                .positions    = geometryBuffer.cubeBuffer.deviceAddress,
                .matrices     = matrixBuffer.deviceAddress,
                .samplerIndex = pipeline.samplerIndex,
                .envMapIndex  = skyboxID.value(),
                .roughness    = roughness,
                .sampleCount  = sampleCount
            };

            pipeline.PushConstants
            (
                cmdBuffer,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(PreFilter::PushConstant),
                &pipeline.pushConstant
            );

            // Mega set
            const std::array descriptorSets = {megaSet.descriptorSet};
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

        preFilter.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                preFilter.aspect,
                0,
                preFilter.mipLevels,
                0,
                preFilter.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);

        const auto preFilterView = Vk::ImageView
        (
            context.device,
            preFilter,
            VK_IMAGE_VIEW_TYPE_CUBE,
            preFilter.format,
            {
                .aspectMask     = preFilter.aspect,
                .baseMipLevel   = 0,
                .levelCount     = preFilter.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = preFilter.arrayLayers
            }
        );

        preFilterID = textureManager.AddTexture(megaSet, context.device, "PreFilter", {preFilter, preFilterView});

        m_deletionQueue.PushDeletor([context, preFilterViews, pipeline] () mutable
        {
            for (auto& view : preFilterViews)
            {
                view.Destroy(context.device);
            }

            pipeline.Destroy(context.device);
        });
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
                .format                = formatHelper.rgSFloatFormat,
                .extent                = {BRDF_LUT_SIZE.x, BRDF_LUT_SIZE.y, 1},
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
            .clearValue         = {}
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

        Vk::EndLabel(cmdBuffer);

        brdfLutID = textureManager.AddTexture(megaSet, context.device, "BRDF_LUT", {brdfLut, brdfLutView});

        m_deletionQueue.PushDeletor([context, pipeline] () mutable
        {
            pipeline.Destroy(context.device);
        });
    }
}
