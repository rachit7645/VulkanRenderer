//    Copyright 2023 - 2024 Rachit Khandelwal
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#ifndef GEOMETRY_BUFFER_H
#define GEOMETRY_BUFFER_H

#include <span>
#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "Models/Vertex.h"
#include "Models/VertexInfo.h"
#include "Util/Util.h"
#include "CommandBuffer.h"

namespace Vk
{
    class GeometryBuffer
    {
    public:
        explicit GeometryBuffer(VkDevice device, VmaAllocator allocator);

        void Bind(const Vk::CommandBuffer& cmdBuffer) const;
        void Destroy(VmaAllocator allocator) const;

        std::array<Models::VertexInfo, 2> LoadData
        (
            const Vk::Context& context,
            const std::span<const Models::Vertex> vertices,
            const std::span<const Models::Index> indices
        );

        // Buffers
        Vk::Buffer vertexBuffer;
        Vk::Buffer indexBuffer;

        // Data
        u32 vertexCount = 0;
        u32 indexCount  = 0;
        // Index type (Does this need to be a variable: probably.)
        VkIndexType indexType = VK_INDEX_TYPE_UINT32;
    private:
        template<typename T>
        Models::VertexInfo UploadData
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Buffer& buffer,
            u32& currentCount,
            const std::span<const T> data
        );

        Vk::Buffer m_stagingBuffer;
    };
}

#endif
