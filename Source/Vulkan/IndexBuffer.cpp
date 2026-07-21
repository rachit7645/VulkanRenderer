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

#include "IndexBuffer.h"

#include <ranges>
#include <volk/volk.h>

#include "DebugUtils.h"
#include "DebugUIShared.h"
#include "Util/Log.h"
#include "Util/Scope.h"

namespace Vk
{
    constexpr VkBufferUsageFlags INDEX_BUFFER_USAGE = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

    constexpr VkPipelineStageFlags2 INDEX_BUFFER_STAGE_MASK = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT |
                                                              VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
                                                              VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;

    constexpr VkAccessFlags2 INDEX_BUFFER_ACCESS_MASK = VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;

    void IndexBuffer::Bind(const Vk::CommandBuffer& cmdBuffer) const
    {
        vkCmdBindIndexBuffer
        (
            cmdBuffer.handle,
            buffer.handle,
            0,
            VK_INDEX_TYPE_UINT32
        );
    }

    void IndexBuffer::Destroy(VmaAllocator allocator)
    {
        buffer.Destroy(allocator);
    }

    IndexBuffer::Allocation IndexBuffer::Allocate
    (
        VkDevice device,
        VmaAllocator allocator,
        usize writeCount,
        Vk::StagingPool& stagingPool,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (writeCount == 0)
        {
            Logger::Error("{}\n", "Can't allocate zero elements!");
        }

        const VkDeviceSize writeSize = writeCount * sizeof(GPU::Index);

        const auto stagingMemoryBlock = stagingPool.Allocate
        (
            device,
            allocator,
            writeSize,
            alignof(GPU::Index)
        );

        deletionQueue.Push([&stagingPool, stagingMemoryBlock] () mutable
        {
            stagingPool.Free(stagingMemoryBlock);
        });

        const std::scoped_lock lock{m_mutex};

        GPU::GeometryInfo info = {};

        MergeFreeBlocks();

        if (const auto block = TryToFindFreeBlock(writeCount); block.has_value())
        {
            info = block.value();
        }
        else
        {
            info = AppendAtEnd(writeCount);
        }

        count += info.count;

        auto iter = m_pendingUploads.find(stagingMemoryBlock.buffer);

        if (iter == m_pendingUploads.end())
        {
            iter = m_pendingUploads.emplace(stagingMemoryBlock.buffer, std::vector<GeometryUpload>{}).first;
        }

        iter->second.emplace_back(info, stagingMemoryBlock.memoryBlock.offset);

        return IndexBuffer::Allocation
        {
            .index = static_cast<GPU::Index*>(stagingMemoryBlock.hostAddress),
            .info  = info
        };
    }

