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

#ifndef DRAW_CALL_BUFFER_H
#define DRAW_CALL_BUFFER_H

#include "Util/Types.h"
#include "Vulkan/Buffer.h"
#include "Renderer/RenderObject.h"
#include "Models/ModelManager.h"

namespace Renderer::Buffers
{
    class DrawCallBuffer
    {
    public:
        enum class Type
        {
            CPUToGPU,
            GPUOnly
        };

        DrawCallBuffer() = default;
        DrawCallBuffer(VkDevice device, VmaAllocator allocator, Type type);

        void WriteDrawCalls
        (
            VmaAllocator allocator,
            const Models::ModelManager& modelManager,
            const std::span<const Renderer::RenderObject> renderObjects
        );

        void Destroy(VmaAllocator allocator);

        Type type = Type::CPUToGPU;

        u32 writtenDrawCount = 0;

        Vk::Buffer drawCallBuffer = {};

        // CPU To GPU draw call buffers do not need a mesh index buffer
        std::optional<Vk::Buffer> meshIndexBuffer = std::nullopt;
    private:
        [[nodiscard]] bool IsCPUWritable() const;
    };
}

#endif
