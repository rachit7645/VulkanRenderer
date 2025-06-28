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

#include "IndirectBuffer.h"

#include "MeshBuffer.h"
#include "Util/Log.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::Buffers
{
    IndirectBuffer::IndirectBuffer(VkDevice device, VmaAllocator allocator)
        : frustumCulledBuffers(device, allocator)
    {
        Vk::SetDebugName(device, frustumCulledBuffers.opaqueBuffer.drawCallBuffer.handle,                      "IndirectBuffer/DrawCallBuffer/FrustumCulled/Opaque/DrawCalls");
        Vk::SetDebugName(device, frustumCulledBuffers.opaqueBuffer.instanceIndexBuffer.handle,                 "IndirectBuffer/DrawCallBuffer/FrustumCulled/Opaque/InstanceIndices");
        Vk::SetDebugName(device, frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer.handle,           "IndirectBuffer/DrawCallBuffer/FrustumCulled/Opaque/DoubleSided/DrawCalls");
        Vk::SetDebugName(device, frustumCulledBuffers.opaqueDoubleSidedBuffer.instanceIndexBuffer.handle,      "IndirectBuffer/DrawCallBuffer/FrustumCulled/Opaque/DoubleSided/InstanceIndices");
        Vk::SetDebugName(device, frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer.handle,                 "IndirectBuffer/DrawCallBuffer/FrustumCulled/AlphaMasked/DrawCalls");
        Vk::SetDebugName(device, frustumCulledBuffers.alphaMaskedBuffer.instanceIndexBuffer.handle,            "IndirectBuffer/DrawCallBuffer/FrustumCulled/AlphaMasked/InstanceIndices");
        Vk::SetDebugName(device, frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer.handle,      "IndirectBuffer/DrawCallBuffer/FrustumCulled/AlphaMasked/DoubleSided/DrawCalls");
        Vk::SetDebugName(device, frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.instanceIndexBuffer.handle, "IndirectBuffer/DrawCallBuffer/FrustumCulled/AlphaMasked/DoubleSided/InstanceIndices");
    }

    void IndirectBuffer::ComputeDrawCount(const Models::ModelManager& modelManager, const std::span<const Renderer::RenderObject> renderObjects)
    {
        maxDrawCount = 0;

        for (const auto& renderObject : renderObjects)
        {
            maxDrawCount += modelManager.GetModel(renderObject.modelID).meshes.size();
        }
    }

    IndirectBuffer::CulledBuffers::CulledBuffers(VkDevice device, VmaAllocator allocator)
        : opaqueBuffer(device, allocator),
          opaqueDoubleSidedBuffer(device, allocator),
          alphaMaskedBuffer(device, allocator),
          alphaMaskedDoubleSidedBuffer(device, allocator)
    {
    }

    void IndirectBuffer::CulledBuffers::Destroy(VmaAllocator allocator)
    {
        opaqueBuffer.Destroy(allocator);
        opaqueDoubleSidedBuffer.Destroy(allocator);
        alphaMaskedBuffer.Destroy(allocator);
        alphaMaskedDoubleSidedBuffer.Destroy(allocator);
    }

    void IndirectBuffer::Destroy(VmaAllocator allocator)
    {
        frustumCulledBuffers.Destroy(allocator);
    }
}