    void IndexBuffer::Free(const GPU::GeometryInfo& info)
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
    }

    void IndexBuffer::Update
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue
    )
    {
        const std::scoped_lock lock{m_mutex};

        Vk::BeginLabel(cmdBuffer, "IndexBuffer::Update", {0.5882f, 0.9294f, 0.2118f, 1.0f});

        Resize
        (
            cmdBuffer,
            device,
            allocator,
            deletionQueue
        );

        if (m_pendingUploads.empty())
        {
            return;
        }

        Vk::BarrierWriter barrierWriter = {};

        for (const auto& uploads : m_pendingUploads | std::views::values)
        {
            for (const auto& upload : uploads)
            {
                barrierWriter.WriteBufferBarrier
                (
                   buffer,
                   Vk::BufferBarrier{
                       .srcStageMask   = INDEX_BUFFER_STAGE_MASK,
                       .srcAccessMask  = INDEX_BUFFER_ACCESS_MASK,
                       .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                       .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                       .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                       .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                       .offset         = upload.info.offset * sizeof(GPU::Index),
                       .size           = upload.info.count  * sizeof(GPU::Index)
                   }
                );
            }
        }

        barrierWriter.Execute(cmdBuffer);

        for (const auto& [stagingBuffer, uploads] : m_pendingUploads)
        {
            std::vector<VkBufferCopy2> copyRegions = {};

            for (const auto& upload : uploads)
            {
                copyRegions.emplace_back(VkBufferCopy2{
                    .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                    .pNext     = nullptr,
                    .srcOffset = upload.sourceOffset,
                    .dstOffset = upload.info.offset * sizeof(GPU::Index),
                    .size      = upload.info.count  * sizeof(GPU::Index)
                });
            }

            const VkCopyBufferInfo2 copyInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .pNext       = nullptr,
                .srcBuffer   = stagingBuffer,
                .dstBuffer   = buffer.handle,
                .regionCount = static_cast<u32>(copyRegions.size()),
                .pRegions    = copyRegions.data()
            };

            vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);
        }

        for (const auto& uploads : m_pendingUploads | std::views::values)
        {
            for (const auto& upload : uploads)
            {
                barrierWriter.WriteBufferBarrier
                (
                    buffer,
                    Vk::BufferBarrier{
                        .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                        .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .dstStageMask   = INDEX_BUFFER_STAGE_MASK,
                        .dstAccessMask  = INDEX_BUFFER_ACCESS_MASK,
                        .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                        .offset         = upload.info.offset * sizeof(GPU::Index),
                        .size           = upload.info.count  * sizeof(GPU::Index)
                    }
                );
            }
        }

        barrierWriter.Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);

        m_pendingUploads.clear();
    }

    bool IndexBuffer::HasPendingUploads()
    {
        const std::scoped_lock lock{m_mutex};

        return !m_pendingUploads.empty();
    }

    void IndexBuffer::ImGuiDisplay()
    {
        const std::scoped_lock lock{m_mutex};

        if (ImGui::CollapsingHeader("Index Buffer"))
        {
            const usize totalBlockCount = m_usedBlocks.size() + m_freeBlocks.size();

            if (totalBlockCount == 0)
            {
                ImGui::TextColored(ImVec4{1.0f, 0.0f, 0.0f, 1.0f}, "No blocks allocated!");
            }
            else
            {
                DisplayMemoryMapAndStatistics();
            }
        }
    }

    void IndexBuffer::MergeFreeBlocks()
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

    std::optional<GPU::GeometryInfo> IndexBuffer::TryToFindFreeBlock(u32 elementCount)
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

    GPU::GeometryInfo IndexBuffer::AppendAtEnd(u32 elementCount)
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
            m_resizeInfo = IndexBuffer::ResizeInfo
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

    void IndexBuffer::Resize
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue
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

        auto oldBuffer = buffer;

        deletionQueue.Push([allocator, oldBuffer] () mutable
        {
            oldBuffer.Destroy(allocator);
        });

        buffer = Vk::Buffer
        (
            device,
            allocator,
            m_resizeInfo->requiredCapacity * sizeof(GPU::Index),
            alignof(GPU::Index),
            INDEX_BUFFER_USAGE,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        if
        (
            oldBuffer.handle == VK_NULL_HANDLE ||
            m_resizeInfo->blocksToCopy.empty() ||
            m_usedBlocks.empty()
        )
        {
            return;
        }

        std::vector<VkBufferCopy2> copyRegions = {};

        Vk::BarrierWriter barrierWriterOld = {};
        Vk::BarrierWriter barrierWriterNew = {};

        for (const auto& block : m_resizeInfo->blocksToCopy)
        {
            if (!std::ranges::contains(m_usedBlocks, block))
            {
                continue;
            }

            const VkBufferCopy2 copyRegion =
            {
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = block.offset * sizeof(GPU::Index),
                .dstOffset = block.offset * sizeof(GPU::Index),
                .size      = block.count  * sizeof(GPU::Index)
            };

            copyRegions.emplace_back(copyRegion);

            barrierWriterOld.WriteBufferBarrier
            (
                oldBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = INDEX_BUFFER_STAGE_MASK,
                    .srcAccessMask  = INDEX_BUFFER_ACCESS_MASK,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = copyRegion.srcOffset,
                    .size           = copyRegion.size
                }
            );

            barrierWriterNew.WriteBufferBarrier
            (
                buffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = INDEX_BUFFER_STAGE_MASK,
                    .dstAccessMask  = INDEX_BUFFER_ACCESS_MASK,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = copyRegion.dstOffset,
                    .size           = copyRegion.size
                }
            );
        }

        if (copyRegions.empty())
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "Index Buffer -> Resize Copy", {0.3882f, 0.9294f, 0.2118f, 1.0f});

        barrierWriterOld.Execute(cmdBuffer);

        const VkCopyBufferInfo2 copyInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext       = nullptr,
            .srcBuffer   = oldBuffer.handle,
            .dstBuffer   = buffer.handle,
            .regionCount = static_cast<u32>(copyRegions.size()),
            .pRegions    = copyRegions.data(),
        };

        vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

        barrierWriterNew.Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void IndexBuffer::DisplayMemoryMapAndStatistics()
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

        std::vector<Vk::UIGeometryInfo> combinedUIBlocks = {};

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

        if (ImGui::BeginTable("##IndexBufferSubAllocatorStatisticsTable", 5, ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Block Count");
            ImGui::TableSetupColumn("Element Count");
            ImGui::TableSetupColumn("Bytes");
            ImGui::TableSetupColumn("Percentage");
            ImGui::TableHeadersRow();

            const usize usedBytes      = totalUsed * sizeof(GPU::Index);
            const usize freeBytes      = totalFree * sizeof(GPU::Index);
            const usize allocatedBytes = (totalUsed + totalFree) * sizeof(GPU::Index);

            const f64 usedFraction      = static_cast<f64>(usedBytes)      / static_cast<f64>(buffer.size);
            const f64 freeFraction      = static_cast<f64>(freeBytes)      / static_cast<f64>(buffer.size);
            const f64 allocatedFraction = static_cast<f64>(allocatedBytes) / static_cast<f64>(buffer.size);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Used");
            ImGui::TableNextColumn();
            ImGui::Text("%llu", m_usedBlocks.size());
            ImGui::TableNextColumn();
            ImGui::Text("%llu", totalUsed);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", usedBytes);
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
            ImGui::Text("%llu", freeBytes);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f%%", 100.0 * freeFraction);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Allocated");
            ImGui::TableNextColumn();
            ImGui::Text("%llu", m_usedBlocks.size() + m_freeBlocks.size());
            ImGui::TableNextColumn();
            ImGui::Text("%llu", totalUsed + totalFree);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", allocatedBytes);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f%%", 100.0 * allocatedFraction);

            ImGui::EndTable();
        }
    }
}