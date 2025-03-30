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
#include "ImmediateSubmit.h"
#include "Util/Maths.h"

namespace Vk
{
    void AccelerationStructure::BuildBottomLevelAS
    (
        const Vk::Context& context,
        Vk::CommandBufferAllocator& cmdBufferAllocator,
        const Models::ModelManager& modelManager,
        const std::span<const Renderer::RenderObject> renderObjects
    )
    {
        std::vector<VkTransformMatrixKHR> transforms = {};

        for (const auto& renderObject : renderObjects)
        {
            for (const auto& mesh : modelManager.GetModel(renderObject.modelID).meshes)
            {
                transforms.emplace_back(glm::vk_cast(mesh.transform));
            }
        }

        auto stagingBuffer = Vk::Buffer
        (
            context.allocator,
            transforms.size() * sizeof(VkTransformMatrixKHR),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy
        (
            stagingBuffer.allocationInfo.pMappedData,
            transforms.data(),
            transforms.size() * sizeof(VkTransformMatrixKHR)
        );

        if (!(stagingBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                context.allocator,
                stagingBuffer.allocation,
                0,
                transforms.size() * sizeof(VkTransformMatrixKHR)),
                "Failed to flush allocation!"
            );
        }

        auto transformBuffer = Vk::Buffer
        (
            context.allocator,
            transforms.size() * sizeof(VkTransformMatrixKHR),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO
        );

        transformBuffer.GetDeviceAddress(context.device);

        std::vector<VkAccelerationStructureKHR>                            blases         = {};
        std::vector<Vk::Buffer>                                            buffers        = {};
        std::vector<Vk::Buffer>                                            scratchBuffers = {};
        std::vector<VkAccelerationStructureBuildGeometryInfoKHR>           blasBuildInfos = {};
        std::list<std::vector<VkAccelerationStructureGeometryKHR>>         geometryList   = {};
        std::vector<std::vector<VkAccelerationStructureBuildRangeInfoKHR>> rangeMap       = {};
        std::vector<const VkAccelerationStructureBuildRangeInfoKHR*>       pRangeInfos    = {};

        usize lastOffset = 0;

        for (const auto & renderObject : renderObjects)
        {
            const auto& meshes = modelManager.GetModel(renderObject.modelID).meshes;

            std::vector<VkAccelerationStructureGeometryKHR>       geometries = {};
            std::vector<u32>                                      counts     = {};
            std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges     = {};

            for (usize j = 0; j < meshes.size(); ++j)
            {
                geometries.emplace_back(VkAccelerationStructureGeometryKHR{
                    .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                    .pNext        = nullptr,
                    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
                    .geometry     = {.triangles = {
                        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                        .pNext         = nullptr,
                        .vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
                        .vertexData    = {.deviceAddress = modelManager.geometryBuffer.positionBuffer.deviceAddress + meshes[j].positionInfo.offset * sizeof(glm::vec3)},
                        .vertexStride  = sizeof(glm::vec3),
                        .maxVertex     = meshes[j].positionInfo.count - 1,
                        .indexType     = VK_INDEX_TYPE_UINT32,
                        .indexData     = modelManager.geometryBuffer.indexBuffer.deviceAddress + meshes[j].indexInfo.offset * sizeof(Models::Index),
                        .transformData = {.deviceAddress = transformBuffer.deviceAddress + (lastOffset + j) * sizeof(VkTransformMatrixKHR)}
                    }},
                    .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR
                });

                counts.push_back(meshes[j].indexInfo.count / 3);

                ranges.emplace_back(VkAccelerationStructureBuildRangeInfoKHR{
                    .primitiveCount  = counts.back(),
                    .primitiveOffset = 0,
                    .firstVertex     = 0,
                    .transformOffset = 0
                });
            }

            geometryList.emplace_back(geometries);
            rangeMap.emplace_back(ranges);
            pRangeInfos.emplace_back(rangeMap.back().data());
            lastOffset += meshes.size();

            VkAccelerationStructureBuildGeometryInfoKHR blasBuildInfo =
            {
                .sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .pNext                    = nullptr,
                .type                     = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                .flags                    = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
                .mode                     = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
                .srcAccelerationStructure = VK_NULL_HANDLE,
                .dstAccelerationStructure = VK_NULL_HANDLE,
                .geometryCount            = static_cast<u32>(geometries.size()),
                .pGeometries              = geometryList.back().data(),
                .ppGeometries             = nullptr,
                .scratchData              = {}
            };

            VkAccelerationStructureBuildSizesInfoKHR blasBuildSizes = {};
            blasBuildSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

            vkGetAccelerationStructureBuildSizesKHR
            (
                context.device,
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &blasBuildInfo,
                counts.data(),
                &blasBuildSizes
            );

            const auto buffer = Vk::Buffer
            (
                context.allocator,
                blasBuildSizes.accelerationStructureSize,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
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
                context.device,
                &blasCreateInfo,
                nullptr,
                &blas),
                "Failed to create BLAS!"
            );

            auto scratchBuffer = Vk::Buffer
            (
                context.allocator,
                blasBuildSizes.buildScratchSize,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            );

            scratchBuffer.GetDeviceAddress(context.device);

            blasBuildInfo.dstAccelerationStructure = blas;
            blasBuildInfo.scratchData              = {.deviceAddress = scratchBuffer.deviceAddress};

            blases.emplace_back(blas);
            buffers.emplace_back(buffer);
            scratchBuffers.emplace_back(scratchBuffer);
            blasBuildInfos.emplace_back(blasBuildInfo);
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

        VkQueryPool queryPool = VK_NULL_HANDLE;
        Vk::CheckResult(vkCreateQueryPool(
            context.device,
            &queryPoolInfo,
            nullptr,
            &queryPool),
            "Failed to create query pool!"
        );

        Vk::ImmediateSubmit
        (
            context.device,
            context.graphicsQueue,
            cmdBufferAllocator,
            [&] (const Vk::CommandBuffer& cmdBuffer)
            {
                const VkBufferCopy2 transformCopyRegion =
                {
                    .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                    .pNext     = nullptr,
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size      = transforms.size() * sizeof(VkTransformMatrixKHR),
                };

                const VkCopyBufferInfo2 transformCopyInfo =
                {
                    .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                    .pNext       = nullptr,
                    .srcBuffer   = stagingBuffer.handle,
                    .dstBuffer   = transformBuffer.handle,
                    .regionCount = 1,
                    .pRegions    = &transformCopyRegion
                };

                vkCmdCopyBuffer2(cmdBuffer.handle, &transformCopyInfo);

                transformBuffer.Barrier
                (
                    cmdBuffer,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                    VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_SHADER_READ_BIT,
                    0,
                    transforms.size() * sizeof(VkTransformMatrixKHR)
                );

                vkCmdBuildAccelerationStructuresKHR
                (
                    cmdBuffer.handle,
                    blasBuildInfos.size(),
                    blasBuildInfos.data(),
                    pRangeInfos.data()
                );

                vkCmdResetQueryPool
                (
                    cmdBuffer.handle,
                    queryPool,
                    0,
                    queryPoolInfo.queryCount
                );

                vkCmdWriteAccelerationStructuresPropertiesKHR
                (
                    cmdBuffer.handle,
                    blases.size(),
                    blases.data(),
                    VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                    queryPool,
                    0
                );
            }
        );

        Vk::ImmediateSubmit
        (
            context.device,
            context.graphicsQueue,
            cmdBufferAllocator,
            [&] (const Vk::CommandBuffer& cmdBuffer)
            {
                std::vector<VkDeviceSize> compactedSizes = {};
                compactedSizes.resize(blases.size());

                Vk::CheckResult(vkGetQueryPoolResults(
                    context.device,
                    queryPool,
                    0,
                    compactedSizes.size(),
                    sizeof(VkDeviceSize) * compactedSizes.size(),
                    compactedSizes.data(),
                    sizeof(VkDeviceSize),
                    VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT),
                    "Failed to retrieve BLAS compacted size!"
                );

                bottomLevelASes.resize(blases.size());

                for (usize i = 0; i < blases.size(); ++i)
                {
                    bottomLevelASes[i].buffer = Vk::Buffer
                    (
                        context.allocator,
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
                        .buffer        = bottomLevelASes[i].buffer.handle,
                        .offset        = 0,
                        .size          = bottomLevelASes[i].buffer.requestedSize,
                        .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                        .deviceAddress = 0
                    };

                    Vk::CheckResult(vkCreateAccelerationStructureKHR(
                        context.device,
                        &compactedBlasCreateInfo,
                        nullptr,
                        &bottomLevelASes[i].as),
                        "Failed to create BLAS!"
                    );

                    const VkCopyAccelerationStructureInfoKHR copyInfo =
                    {
                        .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
                        .pNext = nullptr,
                        .src   = blases[i],
                        .dst   = bottomLevelASes[i].as,
                        .mode  = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
                    };

                    vkCmdCopyAccelerationStructureKHR(cmdBuffer.handle, &copyInfo);
                }
            }
        );

        stagingBuffer.Destroy(context.allocator);
        transformBuffer.Destroy(context.allocator);

        for (const auto as : blases)
        {
            vkDestroyAccelerationStructureKHR(context.device, as, nullptr);
        }

        for (auto& buffer : buffers)
        {
            buffer.Destroy(context.allocator);
        }

        for (auto& buffer : scratchBuffers)
        {
            buffer.Destroy(context.allocator);
        }

        vkDestroyQueryPool(context.device, queryPool, nullptr);

        for (usize i = 0; i < bottomLevelASes.size(); ++i)
        {
            const VkAccelerationStructureDeviceAddressInfoKHR blasDAInfo =
            {
                .sType                  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .pNext                  = nullptr,
                .accelerationStructure  = bottomLevelASes[i].as
            };

            bottomLevelASes[i].deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(context.device, &blasDAInfo);

            Vk::SetDebugName(context.device, bottomLevelASes[i].as,            fmt::format("BLAS/{}",       i));
            Vk::SetDebugName(context.device, bottomLevelASes[i].buffer.handle, fmt::format("BLASBuffer/{}", i));
        }
    }

