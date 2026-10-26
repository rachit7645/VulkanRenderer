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

#include "VertexBuffer.h"

#include "DebugUtils.h"
#include "DebugUIShared.h"
#include "Util/Scope.h"

namespace Vk
{
    namespace Detail
    {
        template<typename T>
        requires GPU::IsVertexType<T>
        consteval VkBufferUsageFlags GetVertexBufferUsage()
        {
            if constexpr (std::is_same_v<T, GPU::Position>)
            {
                return VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            }
            else if constexpr (std::is_same_v<T, GPU::UV>)
            {
                return VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            }
            else if constexpr (std::is_same_v<T, GPU::Vertex>)
            {
                return VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            }
            else
            {
                static_assert(false, "Unsupported vertex type!");
            }

            return 0;
        }

        template<typename T>
        requires GPU::IsVertexType<T>
        consteval VkPipelineStageFlags2 GetVertexBufferPipelineStage()
        {
            if constexpr (std::is_same_v<T, GPU::Position>)
            {
                return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
            }
            else if constexpr (std::is_same_v<T, GPU::UV>)
            {
                return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
            }
            else if constexpr (std::is_same_v<T, GPU::Vertex>)
            {
                return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            }
            else
            {
                static_assert(false, "Unsupported vertex type!");
            }

            return 0;
        }

        template<typename T>
        requires GPU::IsVertexType<T>
        consteval VkAccessFlags2 GetVertexBufferAccessMask()
        {
            if constexpr (std::is_same_v<T, GPU::Position>)
            {
                return VK_ACCESS_2_SHADER_READ_BIT;
            }
            else if constexpr (std::is_same_v<T, GPU::UV>)
            {
                return VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            }
            else if constexpr (std::is_same_v<T, GPU::Vertex>)
            {
                return VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            }
            else
            {
                static_assert(false, "Unsupported vertex type!");
            }

            return 0;
        }
    }

    VertexBuffer::Allocation VertexBuffer::Allocate
    (
        u32 elementCount,
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        Engine::DeletionQueue& deletionQueue
    )
    {
        if (elementCount == 0)
        {
            Logger::Error("{}\n", "Can't allocate zero elements!");
        }

        const VkDeviceSize positionSize         = elementCount * sizeof(GPU::Position);
        const VkDeviceSize uvSize               = elementCount * sizeof(GPU::UV);
        const VkDeviceSize normalAndTangentSize = elementCount * sizeof(GPU::Vertex);

        const Vk::StagingMemoryBlock positionStagingMemoryBlock = stagingPool.Allocate
        (
            device,
            allocator,
            positionSize,
            alignof(GPU::Position)
        );

        const Vk::StagingMemoryBlock uvStagingMemoryBlock = stagingPool.Allocate
        (
            device,
            allocator,
            uvSize,
            alignof(GPU::UV)
        );

        const Vk::StagingMemoryBlock normalAndTangentStagingMemoryBlock = stagingPool.Allocate
        (
            device,
            allocator,
            normalAndTangentSize,
            alignof(GPU::Vertex)
        );

        deletionQueue.Push([&stagingPool, positionStagingMemoryBlock, uvStagingMemoryBlock, normalAndTangentStagingMemoryBlock] () mutable
        {
            stagingPool.Free(positionStagingMemoryBlock);
            stagingPool.Free(uvStagingMemoryBlock);
            stagingPool.Free(normalAndTangentStagingMemoryBlock);
        });

        const std::scoped_lock lock{m_mutex};

        VertexBuffer::Allocation allocation =
        {
            .position         = static_cast<GPU::Position*>(positionStagingMemoryBlock.hostAddress),
            .uv               = static_cast<GPU::UV*>(uvStagingMemoryBlock.hostAddress),
            .normalAndTangent = static_cast<GPU::Vertex*>(normalAndTangentStagingMemoryBlock.hostAddress),
            .info             = {}
        };

        MergeFreeBlocks();

        if (const auto block = TryToFindFreeBlock(elementCount); block.has_value())
        {
            allocation.info = block.value();
        }
        else
        {
            allocation.info = AppendAtEnd(elementCount);
        }

        m_pendingUploads.emplace_back(VertexBuffer::GeometryUpload
        {
            .info                          = allocation.info,
            .positionStagingBuffer         = positionStagingMemoryBlock.buffer,
            .positionSourceOffset          = positionStagingMemoryBlock.memoryBlock.offset,
            .uvStagingBuffer               = uvStagingMemoryBlock.buffer,
            .uvSourceOffset                = uvStagingMemoryBlock.memoryBlock.offset,
            .normalAndTangentStagingBuffer = normalAndTangentStagingMemoryBlock.buffer,
            .normalAndTangentSourceOffset  = normalAndTangentStagingMemoryBlock.memoryBlock.offset
        });

        count += elementCount;

        #ifdef ENGINE_PROFILE
        TracyAllocN(reinterpret_cast<void*>(allocation.info.offset * sizeof(GPU::Position)), allocation.info.count * sizeof(GPU::Position), "Vertex Buffer Sub-Allocator | Position");
        TracyAllocN(reinterpret_cast<void*>(allocation.info.offset * sizeof(GPU::UV)),       allocation.info.count * sizeof(GPU::UV),       "Vertex Buffer Sub-Allocator | UV");
        TracyAllocN(reinterpret_cast<void*>(allocation.info.offset * sizeof(GPU::Vertex)),   allocation.info.count * sizeof(GPU::Vertex),   "Vertex Buffer Sub-Allocator | Normal And Tangent");
        #endif

        return allocation;
    }

