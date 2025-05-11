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

#ifndef BARRIER_WRITER_H
#define BARRIER_WRITER_H

#include "Image.h"
#include "Buffer.h"
#include "Barrier.h"

namespace Vk
{
    class BarrierWriter
    {
    public:
        BarrierWriter& WriteBufferBarrier(const Vk::Buffer& buffer, const Vk::BufferBarrier& barrier);
        BarrierWriter& WriteImageBarrier(const Vk::Image& image, const ImageBarrier& barrier);

        void Execute(const Vk::CommandBuffer& cmdBuffer);

        BarrierWriter& Clear();

        bool IsEmpty() const;
    private:
        std::vector<VkBufferMemoryBarrier2> m_bufferBarriers;
        std::vector<VkImageMemoryBarrier2>  m_imageBarriers;
    };
}

#endif
