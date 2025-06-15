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

#include "AccelerationStructure.h"

#include "DebugUtils.h"
#include "Util.h"
#include "GPU/Vertex.h"
#include "Util/Maths.h"

namespace Vk
{
    void AccelerationStructure::BuildBottomLevelAS
    (
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        const Models::ModelManager& modelManager,
        const std::span<const Renderer::RenderObject> renderObjects,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (renderObjects.empty() || !bottomLevelASes.empty())
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "BLAS Build", {0.7117f, 0.8136f, 0.7313f, 1.0f});

        std::vector<VkTransformMatrixKHR> transforms = {};

        for (const auto& renderObject : renderObjects)
        {
            for (const auto& mesh : modelManager.GetModel(renderObject.modelID).meshes)
            {
                transforms.emplace_back(glm::vk_cast(mesh.transform));
            }
        }

        const VkDeviceSize transformsSize = transforms.size() * sizeof(VkTransformMatrixKHR);

        auto transformBuffer = Vk::Buffer
        (
            allocator,
            transformsSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        transformBuffer.GetDeviceAddress(device);

        std::memcpy
        (
            transformBuffer.allocationInfo.pMappedData,
            transforms.data(),
            transformsSize
        );

        if (!(transformBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                transformBuffer.allocation,
                0,
                transformsSize),
                "Failed to flush allocation!"
            );
        }

