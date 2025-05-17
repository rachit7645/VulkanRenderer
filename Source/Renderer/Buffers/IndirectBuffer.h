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

#ifndef INDIRECT_BUFFER_H
#define INDIRECT_BUFFER_H

#include "DrawCallBuffer.h"
#include "Renderer/RenderObject.h"
#include "Util/Types.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Constants.h"
#include "Models/ModelManager.h"

namespace Renderer::Buffers
{
    class IndirectBuffer
    {
    public:
        IndirectBuffer(VkDevice device, VmaAllocator allocator);

        void WriteDrawCalls
        (
            usize FIF,
            VmaAllocator allocator,
            const Models::ModelManager& modelManager,
            const std::span<const Renderer::RenderObject> renderObjects
        );

        void Destroy(VmaAllocator allocator);

        std::array<Buffers::DrawCallBuffer, Vk::FRAMES_IN_FLIGHT> writtenDrawCallBuffers;

        struct CulledBuffers
        {
            CulledBuffers(VkDevice device, VmaAllocator allocator);

            void Destroy(VmaAllocator allocator);

            DrawCallBuffer opaqueBuffer;
            DrawCallBuffer opaqueDoubleSidedBuffer;
            DrawCallBuffer alphaMaskedBuffer;
            DrawCallBuffer alphaMaskedDoubleSidedBuffer;
        } frustumCulledBuffers;
    };
}

#endif