    void AccelerationStructure::BuildTopLevelAS
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        const std::span<const Renderer::RenderObject> renderObjects
    )
    {
        Vk::BeginLabel(cmdBuffer, fmt::format("TLASBuild/FIF{}", FIF), {0.2117f, 0.8136f, 0.7313f, 1.0f});

        m_deletionQueues[FIF].FlushQueue();

        std::vector<VkAccelerationStructureInstanceKHR> instances = {};
        instances.reserve(renderObjects.size());

        for (usize i = 0; i < renderObjects.size(); ++i)
        {
            instances.emplace_back(VkAccelerationStructureInstanceKHR{
                .transform                              = glm::vk_cast(Maths::CreateTransformMatrix(
                    renderObjects[i].position,
                    renderObjects[i].rotation,
                    renderObjects[i].scale
                )),
                .instanceCustomIndex                    = static_cast<u32>(i),
                .mask                                   = 0xFF,
                .instanceShaderBindingTableRecordOffset = 0,
                .flags                                  = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR,
                .accelerationStructureReference         = bottomLevelASes[i].deviceAddress
            });
        }

        const VkDeviceSize instancesSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

        if (m_instanceBuffers[FIF].requestedSize < instancesSize)
        {
            m_deletionQueues[FIF].PushDeletor([allocator, oldBuffer = m_instanceBuffers[FIF]] () mutable
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
            VK_PIPELINE_STAGE_2_HOST_BIT,
            VK_ACCESS_2_HOST_WRITE_BIT,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
            0,
            instancesSize
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
            .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR
        };

        const u32 count = renderObjects.size();

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
            m_deletionQueues[FIF].PushDeletor([allocator, oldBuffer = topLevelASes[FIF].buffer] () mutable
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

        Vk::CheckResult(vkCreateAccelerationStructureKHR(
            device,
            &tlasCreateInfo,
            nullptr,
            &topLevelASes[FIF].as),
            "Failed to create TLAS!"
        );

        if (m_scratchBuffers[FIF].requestedSize < tlasBuildSizes.buildScratchSize)
        {
            m_deletionQueues[FIF].PushDeletor([allocator, oldBuffer = m_scratchBuffers[FIF]] () mutable
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

        tlasBuildInfo.dstAccelerationStructure = topLevelASes[FIF].as;
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
            .accelerationStructure  = topLevelASes[FIF].as
        };

        topLevelASes[FIF].deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &tlasDAInfo);

        Vk::SetDebugName(device, topLevelASes[FIF].as,            fmt::format("TLAS/{}",               FIF));
        Vk::SetDebugName(device, topLevelASes[FIF].buffer.handle, fmt::format("TLASBuffer/{}",         FIF));
        Vk::SetDebugName(device, m_instanceBuffers[FIF].handle,   fmt::format("TLASInstanceBuffer/{}", FIF));
        Vk::SetDebugName(device, m_scratchBuffers[FIF].handle,    fmt::format("TLASScratchBuffer/{}",  FIF));

        m_deletionQueues[FIF].PushDeletor([device, as = topLevelASes[FIF].as] () mutable
        {
            vkDestroyAccelerationStructureKHR(device, as, nullptr);
        });

        Vk::EndLabel(cmdBuffer);
    }

    void AccelerationStructure::Destroy(VkDevice device, VmaAllocator allocator)
    {
        for (auto& deletionQueue : m_deletionQueues)
        {
            deletionQueue.FlushQueue();
        }

        for (auto& blas : bottomLevelASes)
        {
            blas.buffer.Destroy(allocator);

            vkDestroyAccelerationStructureKHR(device, blas.as, nullptr);
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
        }
    }
}