        transformBuffer.Barrier
        (
            cmdBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_HOST_BIT,
                .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                .offset        = 0,
                .size          = transformsSize
            }
        );

        deletionQueue.PushDeletor([allocator, buffer = transformBuffer] () mutable
        {
            buffer.Destroy(allocator);
        });

        std::vector<VkAccelerationStructureKHR>                      blases         = {};
        std::vector<Vk::Buffer>                                      buffers        = {};
        std::vector<Vk::Buffer>                                      scratchBuffers = {};
        std::vector<VkAccelerationStructureBuildGeometryInfoKHR>     blasBuildInfos = {};
        std::list<VkAccelerationStructureGeometryKHR>                geometryList   = {};
        std::list<VkAccelerationStructureBuildRangeInfoKHR>          rangeMap       = {};
        std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> pRangeInfos    = {};

        for (usize meshIndex = 0; const auto& renderObject : renderObjects)
        {
            for (const auto& mesh : modelManager.GetModel(renderObject.modelID).meshes)
            {
                VkGeometryFlagsKHR geometryFlags = 0;

                // Skip any hit shaders for non-alpha masked meshes
                if (!mesh.material.IsAlphaMasked())
                {
                    geometryFlags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
                }

                geometryList.emplace_back(VkAccelerationStructureGeometryKHR{
                    .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                    .pNext        = nullptr,
                    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                    .geometry     = {.triangles = {
                        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                        .pNext         = nullptr,
                        .vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
                        .vertexData    = {.deviceAddress = modelManager.geometryBuffer.GetPositionBuffer().deviceAddress + mesh.surfaceInfo.positionInfo.offset * sizeof(GPU::Position)},
                        .vertexStride  = sizeof(GPU::Position),
                        .maxVertex     = mesh.surfaceInfo.positionInfo.count - 1,
                        .indexType     = VK_INDEX_TYPE_UINT32,
                        .indexData     = modelManager.geometryBuffer.GetIndexBuffer().deviceAddress + mesh.surfaceInfo.indexInfo.offset * sizeof(GPU::Index),
                        .transformData = {.deviceAddress = transformBuffer.deviceAddress + meshIndex * sizeof(VkTransformMatrixKHR)}
                    }},
                    .flags        = geometryFlags
                });

                const auto count = mesh.surfaceInfo.indexInfo.count / 3;

                rangeMap.emplace_back(VkAccelerationStructureBuildRangeInfoKHR{
                    .primitiveCount  = count,
                    .primitiveOffset = 0,
                    .firstVertex     = 0,
                    .transformOffset = 0
                });

                pRangeInfos.emplace_back(&rangeMap.back());

                VkAccelerationStructureBuildGeometryInfoKHR blasBuildInfo =
                {
                    .sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                    .pNext                    = nullptr,
                    .type                     = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                    .flags                    = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
                    .mode                     = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
                    .srcAccelerationStructure = VK_NULL_HANDLE,
                    .dstAccelerationStructure = VK_NULL_HANDLE,
                    .geometryCount            = 1,
                    .pGeometries              = &geometryList.back(),
                    .ppGeometries             = nullptr,
                    .scratchData              = {}
                };

                VkAccelerationStructureBuildSizesInfoKHR blasBuildSizes = {};
                blasBuildSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

                vkGetAccelerationStructureBuildSizesKHR
                (
                    device,
                    VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                    &blasBuildInfo,
                    &count,
                    &blasBuildSizes
                );

                const auto buffer = Vk::Buffer
                (
                    allocator,
                    blasBuildSizes.accelerationStructureSize,
                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    0,
                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
                );

                const VkAccelerationStructureCreateInfoKHR blasCreateInfo =
                {
                    .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                    .pNext         = nullptr,
                    .createFlags   = 0,
                    .buffer        = buffer.handle,
                    .offset        = 0,
                    .size          = buffer.requestedSize,
                    .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                    .deviceAddress = 0
                };

                VkAccelerationStructureKHR blas = VK_NULL_HANDLE;
                Vk::CheckResult(vkCreateAccelerationStructureKHR(
                    device,
                    &blasCreateInfo,
                    nullptr,
                    &blas),
                    "Failed to create BLAS!"
                );

                auto scratchBuffer = Vk::Buffer
                (
                    allocator,
                    blasBuildSizes.buildScratchSize,
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    0,
                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
                );

                scratchBuffer.GetDeviceAddress(device);

                blasBuildInfo.dstAccelerationStructure = blas;
                blasBuildInfo.scratchData              = {.deviceAddress = scratchBuffer.deviceAddress};

                blases.emplace_back(blas);
                buffers.emplace_back(buffer);
                scratchBuffers.emplace_back(scratchBuffer);
                blasBuildInfos.emplace_back(blasBuildInfo);

                deletionQueue.PushDeletor([allocator, buffer = scratchBuffer] () mutable
                {
                    buffer.Destroy(allocator);
                });

                // On to the next one!
                ++meshIndex;
            }
        }

        vkCmdBuildAccelerationStructuresKHR
        (
            cmdBuffer.handle,
            blasBuildInfos.size(),
            blasBuildInfos.data(),
            pRangeInfos.data()
        );

        if (m_compactionQueryPool != VK_NULL_HANDLE)
        {
            deletionQueue.PushDeletor([device, queryPool = m_compactionQueryPool] ()
            {
                vkDestroyQueryPool(device, queryPool, nullptr);
            });
        }

        const VkQueryPoolCreateInfo queryPoolInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .pNext              = nullptr,
            .flags              = 0,
            .queryType          = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            .queryCount         = static_cast<u32>(blases.size()),
            .pipelineStatistics = 0
        };

        Vk::CheckResult(vkCreateQueryPool(
            device,
            &queryPoolInfo,
            nullptr,
            &m_compactionQueryPool),
            "Failed to create query pool!"
        );

        vkCmdResetQueryPool
        (
            cmdBuffer.handle,
            m_compactionQueryPool,
            0,
            queryPoolInfo.queryCount
        );

        vkCmdWriteAccelerationStructuresPropertiesKHR
        (
            cmdBuffer.handle,
            blases.size(),
            blases.data(),
            VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
            m_compactionQueryPool,
            0
        );

        for (usize i = 0; i < blases.size(); ++i)
        {
            auto& blas = bottomLevelASes.emplace_back(blases[i], buffers[i], 0);

            const VkAccelerationStructureDeviceAddressInfoKHR blasDAInfo =
            {
                .sType                  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .pNext                  = nullptr,
                .accelerationStructure  = blas.handle
            };

            blas.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &blasDAInfo);

            Vk::SetDebugName(device, blas.handle,            fmt::format("BLAS/{}",       i));
            Vk::SetDebugName(device, blas.buffer.handle, fmt::format("BLASBuffer/{}", i));
        }

        m_initialBLASBuildFrameIndex = frameIndex;

        Vk::EndLabel(cmdBuffer);
    }

    void AccelerationStructure::TryCompactBottomLevelAS
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        const Vk::Timeline& timeline,
        Util::DeletionQueue& deletionQueue
    )
    {
        if
        (
            m_compactionQueryPool == VK_NULL_HANDLE ||
            m_initialBLASBuildFrameIndex == std::numeric_limits<usize>::max() ||
            bottomLevelASes.empty()
        )
        {
            return;
        }

        // Wait Vk::FRAMES_IN_FLIGHT frames before trying to retrieve query results (Thanks Darian and devsh!)
        const bool isQueryReady = timeline.IsAtOrPastState
        (
            m_initialBLASBuildFrameIndex + Vk::FRAMES_IN_FLIGHT,
            Vk::Timeline::TIMELINE_STAGE_SWAPCHAIN_IMAGE_ACQUIRED,
            device
        );

        if (!isQueryReady)
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "BLAS Compaction", {0.2117f, 0.4136f, 0.7313f, 1.0f});

        std::vector<VkDeviceSize> compactedSizes = {};
        compactedSizes.resize(bottomLevelASes.size());

        const auto result = vkGetQueryPoolResults
        (
            device,
            m_compactionQueryPool,
            0,
            compactedSizes.size(),
            sizeof(VkDeviceSize) * compactedSizes.size(),
            compactedSizes.data(),
            sizeof(VkDeviceSize),
            VK_QUERY_RESULT_64_BIT
        );

        if (result == VK_NOT_READY)
        {
            return;
        }

        Vk::CheckResult(result, "Failed to retrieve BLAS compacted sizes!");

        for (usize i = 0; i < bottomLevelASes.size(); ++i)
        {
            auto& blas = bottomLevelASes[i];

            auto oldBLAS   = blas.handle;
            auto oldBuffer = blas.buffer;

            deletionQueue.PushDeletor([device, allocator, oldBLAS, oldBuffer] () mutable
            {
                vkDestroyAccelerationStructureKHR(device, oldBLAS, nullptr);

                oldBuffer.Destroy(allocator);
            });

            blas.buffer = Vk::Buffer
            (
                allocator,
                compactedSizes[i],
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            );

            const VkAccelerationStructureCreateInfoKHR compactedBlasCreateInfo =
            {
                .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
                .pNext         = nullptr,
                .createFlags   = 0,
                .buffer        = blas.buffer.handle,
                .offset        = 0,
                .size          = blas.buffer.requestedSize,
                .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                .deviceAddress = 0
            };

            Vk::CheckResult(vkCreateAccelerationStructureKHR(
                device,
                &compactedBlasCreateInfo,
                nullptr,
                &blas.handle),
                "Failed to create BLAS!"
            );

            const VkCopyAccelerationStructureInfoKHR copyInfo =
            {
                .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
                .pNext = nullptr,
                .src   = oldBLAS,
                .dst   = blas.handle,
                .mode  = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
            };

            vkCmdCopyAccelerationStructureKHR(cmdBuffer.handle, &copyInfo);

            const VkAccelerationStructureDeviceAddressInfoKHR blasDAInfo =
            {
                .sType                  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .pNext                  = nullptr,
                .accelerationStructure  = blas.handle
            };

            blas.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &blasDAInfo);

            Vk::SetDebugName(device, blas.handle,            fmt::format("BLAS/Compacted/{}",       i));
            Vk::SetDebugName(device, blas.buffer.handle, fmt::format("BLASBuffer/Compacted/{}", i));
        }

        vkDestroyQueryPool(device, m_compactionQueryPool, nullptr);

        m_compactionQueryPool = VK_NULL_HANDLE;

        m_initialBLASBuildFrameIndex = std::numeric_limits<usize>::max();

        Vk::EndLabel(cmdBuffer);
    }

    void AccelerationStructure::BuildTopLevelAS
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        const Models::ModelManager& modelManager,
        const std::span<const Renderer::RenderObject> renderObjects,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (renderObjects.empty() || bottomLevelASes.empty())
        {
            deletionQueue.PushDeletor([device, as = topLevelASes[FIF].handle] () mutable
            {
                if (as != VK_NULL_HANDLE)
                {
                    vkDestroyAccelerationStructureKHR(device, as, nullptr);
                }
            });

            topLevelASes[FIF] = {};

            return;
        }

        Vk::BeginLabel(cmdBuffer, "TLAS Build", {0.2117f, 0.8136f, 0.7313f, 1.0f});

        std::vector<VkAccelerationStructureInstanceKHR> instances = {};

        for (usize meshIndex = 0; const auto& renderObject : renderObjects)
        {
            for (const auto& mesh : modelManager.GetModel(renderObject.modelID).meshes)
            {
                VkGeometryInstanceFlagsKHR instanceFlags = 0;

                if (mesh.material.IsAlphaMasked())
                {
                    instanceFlags |= VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR;
                }
                else
                {
                    instanceFlags |= VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
                }

                if (mesh.material.IsDoubleSided())
                {
                    instanceFlags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                }

                instances.emplace_back(VkAccelerationStructureInstanceKHR{
                    .transform                              = glm::vk_cast(Maths::TransformMatrix(
                        renderObject.position,
                        renderObject.rotation,
                        renderObject.scale
                    )),
                    .instanceCustomIndex                    = static_cast<u32>(meshIndex),
                    .mask                                   = 0xFF,
                    .instanceShaderBindingTableRecordOffset = 0,
                    .flags                                  = instanceFlags,
                    .accelerationStructureReference         = bottomLevelASes[meshIndex].deviceAddress
                });

                ++meshIndex;
            }
        }

        const VkDeviceSize instancesSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

        if (m_instanceBuffers[FIF].requestedSize < instancesSize)
        {
            deletionQueue.PushDeletor([allocator, oldBuffer = m_instanceBuffers[FIF]] () mutable
            {
                oldBuffer.Destroy(allocator);
            });

            m_instanceBuffers[FIF] = Vk::Buffer
            (
                allocator,
                instancesSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            m_instanceBuffers[FIF].GetDeviceAddress(device);
        }

        std::memcpy
        (
            m_instanceBuffers[FIF].allocationInfo.pMappedData,
            instances.data(),
            instancesSize
        );

        if (!(m_instanceBuffers[FIF].memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                m_instanceBuffers[FIF].allocation,
                0,
                instancesSize),
                "Failed to flush allocation!"
            );
        }

        m_instanceBuffers[FIF].Barrier
        (
            cmdBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_HOST_BIT,
                .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                .offset        = 0,
                .size          = instancesSize
            }
        );

        const VkAccelerationStructureGeometryKHR geometry =
        {
            .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .pNext        = nullptr,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry     = {.instances = {
                .sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .pNext           = nullptr,
                .arrayOfPointers = VK_FALSE,
                .data            = {.deviceAddress = m_instanceBuffers[FIF].deviceAddress}
            }},
            .flags        = 0
        };

        const u32 count = instances.size();

        const VkAccelerationStructureBuildRangeInfoKHR range =
        {
            .primitiveCount  = count,
            .primitiveOffset = 0,
            .firstVertex     = 0,
            .transformOffset = 0
        };

        VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo =
        {
            .sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .pNext                    = nullptr,
            .type                     = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .flags                    = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode                     = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .srcAccelerationStructure = VK_NULL_HANDLE,
            .dstAccelerationStructure = VK_NULL_HANDLE,
            .geometryCount            = 1,
            .pGeometries              = &geometry,
            .ppGeometries             = nullptr,
            .scratchData              = {}
        };

        VkAccelerationStructureBuildSizesInfoKHR tlasBuildSizes = {};
        tlasBuildSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

        vkGetAccelerationStructureBuildSizesKHR
        (
            device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &tlasBuildInfo,
            &count,
            &tlasBuildSizes
        );

        if (topLevelASes[FIF].buffer.requestedSize < tlasBuildSizes.accelerationStructureSize)
        {
            deletionQueue.PushDeletor([allocator, oldBuffer = topLevelASes[FIF].buffer] () mutable
            {
                oldBuffer.Destroy(allocator);
            });

            topLevelASes[FIF].buffer = Vk::Buffer
            (
                allocator,
                tlasBuildSizes.accelerationStructureSize,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            );
        }

        const VkAccelerationStructureCreateInfoKHR tlasCreateInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .pNext         = nullptr,
            .createFlags   = 0,
            .buffer        = topLevelASes[FIF].buffer.handle,
            .offset        = 0,
            .size          = topLevelASes[FIF].buffer.requestedSize,
            .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .deviceAddress = 0
        };

        deletionQueue.PushDeletor([device, as = topLevelASes[FIF].handle] () mutable
        {
            if (as != VK_NULL_HANDLE)
            {
                vkDestroyAccelerationStructureKHR(device, as, nullptr);
            }
        });

        Vk::CheckResult(vkCreateAccelerationStructureKHR(
            device,
            &tlasCreateInfo,
            nullptr,
            &topLevelASes[FIF].handle),
            "Failed to create TLAS!"
        );

        if (m_scratchBuffers[FIF].requestedSize < tlasBuildSizes.buildScratchSize)
        {
            deletionQueue.PushDeletor([allocator, oldBuffer = m_scratchBuffers[FIF]] () mutable
            {
                oldBuffer.Destroy(allocator);
            });

            m_scratchBuffers[FIF] = Vk::Buffer
            (
                allocator,
                tlasBuildSizes.buildScratchSize,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            );

            m_scratchBuffers[FIF].GetDeviceAddress(device);
        }

        tlasBuildInfo.dstAccelerationStructure = topLevelASes[FIF].handle;
        tlasBuildInfo.scratchData              = {.deviceAddress = m_scratchBuffers[FIF].deviceAddress};

        const std::array pRangeInfos = {&range};

        vkCmdBuildAccelerationStructuresKHR
        (
            cmdBuffer.handle,
            1,
            &tlasBuildInfo,
            pRangeInfos.data()
        );

        const VkAccelerationStructureDeviceAddressInfoKHR tlasDAInfo =
        {
            .sType                  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .pNext                  = nullptr,
            .accelerationStructure  = topLevelASes[FIF].handle
        };

        topLevelASes[FIF].deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &tlasDAInfo);

        Vk::SetDebugName(device, topLevelASes[FIF].handle,            fmt::format("TLAS/{}",               FIF));
        Vk::SetDebugName(device, topLevelASes[FIF].buffer.handle, fmt::format("TLASBuffer/{}",         FIF));
        Vk::SetDebugName(device, m_instanceBuffers[FIF].handle,   fmt::format("TLASInstanceBuffer/{}", FIF));
        Vk::SetDebugName(device, m_scratchBuffers[FIF].handle,    fmt::format("TLASScratchBuffer/{}",  FIF));

        Vk::EndLabel(cmdBuffer);
    }

    void AccelerationStructure::Destroy(VkDevice device, VmaAllocator allocator)
    {
        for (auto& blas : bottomLevelASes)
        {
            blas.buffer.Destroy(allocator);

            if (blas.handle != VK_NULL_HANDLE)
            {
                vkDestroyAccelerationStructureKHR(device, blas.handle, nullptr);
            }
        }

        for (auto& instanceBuffer : m_instanceBuffers)
        {
            instanceBuffer.Destroy(allocator);
        }

        for (auto& scratchBuffer : m_scratchBuffers)
        {
            scratchBuffer.Destroy(allocator);
        }

        for (auto& tlas : topLevelASes)
        {
            tlas.buffer.Destroy(allocator);

            if (tlas.handle != VK_NULL_HANDLE)
            {
                vkDestroyAccelerationStructureKHR(device, tlas.handle, nullptr);
            }
        }

        if (m_compactionQueryPool != VK_NULL_HANDLE)
        {
            vkDestroyQueryPool(device, m_compactionQueryPool, nullptr);
        }
    }
}