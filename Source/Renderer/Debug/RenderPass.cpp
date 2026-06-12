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

#include "RenderPass.h"

#include "Debug/AABB/GenerateDrawCalls.h"
#include "Debug/AABB/AABB.h"
#include "Debug/Sphere.h"
#include "Debug/GenerateCullingStatistics.h"
#include "Debug/GenerateTiledLightingStatistics.h"
#include "Util/WireframeCone.h"
#include "Util/WireframeSphere.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::Debug
{
    RenderPass::RenderPass
    (
        VkDevice device,
        VmaAllocator allocator,
        const Vk::FormatHelper& formatHelper,
        const Vk::Swapchain& swapchain,
        Vk::PipelineManager& pipelineManager,
        Vk::FramebufferManager& framebufferManager,
        Vk::StagingPool& stagingPool
    )
    {
        // Pipelines
        {
            constexpr std::array DYNAMIC_STATES =
            {
                VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
                VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT
            };

            const std::array colorFormats = {swapchain.surfaceFormat.format};

            pipelineManager.AddPipeline("Debug/AABB/GenerateDrawCalls", Vk::PipelineConfig{}
                .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
                .AttachShader("Debug/AABB/GenerateDrawCalls.comp", VK_SHADER_STAGE_COMPUTE_BIT)
                .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(AABB::Generate::Constants))
            );

            pipelineManager.AddPipeline("Debug/AABB", Vk::PipelineConfig{}
                .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
                .SetRenderingInfo(0, colorFormats, formatHelper.depthFormat)
                .AttachShader("Debug/AABB/AABB.vert", VK_SHADER_STAGE_VERTEX_BIT)
                .AttachShader("Debug/AABB/AABB.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
                .SetDynamicStates(DYNAMIC_STATES)
                .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
                .SetRasterizerState(VK_FALSE, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
                .SetDepthState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER)
                .AddDefaultBlendAttachment()
                .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(AABB::Render::Constants))
            );

            pipelineManager.AddPipeline("Debug/Light/Sphere", Vk::PipelineConfig{}
                .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
                .SetRenderingInfo(0, colorFormats, formatHelper.depthFormat)
                .AttachShader("Debug/Sphere.vert", VK_SHADER_STAGE_VERTEX_BIT)
                .AttachShader("Debug/Sphere.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
                .SetDynamicStates(DYNAMIC_STATES)
                .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
                .SetRasterizerState(VK_FALSE, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
                .SetDepthState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER)
                .AddDefaultBlendAttachment()
                .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Sphere::Constants))
            );

            pipelineManager.AddPipeline("Debug/Culling/GenerateStatistics", Vk::PipelineConfig{}
                .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
                .AttachShader("Debug/GenerateCullingStatistics.comp", VK_SHADER_STAGE_COMPUTE_BIT)
                .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Culling::Constants))
            );

            pipelineManager.AddPipeline("Debug/TiledLighting/GenerateStatistics", Vk::PipelineConfig{}
                .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
                .AttachShader("Debug/GenerateTiledLightingStatistics.comp", VK_SHADER_STAGE_COMPUTE_BIT)
                .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(TiledLighting::Constants))
            );
        }

        // Framebuffers
        {
            framebufferManager.AddFramebuffer
            (
                "Debug/Depth",
                Vk::FramebufferCustomFormat::Depth,
                VK_IMAGE_VIEW_TYPE_2D,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                [] (ENGINE_UNUSED const VkExtent2D& renderExtent, const VkExtent2D& displayExtent) -> Vk::FramebufferSize
                {
                    return Vk::FramebufferSize
                    {
                        .width       = displayExtent.width,
                        .height      = displayExtent.height,
                        .mipLevels   = 1,
                        .arrayLayers = 1
                    };
                },
                Vk::FramebufferInitialState{
                    .stageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                }
            );

            framebufferManager.AddFramebufferView
            (
                "Debug/Depth",
                "Debug/DepthView",
                VK_IMAGE_VIEW_TYPE_2D,
                Vk::FramebufferViewSize{
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            );
        }

        // AABB
        {
            constexpr std::array<u16, 24> AABB_INDICES = {0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 2, 6, 3, 7};

            constexpr VkDeviceSize AABB_INDICES_SIZE = sizeof(u16) * AABB_INDICES.size();

            m_aabbIndexBuffer = Vk::Buffer
            (
                device,
                allocator,
                AABB_INDICES_SIZE,
                0,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            );

            m_pendingAABBIndexUpload = stagingPool.Allocate
            (
                device,
                allocator,
                AABB_INDICES_SIZE,
                0
            );

            std::memcpy(m_pendingAABBIndexUpload->hostAddress, AABB_INDICES.data(), AABB_INDICES_SIZE);

            constexpr usize DRAW_CALL_BUFFER_COUNT = 4;

            m_aabbDrawCallBuffer = Vk::Buffer
            (
                device,
                allocator,
                DRAW_CALL_BUFFER_COUNT * sizeof(VkDrawIndexedIndirectCommand),
                0,
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            );
        }

        // Light Sphere
        {
            constexpr usize LIGHT_SPHERE_STACKS = 32;
            constexpr usize LIGHT_SPHERE_SLICES = 32;

            const auto lightSphere = Maths::WireframeSphere(LIGHT_SPHERE_STACKS, LIGHT_SPHERE_SLICES);

            // Indices
            {
                const usize sphereIndicesSize = lightSphere.indices.size() * sizeof(u32);

                m_sphereIndexBuffer = Vk::Buffer
                (
                    device,
                    allocator,
                    sphereIndicesSize,
                    0,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    0,
                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
                );

                m_pendingSphereIndexUpload = stagingPool.Allocate
                (
                    device,
                    allocator,
                    sphereIndicesSize,
                    0
                );

                std::memcpy(m_pendingSphereIndexUpload->hostAddress, lightSphere.indices.data(), sphereIndicesSize);
            }

            // Vertices
            {
                const usize sphereVerticesSize = lightSphere.vertices.size() * sizeof(glm::vec3);

                m_sphereVertexBuffer = Vk::Buffer
                (
                    device,
                    allocator,
                    sphereVerticesSize,
                    0,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    0,
                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
                );

                m_pendingSphereVertexUpload = stagingPool.Allocate
                (
                    device,
                    allocator,
                    sphereVerticesSize,
                    0
                );

                std::memcpy(m_pendingSphereVertexUpload->hostAddress, lightSphere.vertices.data(), sphereVerticesSize);
            }
        }

        // Culling Statistics
        {
            m_cullingStatisticsBuffer = Vk::Buffer
            (
                device,
                allocator,
                sizeof(Culling::CullingStatisticsBuffer),
                0,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            );

            for (auto& buffer : m_cullingStatisticsReadbackBuffers)
            {
                buffer = Vk::Buffer
                (
                    device,
                    allocator,
                    sizeof(Culling::CullingStatisticsBuffer),
                    0,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                    VMA_MEMORY_USAGE_AUTO
                );

                constexpr Culling::CullingStatisticsBuffer ZERO = {};

                std::memcpy(buffer.hostAddress, &ZERO, sizeof(Culling::CullingStatisticsBuffer));
            }
        }

        // Tiled Lighting Statistics
        {
            m_tiledLightingStatisticsBuffer = Vk::Buffer
            (
                device,
                allocator,
                sizeof(TiledLighting::TiledLightingStatisticsBuffer),
                0,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            );

            for (auto& buffer : m_tiledLightingStatisticsReadbackBuffers)
            {
                buffer = Vk::Buffer
                (
                    device,
                    allocator,
                    sizeof(TiledLighting::TiledLightingStatisticsBuffer),
                    0,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                    VMA_MEMORY_USAGE_AUTO
                );

                constexpr TiledLighting::TiledLightingStatisticsBuffer ZERO = {};

                std::memcpy(buffer.hostAddress, &ZERO, sizeof(TiledLighting::TiledLightingStatisticsBuffer));
            }
        }

        // Naming
        {
            Vk::SetDebugName(device, m_aabbIndexBuffer.handle,               "Debug/AABB/IndexBuffer");
            Vk::SetDebugName(device, m_aabbDrawCallBuffer.handle,            "Debug/AABB/DrawCalls");
            Vk::SetDebugName(device, m_sphereIndexBuffer.handle,             "Debug/Lights/Sphere/IndexBuffer");
            Vk::SetDebugName(device, m_sphereVertexBuffer.handle,            "Debug/Lights/Sphere/VertexBuffer");
            Vk::SetDebugName(device, m_cullingStatisticsBuffer.handle,       "Debug/Culling/StatisticsBuffer");
            Vk::SetDebugName(device, m_tiledLightingStatisticsBuffer.handle, "Debug/TiledLighting/StatisticsBuffer");

            for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
            {
                Vk::SetDebugName(device, m_cullingStatisticsReadbackBuffers[i].handle,       fmt::format("Debug/Culling/StatisticsBuffer/Readback/{}",       i));
                Vk::SetDebugName(device, m_tiledLightingStatisticsReadbackBuffers[i].handle, fmt::format("Debug/TiledLighting/StatisticsBuffer/Readback/{}", i));
            }
        }
    }

    void RenderPass::Render
    (
        usize FIF,
        usize frameIndex,
        VkDevice device,
        VmaAllocator allocator,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::Swapchain& swapchain,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer,
        const Buffers::TileLightIndexBuffer& tiledLightIndexBuffer,
        Vk::StagingPool& stagingPool,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Debug"))
            {
                constexpr ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH |
                                                  ImGuiTableFlags_BordersInnerV |
                                                  ImGuiTableFlags_BordersOuterH |
                                                  ImGuiTableFlags_BordersOuterV;

                ImGui::Checkbox("Render AABBs", &m_aabbDebugOptions.enabled);

                if (m_aabbDebugOptions.enabled)
                {
                    auto AABBDebugOptionUI = [] (const std::string_view label, const std::string_view id, RenderPass::AABBDebugOption& debugOption) mutable
                    {
                        ImGui::Checkbox(label.data(), &debugOption.enabled);
                        ImGui::SameLine();
                        ImGui::ColorEdit3(id.data(), &debugOption.color[0], ImGuiColorEditFlags_NoInputs);
                    };

                    ImGui::Separator();

                    if (ImGui::TreeNodeEx("Opaque", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        AABBDebugOptionUI("Single Sided", "##OpaqueSingleSided", m_aabbDebugOptions.opaque.singleSided);
                        AABBDebugOptionUI("Double Sided", "##OpaqueDoubleSided", m_aabbDebugOptions.opaque.doubleSided);

                        ImGui::TreePop();
                    }

                    ImGui::Separator();

                    if (ImGui::TreeNodeEx("Alpha Masked", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        AABBDebugOptionUI("Single Sided", "##AlphaMaskedSingleSided", m_aabbDebugOptions.alphaMasked.singleSided);
                        AABBDebugOptionUI("Double Sided", "##AlphaMaskedDoubleSided", m_aabbDebugOptions.alphaMasked.doubleSided);

                        ImGui::TreePop();
                    }
                }

                ImGui::Separator();

                ImGui::Checkbox("Render Point Lights", &m_enablePointLightDebug);

                ImGui::Separator();

                ImGui::Checkbox("Render Spot Lights", &m_enableSpotLightDebug);

                m_enableCullingStatistics = ImGui::CollapsingHeader("Culling Statistics");

                if (m_enableCullingStatistics)
                {
                    if (ImGui::BeginTable("##CullingStatisticsTable", 4, flags))
                    {
                        const auto* statistics = static_cast<const Culling::CullingStatisticsBuffer*>(m_cullingStatisticsReadbackBuffers[FIF].hostAddress);

                        auto CullingStatisticsDebugUI = [] (const std::string_view name, const Culling::MeshCullingStatistics& statistics)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%s", name.data());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%u", statistics.instanceCount);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%u", statistics.vertexCount);
                            ImGui::TableSetColumnIndex(3);
                            ImGui::Text("%u", statistics.indexCount);
                        };

                        ImGui::TableSetupColumn("Type");
                        ImGui::TableSetupColumn("Instance Count");
                        ImGui::TableSetupColumn("Vertex Count");
                        ImGui::TableSetupColumn("Index Count");

                        ImGui::TableSetupScrollFreeze(0, 0);

                        ImGui::TableHeadersRow();

                        CullingStatisticsDebugUI("Opaque",                 statistics->opaque);
                        CullingStatisticsDebugUI("OpaqueDoubleSided",      statistics->opaqueDoubleSided);
                        CullingStatisticsDebugUI("AlphaMasked",            statistics->alphaMasked);
                        CullingStatisticsDebugUI("AlphaMaskedDoubleSided", statistics->alphaMaskedDoubleSided);
                        CullingStatisticsDebugUI("Total",                  statistics->total);

                        ImGui::EndTable();
                    }
                }

                m_enableTiledLightingStatistics = ImGui::CollapsingHeader("Tiled Lighting Statistics");

                if (m_enableTiledLightingStatistics)
                {
                    if (ImGui::BeginTable("##TiledLightingStatisticsTable", 3, flags))
                    {
                        const auto* statistics = static_cast<const TiledLighting::TiledLightingStatisticsBuffer*>(m_tiledLightingStatisticsReadbackBuffers[FIF].hostAddress);

                        auto TiledLightingDebugUI = [] (const std::string_view name, f32 averageLightsPerTile, f32 coverageRatio)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%s", name.data());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%.3f", averageLightsPerTile);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%.3f", 100.0f * coverageRatio);
                        };

                        ImGui::TableSetupColumn("Type");
                        ImGui::TableSetupColumn("Average Lights Per Tile");
                        ImGui::TableSetupColumn("% Coverage");

                        ImGui::TableSetupScrollFreeze(0, 0);

                        ImGui::TableHeadersRow();

                        TiledLightingDebugUI("Point",                  statistics->averagePointLightsPerTile,         statistics->pointLightCoverage);
                        TiledLightingDebugUI("Point (Shadow Casting)", statistics->averageShadowedPointLightsPerTile, statistics->shadowedPointLightCoverage);
                        TiledLightingDebugUI("Spot",                   statistics->averageSpotLightsPerTile,          statistics->spotLightCoverage);
                        TiledLightingDebugUI("Spot (Shadow Casting)",  statistics->averageShadowedSpotLightsPerTile,  statistics->shadowedSpotLightCoverage);
                        TiledLightingDebugUI("Total",                  statistics->averageLightsPerTile,              statistics->lightCoverage);

                        ImGui::EndTable();
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        Vk::BeginLabel(cmdBuffer, "Debug", {0.6117f, 0.5749f, 0.1901f, 1.0f});

        UploadData
        (
            cmdBuffer,
            stagingPool,
            deletionQueue
        );

        if (m_aabbDebugOptions.enabled)
        {
            GenerateAABBDrawCalls
            (
                cmdBuffer,
                pipelineManager,
                indirectBuffer
            );
        }

        BeginDebugRender
        (
            cmdBuffer,
            framebufferManager,
            swapchain
        );

        if (m_aabbDebugOptions.enabled)
        {
            RenderDebugAABB
            (
                FIF,
                frameIndex,
                cmdBuffer,
                pipelineManager,
                sceneBuffer,
                meshBuffer,
                indirectBuffer
            );
        }

        if (m_enablePointLightDebug)
        {
            RenderDebugPointLight
            (
                FIF,
                cmdBuffer,
                pipelineManager,
                sceneBuffer
            );
        }

        if (m_enableSpotLightDebug)
        {
            RenderDebugSpotLight
            (
                FIF,
                device,
                allocator,
                cmdBuffer,
                pipelineManager,
                sceneBuffer,
                deletionQueue
            );
        }

        EndDebugRender(cmdBuffer, framebufferManager);

        if (m_enableCullingStatistics)
        {
            GenerateCullingStatistics
            (
                FIF,
                frameIndex,
                cmdBuffer,
                pipelineManager,
                meshBuffer,
                indirectBuffer
            );
        }

        if (m_enableTiledLightingStatistics)
        {
            GenerateTiledLightingStatistics
            (
                FIF,
                cmdBuffer,
                pipelineManager,
                framebufferManager,
                sceneBuffer,
                tiledLightIndexBuffer
            );
        }

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::UploadData
    (
        const Vk::CommandBuffer& cmdBuffer,
        Vk::StagingPool& stagingPool,
        Util::DeletionQueue& deletionQueue
    )
    {
        Vk::BeginLabel(cmdBuffer, "Data Transfer", {0.6117f, 0.0749f, 0.3901f, 1.0f});

        if (m_pendingAABBIndexUpload.has_value())
        {
            const VkBufferCopy2 copyRegion =
            {
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = m_pendingAABBIndexUpload->memoryBlock.offset,
                .dstOffset = 0,
                .size      = m_pendingAABBIndexUpload->memoryBlock.size
            };

            const VkCopyBufferInfo2 copyInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .pNext       = nullptr,
                .srcBuffer   = m_pendingAABBIndexUpload->buffer,
                .dstBuffer   = m_aabbIndexBuffer.handle,
                .regionCount = 1,
                .pRegions    = &copyRegion
            };

            vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

            m_aabbIndexBuffer.Barrier
            (
                cmdBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
                    .dstAccessMask  = VK_ACCESS_2_INDEX_READ_BIT,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = 0,
                    .size           = m_pendingAABBIndexUpload->memoryBlock.size
                }
            );

            deletionQueue.Push([&stagingPool, stagingMemoryBlock = m_pendingAABBIndexUpload.value()] () mutable
            {
                stagingPool.Free(stagingMemoryBlock);
            });

            m_pendingAABBIndexUpload = std::nullopt;
        }

        if (m_pendingSphereIndexUpload.has_value())
        {
            const VkBufferCopy2 copyRegion =
            {
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = m_pendingSphereIndexUpload->memoryBlock.offset,
                .dstOffset = 0,
                .size      = m_pendingSphereIndexUpload->memoryBlock.size
            };

            const VkCopyBufferInfo2 copyInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .pNext       = nullptr,
                .srcBuffer   = m_pendingSphereIndexUpload->buffer,
                .dstBuffer   = m_sphereIndexBuffer.handle,
                .regionCount = 1,
                .pRegions    = &copyRegion
            };

            vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

            m_sphereIndexBuffer.Barrier
            (
                cmdBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
                    .dstAccessMask  = VK_ACCESS_2_INDEX_READ_BIT,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = 0,
                    .size           = m_pendingSphereIndexUpload->memoryBlock.size
                }
            );

            deletionQueue.Push([&stagingPool, stagingMemoryBlock = m_pendingSphereIndexUpload.value()] () mutable
            {
                stagingPool.Free(stagingMemoryBlock);
            });

            m_pendingSphereIndexUpload = std::nullopt;
        }

        if (m_pendingSphereVertexUpload.has_value())
        {
            const VkBufferCopy2 copyRegion =
            {
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = m_pendingSphereVertexUpload->memoryBlock.offset,
                .dstOffset = 0,
                .size      = m_pendingSphereVertexUpload->memoryBlock.size
            };

            const VkCopyBufferInfo2 copyInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .pNext       = nullptr,
                .srcBuffer   = m_pendingSphereVertexUpload->buffer,
                .dstBuffer   = m_sphereVertexBuffer.handle,
                .regionCount = 1,
                .pRegions    = &copyRegion
            };

            vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

            m_sphereVertexBuffer.Barrier
            (
                cmdBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
                    .dstAccessMask  = VK_ACCESS_2_INDEX_READ_BIT,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = 0,
                    .size           = m_pendingSphereVertexUpload->memoryBlock.size
                }
            );

            deletionQueue.Push([&stagingPool, stagingMemoryBlock = m_pendingSphereVertexUpload.value()] () mutable
            {
                stagingPool.Free(stagingMemoryBlock);
            });

            m_pendingSphereVertexUpload = std::nullopt;
        }

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::BeginDebugRender
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::Swapchain& swapchain
    )
    {
        Vk::BeginLabel(cmdBuffer, "Render", {0.6117f, 0.3749f, 0.1901f, 1.0f});

        const auto& currentImageView = swapchain.imageViews[swapchain.imageIndex];

        const auto& debugDepthView = framebufferManager.GetFramebufferView("Debug/DepthView");

        const auto& debugDepth = framebufferManager.GetFramebuffer(debugDepthView.framebuffer);

        debugDepth.image.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                .dstAccessMask  = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = debugDepth.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = debugDepth.image.arrayLayers
            }
        );

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = currentImageView.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {}
        };

        const VkRenderingAttachmentInfo depthAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = debugDepthView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {.depthStencil = {.depth = 0.0f, .stencil = 0x0}}
        };

        const VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {.x = 0, .y = 0},
                .extent = swapchain.extent
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = &depthAttachmentInfo,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(swapchain.extent.width),
            .height   = static_cast<f32>(swapchain.extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {.x = 0, .y = 0},
            .extent = swapchain.extent
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);
    }

    void RenderPass::EndDebugRender(const Vk::CommandBuffer& cmdBuffer, const Vk::FramebufferManager& framebufferManager)
    {
        vkCmdEndRendering(cmdBuffer.handle);

        const auto& debugDepth = framebufferManager.GetFramebuffer("Debug/Depth");

        debugDepth.image.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                .srcAccessMask  = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = debugDepth.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = debugDepth.image.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::GenerateAABBDrawCalls
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, "AABB/GenerateDrawCalls", {0.1657f, 0.5149f, 0.4901f, 1.0f});

        Vk::BarrierWriter barrierWriter = {};

        barrierWriter
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            m_aabbDrawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = m_aabbDrawCallBuffer.size
            }
        )
        .Execute(cmdBuffer);

        const auto& pipeline = pipelineManager.GetPipeline("Debug/AABB/GenerateDrawCalls");

        pipeline.Bind(cmdBuffer);

        const AABB::Generate::Constants constants =
        {
            .CulledOpaqueDrawCalls                 = indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer.deviceAddress,
            .CulledOpaqueDoubleSidedDrawCalls      = indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer.deviceAddress,
            .CulledAlphaMaskedDrawCalls            = indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer.deviceAddress,
            .CulledAlphaMaskedDoubleSidedDrawCalls = indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer.deviceAddress,
            .DebugAABBDrawCalls                    = m_aabbDrawCallBuffer.deviceAddress
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        vkCmdDispatch
        (
            cmdBuffer.handle,
            1,
            1,
            1
        );

        barrierWriter
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            m_aabbDrawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = m_aabbDrawCallBuffer.size
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::RenderDebugAABB
    (
        usize FIF,
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, "AABB/Render", {0.1657f, 0.9149f, 0.4901f, 1.0f});

        const auto& pipeline = pipelineManager.GetPipeline("Debug/AABB");

        pipeline.Bind(cmdBuffer);

        vkCmdBindIndexBuffer
        (
            cmdBuffer.handle,
            m_aabbIndexBuffer.handle,
            0,
            VK_INDEX_TYPE_UINT16
        );

        // Opaque
        {
            Vk::BeginLabel(cmdBuffer, "Opaque", glm::vec4(0.6091f, 0.7243f, 0.2549f, 1.0f));

            if (m_aabbDebugOptions.opaque.singleSided.enabled)
            {
                Vk::BeginLabel(cmdBuffer, "Single Sided", glm::vec4(0.3091f, 0.7243f, 0.2549f, 1.0f));

                const auto constants = AABB::Render::Constants
                {
                    .Scene           = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                    .Meshes          = meshBuffer.GetCurrentMeshBuffer(frameIndex).deviceAddress,
                    .Instances       = meshBuffer.GetCurrentInstanceBuffer(frameIndex).deviceAddress,
                    .InstanceIndices = indirectBuffer.frustumCulledBuffers.opaqueBuffer.instanceIndexBuffer.deviceAddress,
                    .Color           = m_aabbDebugOptions.opaque.singleSided.color
                };

                pipeline.PushConstants
                (
                   cmdBuffer,
                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                   constants
                );

                vkCmdDrawIndexedIndirect
                (
                    cmdBuffer.handle,
                    m_aabbDrawCallBuffer.handle,
                    0 * sizeof(VkDrawIndexedIndirectCommand),
                    1,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                Vk::EndLabel(cmdBuffer);
            }

            if (m_aabbDebugOptions.opaque.doubleSided.enabled)
            {
                Vk::BeginLabel(cmdBuffer, "Double Sided", glm::vec4(0.6091f, 0.2213f, 0.2549f, 1.0f));

                const auto constants = AABB::Render::Constants
                {
                    .Scene           = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                    .Meshes          = meshBuffer.GetCurrentMeshBuffer(frameIndex).deviceAddress,
                    .Instances       = meshBuffer.GetCurrentInstanceBuffer(frameIndex).deviceAddress,
                    .InstanceIndices = indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.instanceIndexBuffer.deviceAddress,
                    .Color           = m_aabbDebugOptions.opaque.doubleSided.color
                };

                pipeline.PushConstants
                (
                   cmdBuffer,
                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                   constants
                );

                vkCmdDrawIndexedIndirect
                (
                    cmdBuffer.handle,
                    m_aabbDrawCallBuffer.handle,
                    1 * sizeof(VkDrawIndexedIndirectCommand),
                    1,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                Vk::EndLabel(cmdBuffer);
            }

            Vk::EndLabel(cmdBuffer);
        }

        // Alpha Masked
        {
            Vk::BeginLabel(cmdBuffer, "Alpha Masked", glm::vec4(0.9091f, 0.2243f, 0.6549f, 1.0f));

            if (m_aabbDebugOptions.alphaMasked.singleSided.enabled)
            {
                Vk::BeginLabel(cmdBuffer, "Single Sided", glm::vec4(0.3091f, 0.7243f, 0.2549f, 1.0f));

                const auto constants = AABB::Render::Constants
                {
                    .Scene               = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                    .Meshes              = meshBuffer.GetCurrentMeshBuffer(frameIndex).deviceAddress,
                    .Instances           = meshBuffer.GetCurrentInstanceBuffer(frameIndex).deviceAddress,
                    .InstanceIndices     = indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.instanceIndexBuffer.deviceAddress,
                    .Color               = m_aabbDebugOptions.alphaMasked.singleSided.color
                };

                pipeline.PushConstants
                (
                   cmdBuffer,
                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                   constants
                );

                vkCmdDrawIndexedIndirect
                (
                    cmdBuffer.handle,
                    m_aabbDrawCallBuffer.handle,
                    2 * sizeof(VkDrawIndexedIndirectCommand),
                    1,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                Vk::EndLabel(cmdBuffer);
            }

            if (m_aabbDebugOptions.alphaMasked.doubleSided.enabled)
            {
                Vk::BeginLabel(cmdBuffer, "Double Sided", glm::vec4(0.6091f, 0.2213f, 0.2549f, 1.0f));

                const auto constants = AABB::Render::Constants
                {
                    .Scene               = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                    .Meshes              = meshBuffer.GetCurrentMeshBuffer(frameIndex).deviceAddress,
                    .Instances           = meshBuffer.GetCurrentInstanceBuffer(frameIndex).deviceAddress,
                    .InstanceIndices     = indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.instanceIndexBuffer.deviceAddress,
                    .Color               = m_aabbDebugOptions.alphaMasked.doubleSided.color
                };

                pipeline.PushConstants
                (
                   cmdBuffer,
                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                   constants
                );

                vkCmdDrawIndexedIndirect
                (
                    cmdBuffer.handle,
                    m_aabbDrawCallBuffer.handle,
                    3 * sizeof(VkDrawIndexedIndirectCommand),
                    1,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                Vk::EndLabel(cmdBuffer);
            }

            Vk::EndLabel(cmdBuffer);
        }

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::RenderDebugPointLight
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Buffers::SceneBuffer& sceneBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, "Render/PointLights", {0.6657f, 0.9149f, 0.4901f, 1.0f});

        const auto& pipeline = pipelineManager.GetPipeline("Debug/Light/Sphere");

        pipeline.Bind(cmdBuffer);

        vkCmdBindIndexBuffer
        (
            cmdBuffer.handle,
            m_sphereIndexBuffer.handle,
            0,
            VK_INDEX_TYPE_UINT32
        );

        for (const auto& pointLight : sceneBuffer.pointLights)
        {
            const auto constants = Sphere::Constants
            {
                .Scene     = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                .Positions = m_sphereVertexBuffer.deviceAddress,
                .Transform = Maths::TransformMatrix(
                    pointLight.position,
                    glm::vec3(0.0f),
                    glm::vec3(pointLight.range)
                ),
                .Color     = pointLight.color
            };

            pipeline.PushConstants
            (
               cmdBuffer,
               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
               constants
            );

            vkCmdDrawIndexed
            (
                cmdBuffer.handle,
                m_sphereIndexBuffer.size / sizeof(u32),
                1,
                0,
                0,
                0
            );
        }

        for (const auto& pointLight : sceneBuffer.shadowedPointLights)
        {
            const auto constants = Sphere::Constants
            {
                .Scene     = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                .Positions = m_sphereVertexBuffer.deviceAddress,
                .Transform = Maths::TransformMatrix(
                    pointLight.position,
                    glm::vec3(0.0f),
                    glm::vec3(pointLight.range)
                ),
                .Color     = pointLight.color
            };

            pipeline.PushConstants
            (
               cmdBuffer,
               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
               constants
            );

            vkCmdDrawIndexed
            (
                cmdBuffer.handle,
                m_sphereIndexBuffer.size / sizeof(u32),
                1,
                0,
                0,
                0
            );
        }

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::RenderDebugSpotLight
    (
        usize FIF,
        VkDevice device,
        VmaAllocator allocator,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Buffers::SceneBuffer& sceneBuffer,
        Util::DeletionQueue& deletionQueue
    )
    {
        constexpr usize LIGHT_CONE_STACKS = 32;
        constexpr usize LIGHT_CONE_SLICES = 32;

        // Mildy scuffed transform logic, but it works for now
        auto ComputeTransform = [] (const glm::vec3& position, const glm::vec3& direction, f32 range) -> glm::mat4
        {
            constexpr glm::mat4 identity = glm::identity<glm::mat4>();

            const glm::vec3 normalizedDirection = glm::normalize(-direction);

            const glm::vec3 axis  = glm::normalize(Maths::SafeCross(Renderer::WORLD_UP, normalizedDirection, glm::vec3(1.0f, 0.0f, 0.0f), 1e-6f));
            const f32       angle = glm::acos(glm::clamp(glm::dot(Renderer::WORLD_UP, normalizedDirection), -1.0f, 1.0f));

            const glm::quat rotationQuat = glm::angleAxis(angle, axis);

            const glm::mat4 rotation    = glm::mat4_cast(rotationQuat);
            const glm::mat4 translation = glm::translate(identity, position - range * normalizedDirection);

            return translation * rotation;
        };

        Vk::BeginLabel(cmdBuffer, "Render/SpotLights", {0.6157f, 0.9149f, 0.7901f, 1.0f});

        std::vector<Maths::WireframeCone> cones = {};

        for (const auto& light : sceneBuffer.spotLights)
        {
            const f32 height = light.range;
            const f32 base   = std::tanf(light.cutOff.y) * height;

            cones.emplace_back
            (
                base,
                height,
                LIGHT_CONE_STACKS,
                LIGHT_CONE_SLICES
            );
        }

        for (const auto& light : sceneBuffer.shadowedSpotLights)
        {
            const f32 height = light.range;
            const f32 base   = std::tanf(light.cutOff.y) * height;

            cones.emplace_back
            (
                base,
                height,
                LIGHT_CONE_STACKS,
                LIGHT_CONE_SLICES
            );
        }

        u32 totalVertexCount = 0;
        u32 totalIndexCount  = 0;

        for (const auto& cone : cones)
        {
            totalIndexCount  += cone.indices.size();
            totalVertexCount += cone.vertices.size();
        }

        const VkDeviceSize requiredIndexSize  = totalIndexCount  * sizeof(u32);
        const VkDeviceSize requiredVertexSize = totalVertexCount * sizeof(glm::vec3);

        if (m_coneIndexBuffer.size < requiredIndexSize)
        {
            deletionQueue.Push([allocator, buffer = m_coneIndexBuffer] () mutable
            {
                buffer.Destroy(allocator);
            });

            m_coneIndexBuffer = Vk::Buffer
            (
                device,
                allocator,
                requiredIndexSize,
                0,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            Vk::SetDebugName(device, m_coneIndexBuffer.handle, "Debug/Lights/Cone/IndexBuffer");
        }

        if (m_coneVertexBuffer.size < requiredVertexSize)
        {
            deletionQueue.Push([allocator, buffer = m_coneVertexBuffer] () mutable
            {
                buffer.Destroy(allocator);
            });

            m_coneVertexBuffer = Vk::Buffer
            (
                device,
                allocator,
                requiredVertexSize,
                0,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            Vk::SetDebugName(device, m_coneVertexBuffer.handle, "Debug/Lights/Cone/VertexBuffer");
        }

        const auto& pipeline = pipelineManager.GetPipeline("Debug/Light/Sphere");

        pipeline.Bind(cmdBuffer);

        vkCmdBindIndexBuffer
        (
            cmdBuffer.handle,
            m_coneIndexBuffer.handle,
            0,
            VK_INDEX_TYPE_UINT32
        );

        usize coneIndex = 0;

        u32 indexOffset  = 0;
        u32 vertexOffset = 0;

        for (const auto& light : sceneBuffer.spotLights)
        {
            const auto& cone = cones[coneIndex];

            std::memcpy
            (
                static_cast<u8*>(m_coneIndexBuffer.hostAddress) + sizeof(u32) * indexOffset,
                cone.indices.data(),
                sizeof(u32) * cone.indices.size()
            );

            std::memcpy
            (
                static_cast<u8*>(m_coneVertexBuffer.hostAddress) + sizeof(glm::vec3) * vertexOffset,
                cone.vertices.data(),
                sizeof(glm::vec3) * cone.vertices.size()
            );

            const auto constants = Sphere::Constants
            {
                .Scene     = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                .Positions = m_coneVertexBuffer.deviceAddress,
                .Transform = ComputeTransform(light.position, light.direction, light.range),
                .Color     = light.color
            };

            pipeline.PushConstants
            (
               cmdBuffer,
               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
               constants
            );

            vkCmdDrawIndexed
            (
                cmdBuffer.handle,
                cone.indices.size(),
                1,
                indexOffset,
                static_cast<s32>(vertexOffset),
                0
            );

            indexOffset  += cone.indices.size();
            vertexOffset += cone.vertices.size();

            ++coneIndex;
        }

        for (const auto& light : sceneBuffer.shadowedSpotLights)
        {
            const auto& cone = cones[coneIndex];

            std::memcpy
            (
                static_cast<u8*>(m_coneIndexBuffer.hostAddress) + sizeof(u32) * indexOffset,
                cone.indices.data(),
                sizeof(u32) * cone.indices.size()
            );

            std::memcpy
            (
                static_cast<u8*>(m_coneVertexBuffer.hostAddress) + sizeof(glm::vec3) * vertexOffset,
                cone.vertices.data(),
                sizeof(glm::vec3) * cone.vertices.size()
            );

            const auto constants = Sphere::Constants
            {
                .Scene     = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                .Positions = m_coneVertexBuffer.deviceAddress,
                .Transform = ComputeTransform(light.position, light.direction, light.range),
                .Color     = light.color
            };

            pipeline.PushConstants
            (
               cmdBuffer,
               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
               constants
            );

            vkCmdDrawIndexed
            (
                cmdBuffer.handle,
                cone.indices.size(),
                1,
                indexOffset,
                static_cast<s32>(vertexOffset),
                0
            );

            indexOffset  += cone.indices.size();
            vertexOffset += cone.vertices.size();

            ++coneIndex;
        }

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::GenerateCullingStatistics
    (
        usize FIF,
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, "Culling/GenerateStatistics", {0.1657f, 0.1149f, 0.3901f, 1.0f});

        Vk::BarrierWriter barrierWriter = {};

        barrierWriter
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            m_cullingStatisticsBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = m_cullingStatisticsBuffer.size
            }
        )
        .Execute(cmdBuffer);

        const auto& pipeline = pipelineManager.GetPipeline("Debug/Culling/GenerateStatistics");

        pipeline.Bind(cmdBuffer);

        const auto constants = Culling::Constants
        {
            .Meshes                                      = meshBuffer.GetCurrentMeshBuffer(frameIndex).deviceAddress,
            .Instances                                   = meshBuffer.GetCurrentInstanceBuffer(frameIndex).deviceAddress,
            .CulledOpaqueDrawCalls                       = indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer.deviceAddress,
            .CulledOpaqueInstanceIndices                 = indirectBuffer.frustumCulledBuffers.opaqueBuffer.instanceIndexBuffer.deviceAddress,
            .CulledOpaqueDoubleSidedDrawCalls            = indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer.deviceAddress,
            .CulledOpaqueDoubleSidedInstanceIndices      = indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.instanceIndexBuffer.deviceAddress,
            .CulledAlphaMaskedDrawCalls                  = indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer.deviceAddress,
            .CulledAlphaMaskedInstanceIndices            = indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.instanceIndexBuffer.deviceAddress,
            .CulledAlphaMaskedDoubleSidedDrawCalls       = indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer.deviceAddress,
            .CulledAlphaMaskedDoubleSidedInstanceIndices = indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.instanceIndexBuffer.deviceAddress,
            .CullingStatistics                           = m_cullingStatisticsBuffer.deviceAddress
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        vkCmdDispatch
        (
            cmdBuffer.handle,
            1,
            1,
            1
        );

        barrierWriter
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            m_cullingStatisticsBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = m_cullingStatisticsBuffer.size
            }
        )
        .Execute(cmdBuffer);

        constexpr VkBufferCopy2 copyRegion =
        {
            .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .pNext     = nullptr,
            .srcOffset = 0,
            .dstOffset = 0,
            .size      = sizeof(Culling::CullingStatisticsBuffer)
        };

        const VkCopyBufferInfo2 copyInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext       = nullptr,
            .srcBuffer   = m_cullingStatisticsBuffer.handle,
            .dstBuffer   = m_cullingStatisticsReadbackBuffers[FIF].handle,
            .regionCount = 1,
            .pRegions    = &copyRegion
        };

        vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::GenerateTiledLightingStatistics
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::TileLightIndexBuffer& tileLightIndexBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, "TiledLighting/GenerateStatistics", {0.7657f, 0.1149f, 0.3901f, 1.0f});

        m_tiledLightingStatisticsBuffer.Barrier(cmdBuffer, Vk::BufferBarrier{
            .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
            .srcAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
            .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
            .offset         = 0,
            .size           = m_tiledLightingStatisticsBuffer.size
        });

        const auto& pipeline = pipelineManager.GetPipeline("Debug/TiledLighting/GenerateStatistics");

        pipeline.Bind(cmdBuffer);

        const auto& tileDepths = framebufferManager.GetFramebuffer("TiledLighting/TileDepths");

        const auto constants = TiledLighting::Constants
        {
            .Scene            = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
            .TileLightIndices = tileLightIndexBuffer.buffer.deviceAddress,
            .Statistics       = m_tiledLightingStatisticsBuffer.deviceAddress,
            .TileCount        = glm::uvec2(tileDepths.image.width, tileDepths.image.height)
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        vkCmdDispatch
        (
            cmdBuffer.handle,
            1,
            1,
            1
        );

        m_tiledLightingStatisticsBuffer.Barrier(cmdBuffer, Vk::BufferBarrier{
            .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
            .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
            .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
            .offset         = 0,
            .size           = m_tiledLightingStatisticsBuffer.size
        });

        constexpr VkBufferCopy2 copyRegion =
        {
            .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .pNext     = nullptr,
            .srcOffset = 0,
            .dstOffset = 0,
            .size      = sizeof(TiledLighting::TiledLightingStatisticsBuffer)
        };

        const VkCopyBufferInfo2 copyInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext       = nullptr,
            .srcBuffer   = m_tiledLightingStatisticsBuffer.handle,
            .dstBuffer   = m_tiledLightingStatisticsReadbackBuffers[FIF].handle,
            .regionCount = 1,
            .pRegions    = &copyRegion
        };

        vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Destroy(VmaAllocator allocator, Vk::StagingPool& stagingPool)
    {
        m_aabbIndexBuffer.Destroy(allocator);
        m_aabbDrawCallBuffer.Destroy(allocator);

        m_sphereIndexBuffer.Destroy(allocator);
        m_sphereVertexBuffer.Destroy(allocator);

        m_coneIndexBuffer.Destroy(allocator);
        m_coneVertexBuffer.Destroy(allocator);

        m_cullingStatisticsBuffer.Destroy(allocator);
        m_tiledLightingStatisticsBuffer.Destroy(allocator);

        for (auto& buffer : m_cullingStatisticsReadbackBuffers)
        {
            buffer.Destroy(allocator);
        }

        for (auto& buffer : m_tiledLightingStatisticsReadbackBuffers)
        {
            buffer.Destroy(allocator);
        }

        if (m_pendingAABBIndexUpload.has_value())
        {
            stagingPool.Free(m_pendingAABBIndexUpload.value());

            m_pendingAABBIndexUpload = std::nullopt;
        }

        if (m_pendingSphereIndexUpload.has_value())
        {
            stagingPool.Free(m_pendingSphereIndexUpload.value());

            m_pendingSphereIndexUpload = std::nullopt;
        }

        if (m_pendingSphereVertexUpload.has_value())
        {
            stagingPool.Free(m_pendingSphereVertexUpload.value());

            m_pendingSphereVertexUpload = std::nullopt;
        }
    }
}