    void VertexBuffer::Free(const GPU::GeometryInfo& info)
    {
        const std::scoped_lock lock{m_mutex};

        if (info.count == 0)
        {
            Logger::Error("Block has a count of zero! [Offset={}]\n", info.offset);
        }

        if (std::ranges::contains(m_freeBlocks, info))
        {
            Logger::Error("Block already freed! [Offset={}] [Count={}]\n", info.offset, info.count);
        }

        const auto iter = std::ranges::find(m_usedBlocks, info);

        if (iter == m_usedBlocks.cend())
        {
            Logger::Error("Invalid block! [Offset={}] [Count={}]\n", info.offset, info.count);
        }

        m_usedBlocks.erase(iter);
        m_freeBlocks.emplace_back(info);

        if (count < info.count)
        {
            Logger::Warning("Suspicious free! [Offset={}] [Count={}]", info.offset, info.count);

            count = 0;
        }
        else
        {
            count -= info.count;
        }

        #ifdef ENGINE_PROFILE
        TracyFreeN(reinterpret_cast<void*>(info.offset * sizeof(GPU::Position)), "Vertex Buffer Sub-Allocator | Position");
        TracyFreeN(reinterpret_cast<void*>(info.offset * sizeof(GPU::UV)),       "Vertex Buffer Sub-Allocator | UV");
        TracyFreeN(reinterpret_cast<void*>(info.offset * sizeof(GPU::Vertex)),   "Vertex Buffer Sub-Allocator | Normal And Tangent");
        #endif
    }

