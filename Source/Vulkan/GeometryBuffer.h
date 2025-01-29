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

        std::array<Models::Info, 3> SetupUpload
        (
            VmaAllocator allocator,
            const std::vector<Models::Index>& indices,
            const std::vector<glm::vec3>& positions,
            const std::vector<Models::Vertex>& vertices
        );

        void Update(const Vk::CommandBuffer& cmdBuffer);
        void Clear(VmaAllocator allocator);

        void ImGuiDisplay() const;

        Vk::Buffer indexBuffer;
        Vk::Buffer positionBuffer;
        Vk::Buffer vertexBuffer;

        u32 indexCount    = 0;
        u32 positionCount = 0;
        u32 vertexCount   = 0;
    private:
        using Upload = std::pair<Models::Info, Vk::Buffer>;

        void UploadToBuffer
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Buffer& source,
            const Vk::Buffer& destination,
            VkDeviceSize offsetBytes,
            VkDeviceSize sizeBytes,
            VkPipelineStageFlags2 dstStageMask,
            VkAccessFlags2 dstAccessMask
        );

        std::vector<Upload> m_pendingIndexUploads;
        std::vector<Upload> m_pendingPositionUploads;
        std::vector<Upload> m_pendingVertexUploads;
    };
}

#endif
