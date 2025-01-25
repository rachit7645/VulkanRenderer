// Copyright (c) 2023 - 2025 Rachit
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GEOMETRY_BUFFER_H
#define GEOMETRY_BUFFER_H

#include <span>
#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "Models/Info.h"
#include "Models/Vertex.h"
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

        std::pair<Models::Info, Models::Info> SetupLoad
        (
            std::vector<Models::Index>&& indices,
            std::vector<Models::Vertex>&& vertices
        );

        void Update(const Vk::Context& context);

        Vk::Buffer indexBuffer;
        Vk::Buffer vertexBuffer;

        u32         vertexCount = 0;
        u32         indexCount  = 0;
        VkIndexType indexType   = VK_INDEX_TYPE_UINT32;
    private:
        void UploadToBuffer
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Buffer& source,
            const Vk::Buffer& destination,
            VkDeviceSize offsetBytes,
            VkDeviceSize sizeBytes,
            const void* data
        );

        void CheckSize(usize sizeBytes, usize offsetBytes, usize bufferSize);

        std::vector<std::pair<Models::Info, std::vector<Models::Index>>>  m_indicesToLoad;
        std::vector<std::pair<Models::Info, std::vector<Models::Vertex>>> m_verticesToLoad;
    };
}

#endif