    void VertexBuffer::Update
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        Scratch::Allocator& scratchAllocator,
        Engine::DeletionQueue& deletionQueue
    )
    {
        const std::scoped_lock lock{m_mutex};

        Vk::BeginLabel(cmdBuffer, "VertexBuffer::Update", {0.5882f, 0.9294f, 0.2118f, 1.0f});

        Resize
        (
            cmdBuffer,
            device,
            allocator,
            scratchAllocator,
            deletionQueue
        );

        if (m_pendingUploads.empty())
        {
            Vk::EndLabel(cmdBuffer);

            return;
        }

        auto barrierWriter = Vk::ScratchBarrierWriter(scratchAllocator);

        for (const auto& upload : m_pendingUploads)
        {
            barrierWriter
            .WriteBufferBarrier(
               positionBuffer,
               Vk::BufferBarrier{
                   .srcStageMask   = Detail::GetVertexBufferPipelineStage<GPU::Position>(),
                   .srcAccessMask  = Detail::GetVertexBufferAccessMask<GPU::Position>(),
                   .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                   .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                   .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .offset         = upload.info.offset * sizeof(GPU::Position),
                   .size           = upload.info.count  * sizeof(GPU::Position)
               }
            )
            .WriteBufferBarrier(
               uvBuffer,
               Vk::BufferBarrier{
                   .srcStageMask   = Detail::GetVertexBufferPipelineStage<GPU::UV>(),
                   .srcAccessMask  = Detail::GetVertexBufferAccessMask<GPU::UV>(),
                   .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                   .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                   .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .offset         = upload.info.offset * sizeof(GPU::UV),
                   .size           = upload.info.count  * sizeof(GPU::UV)
               }
            )
            .WriteBufferBarrier(
               normalAndTangentBuffer,
               Vk::BufferBarrier{
                   .srcStageMask   = Detail::GetVertexBufferPipelineStage<GPU::Vertex>(),
                   .srcAccessMask  = Detail::GetVertexBufferAccessMask<GPU::Vertex>(),
                   .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                   .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                   .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .offset         = upload.info.offset * sizeof(GPU::Vertex),
                   .size           = upload.info.count  * sizeof(GPU::Vertex)
               }
            );
        }

        barrierWriter.Execute(cmdBuffer);

        auto batchedPositionCopyRegions         = Scratch::CreateMap<VkBuffer, Scratch::Vector<VkBufferCopy2>>(scratchAllocator);
        auto batchedUVCopyRegions               = Scratch::CreateMap<VkBuffer, Scratch::Vector<VkBufferCopy2>>(scratchAllocator);
        auto batchedNormalAndTangentCopyRegions = Scratch::CreateMap<VkBuffer, Scratch::Vector<VkBufferCopy2>>(scratchAllocator);

        for (const auto& upload : m_pendingUploads)
        {
            auto positionIter         = batchedPositionCopyRegions.find(upload.positionStagingBuffer);
            auto uvIter               = batchedUVCopyRegions.find(upload.uvStagingBuffer);
            auto normalAndTangentIter = batchedNormalAndTangentCopyRegions.find(upload.normalAndTangentStagingBuffer);

            if (positionIter == batchedPositionCopyRegions.end())
            {
                positionIter = batchedPositionCopyRegions.emplace(upload.positionStagingBuffer, Scratch::CreateVector<VkBufferCopy2>(scratchAllocator)).first;
            }

            if (uvIter == batchedUVCopyRegions.end())
            {
                uvIter = batchedUVCopyRegions.emplace(upload.uvStagingBuffer, Scratch::CreateVector<VkBufferCopy2>(scratchAllocator)).first;
            }

            if (normalAndTangentIter == batchedNormalAndTangentCopyRegions.end())
            {
                normalAndTangentIter = batchedNormalAndTangentCopyRegions.emplace(upload.normalAndTangentStagingBuffer, Scratch::CreateVector<VkBufferCopy2>(scratchAllocator)).first;
            }

            positionIter->second.emplace_back(VkBufferCopy2{
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = upload.positionSourceOffset,
                .dstOffset = upload.info.offset * sizeof(GPU::Position),
                .size      = upload.info.count  * sizeof(GPU::Position)
            });

            uvIter->second.emplace_back(VkBufferCopy2{
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = upload.uvSourceOffset,
                .dstOffset = upload.info.offset * sizeof(GPU::UV),
                .size      = upload.info.count  * sizeof(GPU::UV)
            });

            normalAndTangentIter->second.emplace_back(VkBufferCopy2{
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = upload.normalAndTangentSourceOffset,
                .dstOffset = upload.info.offset * sizeof(GPU::Vertex),
                .size      = upload.info.count  * sizeof(GPU::Vertex)
            });
        }

        for (const auto& [buffer, copyRegions] : batchedPositionCopyRegions)
        {
            const VkCopyBufferInfo2 copyInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .pNext       = nullptr,
                .srcBuffer   = buffer,
                .dstBuffer   = positionBuffer.handle,
                .regionCount = static_cast<u32>(copyRegions.size()),
                .pRegions    = copyRegions.data()
            };

            vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);
        }

        for (const auto& [buffer, copyRegions] : batchedUVCopyRegions)
        {
            const VkCopyBufferInfo2 copyInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .pNext       = nullptr,
                .srcBuffer   = buffer,
                .dstBuffer   = uvBuffer.handle,
                .regionCount = static_cast<u32>(copyRegions.size()),
                .pRegions    = copyRegions.data()
            };

            vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);
        }

        for (const auto& [buffer, copyRegions] : batchedNormalAndTangentCopyRegions)
        {
            const VkCopyBufferInfo2 copyInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .pNext       = nullptr,
                .srcBuffer   = buffer,
                .dstBuffer   = normalAndTangentBuffer.handle,
                .regionCount = static_cast<u32>(copyRegions.size()),
                .pRegions    = copyRegions.data()
            };

            vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);
        }

        for (const auto& upload : m_pendingUploads)
        {
            barrierWriter
            .WriteBufferBarrier(
               positionBuffer,
               Vk::BufferBarrier{
                   .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                   .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                   .dstStageMask   = Detail::GetVertexBufferPipelineStage<GPU::Position>(),
                   .dstAccessMask  = Detail::GetVertexBufferAccessMask<GPU::Position>(),
                   .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .offset         = upload.info.offset * sizeof(GPU::Position),
                   .size           = upload.info.count  * sizeof(GPU::Position)
               }
            )
            .WriteBufferBarrier(
               uvBuffer,
               Vk::BufferBarrier{
                   .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                   .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                   .dstStageMask   = Detail::GetVertexBufferPipelineStage<GPU::UV>(),
                   .dstAccessMask  = Detail::GetVertexBufferAccessMask<GPU::UV>(),
                   .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .offset         = upload.info.offset * sizeof(GPU::UV),
                   .size           = upload.info.count  * sizeof(GPU::UV)
               }
            )
            .WriteBufferBarrier(
               normalAndTangentBuffer,
               Vk::BufferBarrier{
                   .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                   .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                   .dstStageMask   = Detail::GetVertexBufferPipelineStage<GPU::Vertex>(),
                   .dstAccessMask  = Detail::GetVertexBufferAccessMask<GPU::Vertex>(),
                   .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .offset         = upload.info.offset * sizeof(GPU::Vertex),
                   .size           = upload.info.count  * sizeof(GPU::Vertex)
               }
            );
        }

        barrierWriter.Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);

        m_pendingUploads.clear();
    }

    bool VertexBuffer::HasPendingUploads()
    {
        const std::scoped_lock lock{m_mutex};

        return !m_pendingUploads.empty() || m_resizeInfo.has_value();
    }

    void VertexBuffer::ImGuiDisplay(Scratch::Allocator& scratchAllocator)
    {
        const std::scoped_lock lock{m_mutex};

        if (ImGui::CollapsingHeader("Vertex Buffer"))
        {
            const usize totalBlockCount = m_usedBlocks.size() + m_freeBlocks.size();

            if (totalBlockCount == 0)
            {
                ImGui::TextColored(ImVec4{1.0f, 0.0f, 0.0f, 1.0f}, "No blocks allocated!");
            }
            else
            {
                DisplayMemoryMapAndStatistics(scratchAllocator);
            }
        }
    }

    void VertexBuffer::Destroy(VmaAllocator allocator)
    {
        positionBuffer.Destroy(allocator);
        uvBuffer.Destroy(allocator);
        normalAndTangentBuffer.Destroy(allocator);
    }

    void VertexBuffer::MergeFreeBlocks()
    {
        if (m_freeBlocks.size() <= 1)
        {
            return;
        }

        std::ranges::sort(m_freeBlocks, [] (const GPU::GeometryInfo& a, const GPU::GeometryInfo& b)
        {
            return a.offset < b.offset;
        });

        std::vector<GPU::GeometryInfo> mergedBlocks;

        GPU::GeometryInfo currentBlock = m_freeBlocks.front();

        for (auto iter = std::next(m_freeBlocks.begin()); iter != m_freeBlocks.end(); ++iter)
        {
            const auto& nextBlock = *iter;

            if (currentBlock.offset + currentBlock.count == nextBlock.offset)
            {
                currentBlock.count += nextBlock.count;
            }
            else
            {
                mergedBlocks.emplace_back(currentBlock);
                currentBlock = nextBlock;
            }
        }

        mergedBlocks.emplace_back(currentBlock);

        m_freeBlocks = std::move(mergedBlocks);
    }

    std::optional<GPU::GeometryInfo> VertexBuffer::TryToFindFreeBlock(u32 elementCount)
    {
        auto candidate = m_freeBlocks.end();

        for (auto iter = m_freeBlocks.begin(); iter != m_freeBlocks.end(); ++iter)
        {
            if (iter->count < elementCount)
            {
                continue;
            }

            // Perfectly sized block!
            if (iter->count == elementCount)
            {
                candidate = iter;

                break;
            }

            if (iter->count > elementCount)
            {
                if (candidate == m_freeBlocks.end())
                {
                    candidate = iter;

                    continue;
                }

                if (candidate->count > iter->count)
                {
                    candidate = iter;
                }
            }
        }

        if (candidate == m_freeBlocks.end())
        {
            return std::nullopt;
        }

        const GPU::GeometryInfo allocated =
        {
            .offset = candidate->offset,
            .count  = elementCount
        };

        const GPU::GeometryInfo remaining =
        {
            .offset = candidate->offset + allocated.count,
            .count  = candidate->count  - allocated.count
        };

        m_usedBlocks.emplace_back(allocated);
        m_freeBlocks.erase(candidate);

        if (remaining.count != 0)
        {
            m_freeBlocks.emplace_back(remaining);
        }

        return allocated;
    }

    GPU::GeometryInfo VertexBuffer::AppendAtEnd(u32 elementCount)
    {
        const auto OrderByOffset = [] (const GPU::GeometryInfo& a, const GPU::GeometryInfo& b)
        {
            return a.offset < b.offset;
        };

        const auto lastUsedIter = std::ranges::max_element(m_usedBlocks, OrderByOffset);
        const auto lastFreeIter = std::ranges::max_element(m_freeBlocks, OrderByOffset);

        const bool lastUsedValid = lastUsedIter != m_usedBlocks.end();
        const bool lastFreeValid = lastFreeIter != m_freeBlocks.end();

        // This also implies that lastFreeValid == true
        bool isLastBlockAFreeBlock = false;

        if (!lastUsedValid && lastFreeValid)
        {
            isLastBlockAFreeBlock = true;
        }
        else if (lastUsedValid && lastFreeValid)
        {
            isLastBlockAFreeBlock = OrderByOffset(*lastUsedIter, *lastFreeIter);
        }

        u32 appendOffset = 0;

        if (isLastBlockAFreeBlock)
        {
            appendOffset = lastFreeIter->offset;
        }
        else if (lastUsedValid)
        {
            appendOffset = lastUsedIter->offset + lastUsedIter->count;
        }

        constexpr f64 GROWTH_FACTOR = 1.2;

        const u32 newAllocatedCount = elementCount + static_cast<u32>(std::ceil(GROWTH_FACTOR * static_cast<f64>(appendOffset)));

        if (m_resizeInfo.has_value())
        {
            m_resizeInfo->requiredCapacity = newAllocatedCount;
        }
        else
        {
            m_resizeInfo = VertexBuffer::ResizeInfo
            {
                .requiredCapacity = newAllocatedCount,
                .blocksToCopy = m_usedBlocks
            };
        }

        const GPU::GeometryInfo allocated =
        {
            .offset = appendOffset,
            .count  = elementCount
        };

        const GPU::GeometryInfo remaining =
        {
            .offset = allocated.offset  + allocated.count,
            .count  = newAllocatedCount - (elementCount + appendOffset)
        };

        m_usedBlocks.emplace_back(allocated);

        if (isLastBlockAFreeBlock)
        {
            m_freeBlocks.erase(lastFreeIter);
        }

        if (remaining.count != 0)
        {
            m_freeBlocks.emplace_back(remaining);
        }

        return allocated;
    }

    void VertexBuffer::Resize
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        Scratch::Allocator& scratchAllocator,
        Engine::DeletionQueue& deletionQueue
    )
    {
        auto Reset = Util::MakeScopeGuard([this] ()
        {
            m_resizeInfo = std::nullopt;
        });

        if (!m_resizeInfo.has_value())
        {
            return;
        }

        auto oldPositionBuffer         = positionBuffer;
        auto oldUVBuffer               = uvBuffer;
        auto oldNormalAndTangentBuffer = normalAndTangentBuffer;

        deletionQueue.Push([allocator, oldPositionBuffer, oldUVBuffer, oldNormalAndTangentBuffer] () mutable
        {
            oldPositionBuffer.Destroy(allocator);
            oldUVBuffer.Destroy(allocator);
            oldNormalAndTangentBuffer.Destroy(allocator);
        });

        positionBuffer = Vk::Buffer
        (
            device,
            allocator,
            m_resizeInfo->requiredCapacity * sizeof(GPU::Position),
            alignof(GPU::Position),
            Detail::GetVertexBufferUsage<GPU::Position>(),
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        uvBuffer = Vk::Buffer
        (
            device,
            allocator,
            m_resizeInfo->requiredCapacity * sizeof(GPU::UV),
            alignof(GPU::UV),
            Detail::GetVertexBufferUsage<GPU::UV>(),
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        normalAndTangentBuffer = Vk::Buffer
        (
            device,
            allocator,
            m_resizeInfo->requiredCapacity * sizeof(GPU::Vertex),
            alignof(GPU::Vertex),
            Detail::GetVertexBufferUsage<GPU::Vertex>(),
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        if
        (
            oldPositionBuffer.handle == VK_NULL_HANDLE ||
            oldUVBuffer.handle == VK_NULL_HANDLE ||
            oldNormalAndTangentBuffer.handle == VK_NULL_HANDLE ||
            m_resizeInfo->blocksToCopy.empty() ||
            m_usedBlocks.empty()
        )
        {
            return;
        }

        auto positionCopyRegions         = Scratch::CreateVector<VkBufferCopy2>(scratchAllocator);
        auto uvCopyRegions               = Scratch::CreateVector<VkBufferCopy2>(scratchAllocator);
        auto normalAndTangentCopyRegions = Scratch::CreateVector<VkBufferCopy2>(scratchAllocator);

        auto barrierWriterOld = Vk::ScratchBarrierWriter(scratchAllocator);
        auto barrierWriterNew = Vk::ScratchBarrierWriter(scratchAllocator);

        for (const auto& block : m_resizeInfo->blocksToCopy)
        {
            if (!std::ranges::contains(m_usedBlocks, block))
            {
                continue;
            }

            const VkBufferCopy2 positionCopyRegion =
            {
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = block.offset * sizeof(GPU::Position),
                .dstOffset = block.offset * sizeof(GPU::Position),
                .size      = block.count  * sizeof(GPU::Position)
            };

            const VkBufferCopy2 uvCopyRegion =
            {
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = block.offset * sizeof(GPU::UV),
                .dstOffset = block.offset * sizeof(GPU::UV),
                .size      = block.count  * sizeof(GPU::UV)
            };

            const VkBufferCopy2 normalAndTangentCopyRegion =
            {
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = block.offset * sizeof(GPU::Vertex),
                .dstOffset = block.offset * sizeof(GPU::Vertex),
                .size      = block.count  * sizeof(GPU::Vertex)
            };

            positionCopyRegions.emplace_back(positionCopyRegion);
            uvCopyRegions.emplace_back(uvCopyRegion);
            normalAndTangentCopyRegions.emplace_back(normalAndTangentCopyRegion);

            barrierWriterOld
            .WriteBufferBarrier(
                oldPositionBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = Detail::GetVertexBufferPipelineStage<GPU::Position>(),
                    .srcAccessMask  = Detail::GetVertexBufferAccessMask<GPU::Position>(),
                    .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = positionCopyRegion.srcOffset,
                    .size           = positionCopyRegion.size
                }
            )
            .WriteBufferBarrier(
                oldUVBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = Detail::GetVertexBufferPipelineStage<GPU::UV>(),
                    .srcAccessMask  = Detail::GetVertexBufferAccessMask<GPU::UV>(),
                    .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = uvCopyRegion.srcOffset,
                    .size           = uvCopyRegion.size
                }
            )
            .WriteBufferBarrier(
                oldNormalAndTangentBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = Detail::GetVertexBufferPipelineStage<GPU::Vertex>(),
                    .srcAccessMask  = Detail::GetVertexBufferAccessMask<GPU::Vertex>(),
                    .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = normalAndTangentCopyRegion.srcOffset,
                    .size           = normalAndTangentCopyRegion.size
                }
            );

            barrierWriterNew
            .WriteBufferBarrier(
                positionBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = Detail::GetVertexBufferPipelineStage<GPU::Position>(),
                    .dstAccessMask  = Detail::GetVertexBufferAccessMask<GPU::Position>(),
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = positionCopyRegion.dstOffset,
                    .size           = positionCopyRegion.size
                }
            )
            .WriteBufferBarrier(
                uvBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = Detail::GetVertexBufferPipelineStage<GPU::UV>(),
                    .dstAccessMask  = Detail::GetVertexBufferAccessMask<GPU::UV>(),
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = uvCopyRegion.dstOffset,
                    .size           = uvCopyRegion.size
                }
            )
            .WriteBufferBarrier(
                normalAndTangentBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = Detail::GetVertexBufferPipelineStage<GPU::Vertex>(),
                    .dstAccessMask  = Detail::GetVertexBufferAccessMask<GPU::Vertex>(),
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = normalAndTangentCopyRegion.dstOffset,
                    .size           = normalAndTangentCopyRegion.size
                }
            );
        }

        if (positionCopyRegions.empty())
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "Vertex Buffer v2 -> Resize Copy", {0.3882f, 0.9294f, 0.2118f, 1.0f});

        barrierWriterOld.Execute(cmdBuffer);

        const VkCopyBufferInfo2 positionCopyInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext       = nullptr,
            .srcBuffer   = oldPositionBuffer.handle,
            .dstBuffer   = positionBuffer.handle,
            .regionCount = static_cast<u32>(positionCopyRegions.size()),
            .pRegions    = positionCopyRegions.data(),
        };

        vkCmdCopyBuffer2(cmdBuffer.handle, &positionCopyInfo);

        const VkCopyBufferInfo2 uvCopyInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext       = nullptr,
            .srcBuffer   = oldUVBuffer.handle,
            .dstBuffer   = uvBuffer.handle,
            .regionCount = static_cast<u32>(uvCopyRegions.size()),
            .pRegions    = uvCopyRegions.data(),
        };

        vkCmdCopyBuffer2(cmdBuffer.handle, &uvCopyInfo);

        const VkCopyBufferInfo2 normalAndTangentCopyInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext       = nullptr,
            .srcBuffer   = oldNormalAndTangentBuffer.handle,
            .dstBuffer   = normalAndTangentBuffer.handle,
            .regionCount = static_cast<u32>(normalAndTangentCopyRegions.size()),
            .pRegions    = normalAndTangentCopyRegions.data(),
        };

        vkCmdCopyBuffer2(cmdBuffer.handle, &normalAndTangentCopyInfo);

        barrierWriterNew.Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void VertexBuffer::DisplayMemoryMapAndStatistics(Scratch::Allocator& scratchAllocator)
    {
        constexpr f32 BLOCK_HEIGHT = 20.0f;

        constexpr f32 X_PADDING    = 15.0f;
        constexpr f32 Y_PADDING    = 5.0f;
        constexpr f32 MIN_X_EXTENT = 5.0f;
        constexpr f32 MAX_Y_EXTENT = 2.0f * Y_PADDING + BLOCK_HEIGHT;

        constexpr f32 MAX_WIDTH_FRACTION = 0.9f;

        constexpr u32 USED_COLOR    = IM_COL32(50,  110, 200, 255);
        constexpr u32 FREE_COLOR    = IM_COL32(200, 90,  50,  255);
        constexpr u32 OUTLINE_COLOR = IM_COL32(0,   0,   0,   150);

        constexpr f32 OUTLINE_THICKNESS = 1.1f;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImGui::Text("Memory Map");

        const ImVec2 origin       = ImGui::GetCursorScreenPos();
        const ImVec2 paddedOrigin = origin + ImVec2{X_PADDING, Y_PADDING};

        auto combinedUIBlocks = Scratch::CreateVector<Vk::UIGeometryInfo>(scratchAllocator);

        usize totalUsed = 0;
        usize totalFree = 0;

        // Combine and Sort
        {
            combinedUIBlocks.reserve(m_usedBlocks.size() + m_freeBlocks.size());

            for (const auto& block : m_usedBlocks)
            {
                combinedUIBlocks.emplace_back(Vk::UIGeometryInfo
                {
                    .info   = block,
                    .isFree = false
                });

                totalUsed += block.count;
            }

            for (const auto& block : m_freeBlocks)
            {
                combinedUIBlocks.emplace_back(Vk::UIGeometryInfo
                {
                    .info   = block,
                    .isFree = true
                });

                totalFree += block.count;
            }

            std::ranges::sort(combinedUIBlocks, [] (const auto& A, const auto& B)
            {
                return A.info.offset < B.info.offset;
            });
        }

        f32 elementsPerPixel = 1.0f;

        // Compute Canvas Size
        {
            const auto& lastCombinedBlock = combinedUIBlocks.back();

            const u32 totalAllocated = lastCombinedBlock.info.offset + lastCombinedBlock.info.count;

            const f32 targetWidth = MAX_WIDTH_FRACTION * viewport->WorkSize.x;
            const f32 usableWidth = std::max(targetWidth - 2.0f * X_PADDING, 1.0f);

            elementsPerPixel = static_cast<f32>(totalAllocated) / usableWidth;

            ImGui::Dummy(ImVec2{targetWidth, MAX_Y_EXTENT});
        }

        for (const auto& block : combinedUIBlocks)
        {
            const ImVec2 pMin =
            {
                paddedOrigin.x + static_cast<f32>(block.info.offset) / elementsPerPixel,
                paddedOrigin.y
            };

            const ImVec2 pMax =
            {
                paddedOrigin.x + std::max(static_cast<f32>(block.info.offset + block.info.count) / elementsPerPixel, MIN_X_EXTENT),
                paddedOrigin.y + BLOCK_HEIGHT
            };

            drawList->AddRectFilled
            (
                pMin,
                pMax,
                block.isFree ? FREE_COLOR : USED_COLOR
            );

            drawList->AddRect
            (
                pMin,
                pMax,
                OUTLINE_COLOR,
                0.0f,
                OUTLINE_THICKNESS
            );

            if (ImGui::IsMouseHoveringRect(pMin, pMax))
            {
                ImGui::SetTooltip("%s | Offset=%u | Count=%u", block.isFree ? "Free" : "Used", block.info.offset, block.info.count);
            }
        }

        if (ImGui::BeginTable("##VertexBufferSubAllocatorStatisticsTable", 7, ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Block Count");
            ImGui::TableSetupColumn("Element Count");
            ImGui::TableSetupColumn("Position Bytes");
            ImGui::TableSetupColumn("UV Bytes");
            ImGui::TableSetupColumn("Normal and Tangent Bytes");
            ImGui::TableSetupColumn("Percentage");
            ImGui::TableHeadersRow();

            const usize totalElements = totalUsed + totalFree;

            const usize usedPositionBytes         = totalUsed * sizeof(GPU::Position);
            const usize usedUVBytes               = totalUsed * sizeof(GPU::UV);
            const usize usedNormalAndTangentBytes = totalUsed * sizeof(GPU::Vertex);

            const usize freePositionBytes         = totalFree * sizeof(GPU::Position);
            const usize freeUVBytes               = totalFree * sizeof(GPU::UV);
            const usize freeNormalAndTangentBytes = totalFree * sizeof(GPU::Vertex);

            const usize allocatedPositionBytes         = totalElements * sizeof(GPU::Position);
            const usize allocatedUVBytes               = totalElements * sizeof(GPU::UV);
            const usize allocatedNormalAndTangentBytes = totalElements * sizeof(GPU::Vertex);

            const f64 usedFraction = static_cast<f64>(totalUsed) / static_cast<f64>(totalElements);
            const f64 freeFraction = static_cast<f64>(totalFree) / static_cast<f64>(totalElements);

            constexpr usize TOTAL_SIZE_PER_ELEMENT = sizeof(GPU::Position) + sizeof(GPU::UV) + sizeof(GPU::Vertex);

            const f64 allocatedFraction = static_cast<f64>(TOTAL_SIZE_PER_ELEMENT * totalElements) /
                                          static_cast<f64>(positionBuffer.size + uvBuffer.size + normalAndTangentBuffer.size);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Used");
            ImGui::TableNextColumn();
            ImGui::Text("%llu", m_usedBlocks.size());
            ImGui::TableNextColumn();
            ImGui::Text("%llu", totalUsed);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", usedPositionBytes);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", usedUVBytes);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", usedNormalAndTangentBytes);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f%%", 100.0 * usedFraction);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Free");
            ImGui::TableNextColumn();
            ImGui::Text("%llu", m_freeBlocks.size());
            ImGui::TableNextColumn();
            ImGui::Text("%llu", totalFree);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", freePositionBytes);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", freeUVBytes);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", freeNormalAndTangentBytes);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f%%", 100.0 * freeFraction);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Allocated");
            ImGui::TableNextColumn();
            ImGui::Text("%llu", m_usedBlocks.size() + m_freeBlocks.size());
            ImGui::TableNextColumn();
            ImGui::Text("%llu", totalElements);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", allocatedPositionBytes);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", allocatedUVBytes);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", allocatedNormalAndTangentBytes);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f%%", 100.0 * allocatedFraction);

            ImGui::EndTable();
        }
    }
}
