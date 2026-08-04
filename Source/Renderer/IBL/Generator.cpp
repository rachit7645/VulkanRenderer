/*
 * Copyright (c) 2023 - 2026 Rachit
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

#include <vulkan/utility/vk_format_utils.h>

#include "Externals/GLM.h"
#include "IBL/BRDF.h"
#include "IBL/EquirectangularToCubemap.h"
#include "IBL/Irradiance.h"
#include "IBL/PreFilter.h"
#include "Util/Log.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::IBL
{
    constexpr glm::uvec2 SKYBOX_SIZE     = {1024, 1024};
    constexpr glm::uvec2 IRRADIANCE_SIZE = {128,  128};
    constexpr glm::uvec2 PRE_FILTER_SIZE = {1024, 1024};

    constexpr u32 PREFILTER_SAMPLE_COUNT = 512;

    constexpr auto BRDF_LUT_CACHE_FILE = "BRDF.cache";

    constexpr u64 GetBRDFLookupTableHash()
    {
        u64 hash = 0;

        hash = Util::HashCombine(hash, BRDF_LUT_SIZE.x);
        hash = Util::HashCombine(hash, BRDF_LUT_SIZE.y);
        hash = Util::HashCombine(hash, BRDF_LUT_FORMAT);
        hash = Util::HashCombine(hash, BRDF::BRDF_LUT_SAMPLE_COUNT);

        return hash;
    }
    
    Generator::Generator
    (
        VkDevice device,
        VmaAllocator allocator,
        const Vk::FormatHelper& formatHelper,
        const Vk::MegaSet& megaSet,
        Vk::PipelineManager& pipelineManager
    )
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        const std::array colorFormats = {formatHelper.colorAttachmentFormatHDR};

        pipelineManager.AddPipeline(Vk::PipelineID::IBLEquirectangularToCubemap, Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0b00111111, colorFormats, VK_FORMAT_UNDEFINED)
            .AttachShader("IBL/EquirectangularToCubemap.vert", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("IBL/EquirectangularToCubemap.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .AddDefaultBlendAttachment()
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(EquirectangularToCubemap::Constants))
            .AddDescriptorLayout(megaSet.layout)
        );

        pipelineManager.AddPipeline(Vk::PipelineID::IBLIrradiance, Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0b00111111, colorFormats, VK_FORMAT_UNDEFINED)
            .AttachShader("IBL/Irradiance.vert", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("IBL/Irradiance.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .AddDefaultBlendAttachment()
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Irradiance::Constants))
            .AddDescriptorLayout(megaSet.layout)
        );

        pipelineManager.AddPipeline(Vk::PipelineID::IBLPreFilter, Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0b00111111, colorFormats, VK_FORMAT_UNDEFINED)
            .AttachShader("IBL/PreFilter.vert", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("IBL/PreFilter.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .AddDefaultBlendAttachment()
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PreFilter::Constants))
            .AddDescriptorLayout(megaSet.layout)
        );

        constexpr std::array BRDF_COLOR_FORMATS = {VK_FORMAT_R16G16_SFLOAT};

        pipelineManager.AddPipeline(Vk::PipelineID::IBLBRDF, Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, BRDF_COLOR_FORMATS, VK_FORMAT_UNDEFINED)
            .AttachShader("Misc/Triangle.vert", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("IBL/BRDF.frag",      VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .AddDefaultBlendAttachment()
        );

        const auto projection = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

        const std::array matrices =
        {
            projection * glm::lookAtRH(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            projection * glm::lookAtRH(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            projection * glm::lookAtRH(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
            projection * glm::lookAtRH(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
            projection * glm::lookAtRH(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            projection * glm::lookAtRH(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
        };

        m_matrixBuffer = Vk::Buffer
        (
            device,
            allocator,
            matrices.size() * sizeof(glm::mat4),
            0,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy
        (
            m_matrixBuffer.hostAddress,
            matrices.data(),
            matrices.size() * sizeof(glm::mat4)
        );

        Vk::SetDebugName(device, m_matrixBuffer.handle, "IBLMaps/MatrixBuffer");

        if ((m_matrixBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0u)
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                m_matrixBuffer.allocation,
                0,
                matrices.size() * sizeof(glm::mat4)),
                "Failed to flush allocation!"
            );
        }
    }

    void Generator::Update
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::Context& context,
        const Objects::Samplers& samplers,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Vk::MegaSet& megaSet,
        Vk::StagingPool& stagingPool,
        Vk::ImageDownloader& imageDownloader,
        tf::Executor& executor,
        Scratch::Allocator& scratchAllocator,
        Engine::DeletionQueue& deletionQueue
    )
    {
        if (m_mapsToDestroy.has_value())
        {
            m_mapsToDestroy->Destroy
            (
                context.device,
                context.allocator,
                textureManager,
                megaSet,
                deletionQueue
            );
        }

        if (!m_pathToLoad.has_value())
        {
            return;
        }

        if (m_loadedIBLMaps.has_value())
        {
            const IBL::IBLID id = std::hash<std::string>{}(*m_pathToLoad);

            if (id != m_loadedIBLMaps->id)
            {
                m_loadedIBLMaps->iblMaps.Destroy
                (
                    context.device,
                    context.allocator,
                    textureManager,
                    megaSet,
                    deletionQueue
                );

                m_loadedIBLMaps = std::nullopt;
            }
        }

        if (!m_loadedIBLMaps.has_value())
        {
            m_loadedIBLMaps = Generator::LoadedIBLMaps
            {
                .id      = std::hash<std::string>{}(*m_pathToLoad),
                .iblMaps = LoadIBLMaps(
                    cmdBuffer,
                    pipelineManager,
                    context,
                    samplers,
                    geometryBuffer,
                    textureManager,
                    megaSet,
                    stagingPool,
                    imageDownloader,
                    executor,
                    scratchAllocator,
                    deletionQueue
                )
            };
        }

        m_pathToLoad = std::nullopt;
    }

    IBL::IBLID Generator::GenerateIBL(const std::string_view hdrMapAssetPath)
    {
        m_pathToLoad = hdrMapAssetPath.data();

        return std::hash<std::string>{}(*m_pathToLoad);
    }

    void Generator::DestroyIBL(IBL::IBLID id)
    {
        if (!m_loadedIBLMaps.has_value())
        {
            Logger::Error("{}\n", "No loaded IBL maps!");
        }

        if (id != m_loadedIBLMaps->id)
        {
            Logger::Error("{}\n", "Invalid IBL Map ID!");
        }

        m_mapsToDestroy = m_loadedIBLMaps->iblMaps;

        m_loadedIBLMaps = std::nullopt;
    }

    IBL::IBLMaps Generator::GetIBLMaps(IBL::IBLID id)
    {
        if (!m_loadedIBLMaps.has_value())
        {
            Logger::Error("{}\n", "No loaded IBL maps!");
        }

        if (id != m_loadedIBLMaps->id)
        {
            Logger::Error("{}\n", "Invalid IBL Map ID!");
        }

        return m_loadedIBLMaps->iblMaps;
    }

    IBL::IBLMaps Generator::LoadIBLMaps
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::Context& context,
        const Objects::Samplers& samplers,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Vk::MegaSet& megaSet,
        Vk::StagingPool& stagingPool,
        Vk::ImageDownloader& imageDownloader,
        tf::Executor& executor,
        Scratch::Allocator& scratchAllocator,
        Engine::DeletionQueue& deletionQueue
    )
    {
        Vk::BeginLabel(cmdBuffer, "IBL Map Generation", {0.9215f, 0.8470f, 0.0274f, 1.0f});

        const Vk::TextureID hdrMapID = LoadHDRMap
        (
            cmdBuffer,
            context,
            geometryBuffer,
            textureManager,
            megaSet,
            stagingPool,
            executor,
            scratchAllocator,
            deletionQueue
        );

        Vk::TextureID skyboxID = GenerateSkybox
        (
            cmdBuffer,
            pipelineManager,
            context,
            samplers,
            geometryBuffer,
            textureManager,
            megaSet,
            hdrMapID,
            scratchAllocator,
            deletionQueue
        );

        textureManager.DestroyTexture
        (
            hdrMapID,
            context.device,
            context.allocator,
            megaSet,
            deletionQueue
        );

        const Vk::TextureID irradianceMapID = GenerateIrradianceMap
        (
            cmdBuffer,
            pipelineManager,
            context,
            samplers,
            geometryBuffer,
            textureManager,
            megaSet,
            skyboxID
        );

        const Vk::TextureID preFilterMapID = GeneratePreFilterMap
        (
            cmdBuffer,
            pipelineManager,
            context,
            samplers,
            geometryBuffer,
            textureManager,
            megaSet,
            skyboxID,
            deletionQueue
        );

        skyboxID = ShrinkSkybox
        (
            cmdBuffer,
            context,
            textureManager,
            megaSet,
            skyboxID,
            scratchAllocator,
            deletionQueue
        );

        const Vk::TextureID brdfLutID = GenerateBRDFLookupTable
        (
            cmdBuffer,
            pipelineManager,
            context,
            textureManager,
            stagingPool,
            megaSet,
            imageDownloader,
            executor,
            deletionQueue
        );

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
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Vk::MegaSet& megaSet,
        Vk::StagingPool& stagingPool,
        tf::Executor& executor,
        Scratch::Allocator& scratchAllocator,
        Engine::DeletionQueue& deletionQueue
    )
    {
        Vk::BeginLabel(cmdBuffer, "Load HDR Map", {0.7215f, 0.8410f, 0.6274f, 1.0f});

        const auto hdrMapID = textureManager.LoadTexture
        (
            context.device,
            context.allocator,
            stagingPool,
            executor,
            deletionQueue,
            Vk::ImageUpload{
                .type   = Vk::FileToImageUploadType(*m_pathToLoad),
                .flags  = Vk::ImageUploadFlags::F16,
                .source = Vk::ImageUploadFile{
                    .path = *m_pathToLoad
                }
            }
        );

        // TODO: Add a way to only load ze cube
        geometryBuffer.Update
        (
            cmdBuffer,
            context.device,
            context.allocator,
            stagingPool,
            scratchAllocator,
            deletionQueue
        );

        textureManager.ForceUpdate
        (
            hdrMapID,
            context.device,
            cmdBuffer,
            megaSet,
            scratchAllocator
        );

        Vk::EndLabel(cmdBuffer);

        return hdrMapID;
    }

    Vk::TextureID Generator::GenerateSkybox
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::Context& context,
        const Objects::Samplers& samplers,
        const Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Vk::MegaSet& megaSet,
        Vk::TextureID hdrMapID,
        Scratch::Allocator& scratchAllocator,
        Engine::DeletionQueue& deletionQueue
    )
    {
        Vk::BeginLabel(cmdBuffer, "Equirectangular To Cubemap Conversion", {0.2588f, 0.5294f, 0.9607f, 1.0f});

        const auto& pipeline = pipelineManager.GetPipeline(Vk::PipelineID::IBLEquirectangularToCubemap);

        const auto skybox = Vk::Image
        (
            context.allocator,
            VkImageCreateInfo{
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = context.formatHelper.colorAttachmentFormatHDR,
                .extent                = {.width = SKYBOX_SIZE.x, .height = SKYBOX_SIZE.y, .depth = 1},
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
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
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
                .offset = {.x = 0, .y = 0},
                .extent = {.width = skybox.width, .height = skybox.height}
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
            .offset = {.x = 0, .y = 0},
            .extent = {.width = skybox.width, .height = skybox.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        const auto constants = EquirectangularToCubemap::Constants
        {
            .Vertices     = geometryBuffer.cubeBuffer.deviceAddress,
            .Matrices     = m_matrixBuffer.deviceAddress,
            .SamplerIndex = textureManager.GetSampler(samplers.linearSamplerID).descriptorID,
            .TextureIndex = textureManager.GetTexture(hdrMapID).descriptorID
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            constants
        );

        pipeline.BindDescriptors(cmdBuffer, megaSet);

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
                .srcStageMask    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask   = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask    = VK_PIPELINE_STAGE_2_BLIT_BIT,
                .dstAccessMask   = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel    = 0,
                .levelCount      = skybox.mipLevels,
                .baseArrayLayer  = 0,
                .layerCount      = skybox.arrayLayers
            }
        );

        skybox.GenerateMipmaps(cmdBuffer, scratchAllocator);

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

        const auto skyboxID = textureManager.RegisterTexture
        (
            megaSet,
            context.device,
            "IBL/Skybox",
            skybox,
            skyboxView
        );

        deletionQueue.Push([device = context.device, skyboxRenderView] () mutable
        {
            skyboxRenderView.Destroy(device);
        });

        return skyboxID;
    }

    Vk::TextureID Generator::GenerateIrradianceMap
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::Context& context,
        const Objects::Samplers& samplers,
        const Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Vk::MegaSet& megaSet,
        Vk::TextureID skyboxID
    )
    {
        Vk::BeginLabel(cmdBuffer, "Irradiance Map Generation", {0.2988f, 0.2294f, 0.6607f, 1.0f});

        const auto& pipeline = pipelineManager.GetPipeline(Vk::PipelineID::IBLIrradiance);

        const auto irradianceMap = Vk::Image
        (
            context.allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = context.formatHelper.colorAttachmentFormatHDR,
                .extent                = {.width = IRRADIANCE_SIZE.x, .height = IRRADIANCE_SIZE.y, .depth = 1},
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
                .srcStageMask    = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask   = VK_ACCESS_2_NONE,
                .dstStageMask    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask   = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel    = 0,
                .levelCount      = irradianceMap.mipLevels,
                .baseArrayLayer  = 0,
                .layerCount      = irradianceMap.arrayLayers
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
                .offset = {.x = 0, .y = 0},
                .extent = {.width = irradianceMap.width, .height = irradianceMap.height}
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
            .width    = static_cast<f32>(irradianceMap.width),
            .height   = static_cast<f32>(irradianceMap.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {.x = 0, .y = 0},
            .extent = {.width = irradianceMap.width, .height = irradianceMap.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        const auto constants = Irradiance::Constants
        {
            .Vertices     = geometryBuffer.cubeBuffer.deviceAddress,
            .Matrices     = m_matrixBuffer.deviceAddress,
            .SamplerIndex = textureManager.GetSampler(samplers.linearSamplerID).descriptorID,
            .EnvMapIndex  = textureManager.GetTexture(skyboxID).descriptorID
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            constants
        );

        pipeline.BindDescriptors(cmdBuffer, megaSet);

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
                .srcStageMask    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask   = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask    = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask   = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel    = 0,
                .levelCount      = irradianceMap.mipLevels,
                .baseArrayLayer  = 0,
                .layerCount      = irradianceMap.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);

        return textureManager.RegisterTexture
        (
            megaSet,
            context.device,
            "IBL/Irradiance",
            irradianceMap,
            irradianceView
        );
    }

    Vk::TextureID Generator::GeneratePreFilterMap
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::Context& context,
        const Objects::Samplers& samplers,
        const Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        Vk::MegaSet& megaSet,
        Vk::TextureID skyboxID,
        Engine::DeletionQueue& deletionQueue
    )
    {
        Vk::BeginLabel(cmdBuffer, "PreFilter Map Generation", {0.2928f, 0.4794f, 0.6607f, 1.0f});

        const auto& preFilterPipeline = pipelineManager.GetPipeline(Vk::PipelineID::IBLPreFilter);

        const auto preFilterMap = Vk::Image
        (
            context.allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = context.formatHelper.colorAttachmentFormatHDR,
                .extent                = {.width = PRE_FILTER_SIZE.x, .height = PRE_FILTER_SIZE.y, .depth = 1},
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
                .srcStageMask    = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask   = VK_ACCESS_2_NONE,
                .dstStageMask    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask   = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel    = 0,
                .levelCount      = preFilterMap.mipLevels,
                .baseArrayLayer  = 0,
                .layerCount      = preFilterMap.arrayLayers
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
                    .offset = {.x = 0, .y = 0},
                    .extent = {.width = mipWidth, .height = mipHeight}
                },
                .layerCount           = 1,
                .viewMask             = 0b00111111,
                .colorAttachmentCount = 1,
                .pColorAttachments    = &colorAttachmentInfo,
                .pDepthAttachment     = nullptr,
                .pStencilAttachment   = nullptr
            };

            vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

            preFilterPipeline.Bind(cmdBuffer);

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
                .offset = {.x = 0, .y = 0},
                .extent = {.width = mipWidth, .height = mipHeight}
            };

            vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

            const auto constants = PreFilter::Constants
            {
                .Vertices     = geometryBuffer.cubeBuffer.deviceAddress,
                .Matrices     = m_matrixBuffer.deviceAddress,
                .SamplerIndex = textureManager.GetSampler(samplers.linearSamplerID).descriptorID,
                .EnvMapIndex  = textureManager.GetTexture(skyboxID).descriptorID ,
                .Roughness    = roughness,
                .SampleCount  = sampleCount
            };

            preFilterPipeline.PushConstants
            (
                cmdBuffer,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                constants
            );

            preFilterPipeline.BindDescriptors(cmdBuffer, megaSet);

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
                .srcStageMask    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask   = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask    = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask   = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel    = 0,
                .levelCount      = preFilterMap.mipLevels,
                .baseArrayLayer  = 0,
                .layerCount      = preFilterMap.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);

        const auto preFilterView = Vk::ImageView
        (
            context.device,
            preFilterMap,
            VK_IMAGE_VIEW_TYPE_CUBE,
            VkImageSubresourceRange{
                .aspectMask     = preFilterMap.aspect,
                .baseMipLevel   = 0,
                .levelCount     = preFilterMap.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = preFilterMap.arrayLayers
            }
        );

        const auto preFilterID = textureManager.RegisterTexture
        (
            megaSet,
            context.device,
            "IBL/PreFilter",
            preFilterMap,
            preFilterView
        );

        deletionQueue.Push([device = context.device, preFilterRenderViews] () mutable
        {
            for (const auto& view : preFilterRenderViews)
            {
                view.Destroy(device);
            }
        });

        return preFilterID;
    }

    Vk::TextureID Generator::ShrinkSkybox
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        Vk::TextureManager& textureManager,
        Vk::MegaSet& megaSet,
        Vk::TextureID skyboxID,
        Scratch::Allocator& scratchAllocator,
        Engine::DeletionQueue& deletionQueue
    )
    {
        Vk::BeginLabel(cmdBuffer, "Shrink Skybox", {0.2928f, 0.2794f, 0.6607f, 1.0f});

        const auto& skyboxWithMipmaps = textureManager.GetTexture(skyboxID);

        const auto skybox = Vk::Image
        (
            context.allocator,
            VkImageCreateInfo{
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = context.formatHelper.colorAttachmentFormatHDR,
                .extent                = {.width = SKYBOX_SIZE.x, .height = SKYBOX_SIZE.y, .depth = 1},
                .mipLevels             = 1,
                .arrayLayers           = 6,
                .samples               = VK_SAMPLE_COUNT_1_BIT,
                .tiling                = VK_IMAGE_TILING_OPTIMAL,
                .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
                .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
            },
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        auto barrierWriter = Vk::ScratchBarrierWriter(scratchAllocator);

        barrierWriter
        .WriteImageBarrier(skyboxWithMipmaps.image,
            Vk::ImageBarrier{
                .srcStageMask    = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask   = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask    = VK_PIPELINE_STAGE_2_COPY_BIT,
                .dstAccessMask   = VK_ACCESS_2_TRANSFER_READ_BIT,
                .oldLayout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel    = 0,
                .levelCount      = skyboxWithMipmaps.image.mipLevels,
                .baseArrayLayer  = 0,
                .layerCount      = skyboxWithMipmaps.image.arrayLayers
            }
        )
        .WriteImageBarrier(skybox,
            Vk::ImageBarrier{
                .srcStageMask    = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask   = VK_ACCESS_2_NONE,
                .dstStageMask    = VK_PIPELINE_STAGE_2_COPY_BIT,
                .dstAccessMask   = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel    = 0,
                .levelCount      = skybox.mipLevels,
                .baseArrayLayer  = 0,
                .layerCount      = skybox.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        const VkImageCopy2 copyRegion =
        {
            .sType          = VK_STRUCTURE_TYPE_IMAGE_COPY_2,
            .pNext          = nullptr,
            .srcSubresource = VkImageSubresourceLayers{
                .aspectMask     = skyboxWithMipmaps.image.aspect,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = skyboxWithMipmaps.image.arrayLayers
            },
            .srcOffset      = {.x = 0, .y = 0, .z = 0},
            .dstSubresource = VkImageSubresourceLayers{
                .aspectMask     = skybox.aspect,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = skybox.arrayLayers
            },
            .dstOffset      = {.x     = 0,            .y = 0,                  .z     = 0},
            .extent         = {.width = skybox.width, .height = skybox.height, .depth = 1}
        };

        const VkCopyImageInfo2 copyInfo =
        {
            .sType          = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
            .pNext          = nullptr,
            .srcImage       = skyboxWithMipmaps.image.handle,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage       = skybox.handle,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount    = 1,
            .pRegions       = &copyRegion
        };

        vkCmdCopyImage2(cmdBuffer.handle, &copyInfo);

        skybox.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask    = VK_PIPELINE_STAGE_2_COPY_BIT,
                .srcAccessMask   = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask    = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask   = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel    = 0,
                .levelCount      = skybox.mipLevels,
                .baseArrayLayer  = 0,
                .layerCount      = skybox.arrayLayers
            }
        );

        textureManager.DestroyTexture
        (
            skyboxID,
            context.device,
            context.allocator,
            megaSet,
            deletionQueue
        );

        const auto skyboxView = Vk::ImageView
        (
            context.device,
            skybox,
            VK_IMAGE_VIEW_TYPE_CUBE,
            VkImageSubresourceRange{
                .aspectMask     = skybox.aspect,
                .baseMipLevel   = 0,
                .levelCount     = skybox.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = skybox.arrayLayers
            }
        );

        skyboxID = textureManager.RegisterTexture
        (
            megaSet,
            context.device,
            "IBL/Skybox",
            skybox,
            skyboxView
        );

        Vk::EndLabel(cmdBuffer);

        return skyboxID;
    }

    Vk::TextureID Generator::GenerateBRDFLookupTable
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::Context& context,
        Vk::TextureManager& textureManager,
        Vk::StagingPool& stagingPool,
        Vk::MegaSet& megaSet,
        Vk::ImageDownloader& imageDownloader,
        tf::Executor& executor,
        Engine::DeletionQueue& deletionQueue
    )
    {
        if (m_brdfLutID.has_value())
        {
            return m_brdfLutID.value();
        }

        const Cache::Query query =
        {
            .cachedFile = BRDF_LUT_CACHE_FILE,
            .assetType  = Cache::AssetType::Texture,
            .hash       = GetBRDFLookupTableHash()
        };

        if (Cache::IsInCache(query))
        {
            m_brdfLutID = textureManager.LoadTexture
            (
                context.device,
                context.allocator,
                stagingPool,
                executor,
                deletionQueue,
                Vk::ImageUpload{
                    .type   = Vk::ImageUploadType::CACHE,
                    .flags  = Vk::ImageUploadFlags::None,
                    .source = Vk::ImageUploadCache{
                        .name       = "IBL/BRDFLookupTable",
                        .cachedPath = BRDF_LUT_CACHE_FILE
                    }
                }
            );
        }
        else
        {
            Vk::BeginLabel(cmdBuffer, "BRDF LUT Generation", {0.9215f, 0.0274f, 0.8588f, 1.0f});

            const auto& brdfLutPipeline = pipelineManager.GetPipeline(Vk::PipelineID::IBLBRDF);

            const auto brdfLut = Vk::Image
            (
                context.allocator,
                {
                    .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                    .pNext                 = nullptr,
                    .flags                 = 0,
                    .imageType             = VK_IMAGE_TYPE_2D,
                    .format                = BRDF_LUT_FORMAT,
                    .extent                = {.width = BRDF_LUT_SIZE.x, .height = BRDF_LUT_SIZE.y, .depth = 1},
                    .mipLevels             = 1,
                    .arrayLayers           = 1,
                    .samples               = VK_SAMPLE_COUNT_1_BIT,
                    .tiling                = VK_IMAGE_TILING_OPTIMAL,
                    .usage                 = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
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
                    .srcStageMask    = VK_PIPELINE_STAGE_2_NONE,
                    .srcAccessMask   = VK_ACCESS_2_NONE,
                    .dstStageMask    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dstAccessMask   = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                    .baseMipLevel    = 0,
                    .levelCount      = brdfLut.mipLevels,
                    .baseArrayLayer  = 0,
                    .layerCount      = brdfLut.arrayLayers
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
                    .offset = {.x = 0, .y = 0},
                    .extent = {.width = brdfLut.width, .height = brdfLut.height}
                },
                .layerCount           = 1,
                .viewMask             = 0,
                .colorAttachmentCount = 1,
                .pColorAttachments    = &colorAttachmentInfo,
                .pDepthAttachment     = nullptr,
                .pStencilAttachment   = nullptr
            };

            vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

            brdfLutPipeline.Bind(cmdBuffer);

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
                .offset = {.x = 0, .y = 0},
                .extent = {.width = brdfLut.width, .height = brdfLut.height}
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
                    .srcStageMask    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .srcAccessMask   = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstStageMask    = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dstAccessMask   = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .oldLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .newLayout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                    .baseMipLevel    = 0,
                    .levelCount      = brdfLut.mipLevels,
                    .baseArrayLayer  = 0,
                    .layerCount      = brdfLut.arrayLayers
                }
            );

            Vk::EndLabel(cmdBuffer);

            imageDownloader.RequestDownload(Vk::ImageDownload{
                .image                  = brdfLut,
                .postDownloadAction     = Vk::PostDownloadAction::Cache,
                .postDownloadActionData = Vk::PostDownloadCache{
                    .cacheFile = BRDF_LUT_CACHE_FILE,
                    .hash      = GetBRDFLookupTableHash()
                }
            });

            m_brdfLutID = textureManager.RegisterTexture
            (
                megaSet,
                context.device,
                "IBL/BRDFLookupTable",
                brdfLut,
                brdfLutView
            );
        }

        return m_brdfLutID.value();
    }

    void Generator::Destroy(VmaAllocator allocator)
    {
        m_matrixBuffer.Destroy(allocator);
    }
}