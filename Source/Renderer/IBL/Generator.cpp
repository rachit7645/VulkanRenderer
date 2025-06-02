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

#include "Generator.h"

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"
#include "Externals/GLM.h"
#include "IBL/Converter.h"
#include "IBL/Convolution.h"
#include "IBL/PreFilter.h"

namespace Renderer::IBL
{
    constexpr glm::uvec2 SKYBOX_SIZE     = {2048, 2048};
    constexpr glm::uvec2 IRRADIANCE_SIZE = {128,  128};
    constexpr glm::uvec2 PRE_FILTER_SIZE = {1024, 1024};
    constexpr glm::uvec2 BRDF_LUT_SIZE   = {1024, 1024};

    constexpr u32 PREFILTER_SAMPLE_COUNT  = 512;
    
    Generator::Generator
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : m_converterPipeline(context, formatHelper, megaSet, textureManager),
          m_convolutionPipeline(context, formatHelper, megaSet, textureManager),
          m_preFilterPipeline(context, formatHelper, megaSet, textureManager),
          m_brdfLutPipeline(context, formatHelper)
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

        m_matrixBuffer = Vk::Buffer
        (
            context.allocator,
            matrices.size() * sizeof(glm::mat4),
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy
        (
            m_matrixBuffer.allocationInfo.pMappedData,
            matrices.data(),
            matrices.size() * sizeof(glm::mat4)
        );

        m_matrixBuffer.GetDeviceAddress(context.device);

        Vk::SetDebugName(context.device, m_matrixBuffer.handle, "IBLMaps/MatrixBuffer");

        if (!(m_matrixBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                context.allocator,
                m_matrixBuffer.allocation,
                0,
                matrices.size() * sizeof(glm::mat4)),
                "Failed to flush allocation!"
            );
        }
    }

    IBL::IBLMaps Generator::Generate
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Models::ModelManager& modelManager,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue,
        const std::string_view hdrMapAssetPath
    )
    {
        Vk::BeginLabel(cmdBuffer, "IBL Map Generation", {0.9215f, 0.8470f, 0.0274f, 1.0f});

        const auto hdrMapID = LoadHDRMap
        (
            cmdBuffer,
            context,
            modelManager,
            megaSet,
            deletionQueue,
            hdrMapAssetPath
        );

        const auto skyboxID = GenerateSkybox
        (
            cmdBuffer,
            hdrMapID,
            context,
            formatHelper,
            modelManager,
            megaSet,
            deletionQueue
        );

        modelManager.textureManager.DestroyTexture
        (
            hdrMapID,
            context.device,
            context.allocator,
            megaSet,
            deletionQueue
        );

        megaSet.Update(context.device);

        const auto irradianceMapID = GenerateIrradianceMap
        (
            cmdBuffer,
            skyboxID,
            context,
            formatHelper,
            modelManager,
            megaSet
        );

        const auto preFilterMapID = GeneratePreFilterMap
        (
            cmdBuffer,
            skyboxID,
            context,
            formatHelper,
            modelManager,
            megaSet,
            deletionQueue
        );

        const auto brdfLutID = GenerateBRDFLUT
        (
            cmdBuffer,
            context,
            formatHelper,
            modelManager.textureManager,
            megaSet
        );

        megaSet.Update(context.device);

        Vk::EndLabel(cmdBuffer);

        return IBL::IBLMaps
        {
            .skyboxID        = skyboxID,
            .irradianceMapID = irradianceMapID,
            .preFilterMapID  = preFilterMapID,
            .brdfLutID       = brdfLutID,
        };
    }

    Vk::TextureID Generator::LoadHDRMap
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        Models::ModelManager& modelManager,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue,
        const std::string_view hdrMapAssetPath
    )
    {
        Vk::BeginLabel(cmdBuffer, "Load HDR Map", {0.7215f, 0.8410f, 0.6274f, 1.0f});

        const auto hdrMapID = modelManager.textureManager.AddTexture
        (
            context.allocator,
            deletionQueue,
            Vk::ImageUpload{
                .type   = Vk::ImageUploadType::HDR,
                .flags  = Vk::ImageUploadFlags::Flipped | Vk::ImageUploadFlags::F16,
                .source = Vk::ImageUploadFile{
                    .path = hdrMapAssetPath.data()
                }
            }
        );

        modelManager.Update
        (
            cmdBuffer,
            context.device,
            context.allocator,
            megaSet,
            deletionQueue
        );

        megaSet.Update(context.device);

        Vk::EndLabel(cmdBuffer);

        return hdrMapID;
    }

    Vk::TextureID Generator::GenerateSkybox
    (
        const Vk::CommandBuffer& cmdBuffer,
        Vk::TextureID hdrMapID,
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Models::ModelManager& modelManager,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
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
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = skybox.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = skybox.arrayLayers
            }
        );

        const auto skyboxRenderView = Vk::ImageView
        (
            context.device,
            skybox,
            VK_IMAGE_VIEW_TYPE_CUBE,
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
            .imageView          = skyboxRenderView.handle,
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

        m_converterPipeline.Bind(cmdBuffer);

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

        const auto constants = Converter::Constants
        {
            .Vertices     = modelManager.geometryBuffer.cubeBuffer.deviceAddress,
            .Matrices     = m_matrixBuffer.deviceAddress,
            .SamplerIndex = modelManager.textureManager.GetSampler(m_converterPipeline.samplerID).descriptorID,
            .TextureIndex = modelManager.textureManager.GetTexture(hdrMapID).descriptorID
        };

        m_converterPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            constants
        );

        // Mega set
        const std::array descriptorSets = {megaSet.descriptorSet};
        m_converterPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        vkCmdDraw
        (
            cmdBuffer.handle,
            36,
            1,
            0,
            0
        );

        vkCmdEndRendering(cmdBuffer.handle);

        Vk::BeginLabel(cmdBuffer, "Skybox Mipmap Generation", {0.4588f, 0.1294f, 0.9207f, 1.0f});

        skybox.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = skybox.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = skybox.arrayLayers
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
            {
                .aspectMask     = skybox.aspect,
                .baseMipLevel   = 0,
                .levelCount     = skybox.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = skybox.arrayLayers
            }
        );

        const auto skyboxID = modelManager.textureManager.AddTexture
        (
            megaSet,
            context.device,
            "IBL/Skybox",
            skybox,
            skyboxView
        );

        deletionQueue.PushDeletor([device = context.device, skyboxRenderView] () mutable
        {
            skyboxRenderView.Destroy(device);
        });

        return skyboxID;
    }

    Vk::TextureID Generator::GenerateIrradianceMap
    (
        const Vk::CommandBuffer& cmdBuffer,
        Vk::TextureID skyboxID,
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Models::ModelManager& modelManager,
        Vk::MegaSet& megaSet
    )
    {
        Vk::BeginLabel(cmdBuffer, "Irradiance Map Generation", {0.2988f, 0.2294f, 0.6607f, 1.0f});

        const auto irradianceMap = Vk::Image
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

        irradianceMap.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = irradianceMap.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = irradianceMap.arrayLayers
            }
        );

        const auto irradianceView = Vk::ImageView
        (
            context.device,
            irradianceMap,
            VK_IMAGE_VIEW_TYPE_CUBE,
            {
                .aspectMask     = irradianceMap.aspect,
                .baseMipLevel   = 0,
                .levelCount     = irradianceMap.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = irradianceMap.arrayLayers
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
                .extent = {irradianceMap.width, irradianceMap.height}
            },
            .layerCount           = 1,
            .viewMask             = 0b00111111,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        m_convolutionPipeline.Bind(cmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(irradianceMap.width),
            .height   = static_cast<f32>(irradianceMap.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {irradianceMap.width, irradianceMap.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        const auto constants = Convolution::Constants
        {
            .Vertices     = modelManager.geometryBuffer.cubeBuffer.deviceAddress,
            .Matrices     = m_matrixBuffer.deviceAddress,
            .SamplerIndex = modelManager.textureManager.GetSampler(m_convolutionPipeline.samplerID).descriptorID,
            .EnvMapIndex  = modelManager.textureManager.GetTexture(skyboxID).descriptorID
        };

        m_convolutionPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            constants
        );

        // Mega set
        const std::array descriptorSets = {megaSet.descriptorSet};
        m_convolutionPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        vkCmdDraw
        (
            cmdBuffer.handle,
            36,
            1,
            0,
            0
        );

        vkCmdEndRendering(cmdBuffer.handle);

        irradianceMap.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = irradianceMap.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = irradianceMap.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);

        return modelManager.textureManager.AddTexture
        (
            megaSet,
            context.device,
            "IBL/Irradiance",
            irradianceMap,
            irradianceView
        );
    }

    [[nodiscard]] Vk::TextureID Generator::GeneratePreFilterMap
    (
        const Vk::CommandBuffer& cmdBuffer,
        Vk::TextureID skyboxID,
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Models::ModelManager& modelManager,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        Vk::BeginLabel(cmdBuffer, "PreFilter Map Generation", {0.2928f, 0.4794f, 0.6607f, 1.0f});

        const auto preFilterMap = Vk::Image
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

        preFilterMap.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = preFilterMap.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = preFilterMap.arrayLayers
            }
        );

        std::array<Vk::ImageView, PREFILTER_MIPMAP_LEVELS> preFilterRenderViews = {};

        for (usize mip = 0; mip < preFilterRenderViews.size(); ++mip)
        {
            Vk::BeginLabel(cmdBuffer, fmt::format("Mip #{}", mip), {0.5882f, 0.9294f, 0.2117f, 1.0f});

            const auto mipWidth  = static_cast<u32>(preFilterMap.width  * std::pow(0.5f, mip));
            const auto mipHeight = static_cast<u32>(preFilterMap.height * std::pow(0.5f, mip));

            const auto roughness = static_cast<f32>(mip) / static_cast<f32>(preFilterMap.mipLevels - 1);
            const auto sampleCount    = static_cast<u32>(std::floor(std::pow(2, (roughness * std::log2(PREFILTER_SAMPLE_COUNT)))));

            preFilterRenderViews[mip] = Vk::ImageView
            (
                context.device,
                preFilterMap,
                VK_IMAGE_VIEW_TYPE_CUBE,
                {
                    .aspectMask     = preFilterMap.aspect,
                    .baseMipLevel   = static_cast<u32>(mip),
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = preFilterMap.arrayLayers
                }
            );

            const VkRenderingAttachmentInfo colorAttachmentInfo =
            {
                .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext              = nullptr,
                .imageView          = preFilterRenderViews[mip].handle,
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

            m_preFilterPipeline.Bind(cmdBuffer);

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

            const auto constants = PreFilter::Constants
            {
                .Vertices     = modelManager.geometryBuffer.cubeBuffer.deviceAddress,
                .Matrices     = m_matrixBuffer.deviceAddress,
                .SamplerIndex = modelManager.textureManager.GetSampler(m_preFilterPipeline.samplerID).descriptorID,
                .EnvMapIndex  = modelManager.textureManager.GetTexture(skyboxID).descriptorID ,
                .Roughness    = roughness,
                .SampleCount  = sampleCount
            };

            m_preFilterPipeline.PushConstants
            (
                cmdBuffer,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                constants
            );

            // Mega set
            const std::array descriptorSets = {megaSet.descriptorSet};
            m_preFilterPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

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

        preFilterMap.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = preFilterMap.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = preFilterMap.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);

        const auto preFilterView = Vk::ImageView
        (
            context.device,
            preFilterMap,
            VK_IMAGE_VIEW_TYPE_CUBE,
            {
                .aspectMask     = preFilterMap.aspect,
                .baseMipLevel   = 0,
                .levelCount     = preFilterMap.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = preFilterMap.arrayLayers
            }
        );

        const auto preFilterID = modelManager.textureManager.AddTexture
        (
            megaSet,
            context.device,
            "IBL/PreFilter",
            preFilterMap,
            preFilterView
        );

        deletionQueue.PushDeletor([device = context.device, preFilterRenderViews] () mutable
        {
            for (const auto& view : preFilterRenderViews)
            {
                view.Destroy(device);
            }
        });

        return preFilterID;
    }

    [[nodiscard]] Vk::TextureID Generator::GenerateBRDFLUT
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::TextureManager& textureManager,
        Vk::MegaSet& megaSet
    )
    {
        if (m_brdfLutID.has_value())
        {
            return m_brdfLutID.value();
        }

        Vk::BeginLabel(cmdBuffer, "BRDF LUT Generation", {0.9215f, 0.0274f, 0.8588f, 1.0f});

        const auto brdfLut = Vk::Image
        (
            context.allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = formatHelper.rgSFloat16Format,
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
            {
                .aspectMask     = brdfLut.aspect,
                .baseMipLevel   = 0,
                .levelCount     = brdfLut.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = brdfLut.arrayLayers
            }
        );

        brdfLut.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = brdfLut.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = brdfLut.arrayLayers
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

        m_brdfLutPipeline.Bind(cmdBuffer);

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
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = brdfLut.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = brdfLut.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);

        m_brdfLutID = textureManager.AddTexture
        (
            megaSet,
            context.device,
            "IBL/BRDFLookupTable",
            brdfLut,
            brdfLutView
        );

        return m_brdfLutID.value();
    }

    void Generator::Destroy(VkDevice device, VmaAllocator allocator)
    {
        m_converterPipeline.Destroy(device);
        m_convolutionPipeline.Destroy(device);
        m_preFilterPipeline.Destroy(device);
        m_brdfLutPipeline.Destroy(device);

        m_matrixBuffer.Destroy(allocator);
    }
}