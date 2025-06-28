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

#ifndef FORWARD_MESH_BUFFER_H
#define FORWARD_MESH_BUFFER_H

#include <array>
#include <vulkan/vulkan.h>

#include "Renderer/RenderObject.h"
#include "Util/Types.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Constants.h"
#include "Models/ModelManager.h"

namespace Renderer::Buffers
{
    constexpr usize MAX_MESH_COUNT = 1 << 16;

    class MeshBuffer
    {
    public:
        MeshBuffer(VkDevice device, VmaAllocator allocator);

        void LoadMeshes
        (
            usize frameIndex,
            VmaAllocator allocator,
            const Models::ModelManager& modelManager,
            const std::vector<Renderer::RenderObject>& renderObjects
        );

        [[nodiscard]] const Vk::Buffer& GetCurrentMeshBuffer(usize frameIndex)  const;
        [[nodiscard]] const Vk::Buffer& GetPreviousMeshBuffer(usize frameIndex) const;

        [[nodiscard]] const Vk::Buffer& GetCurrentInstanceBuffer(usize frameIndex)  const;
        [[nodiscard]] const Vk::Buffer& GetPreviousInstanceBuffer(usize frameIndex) const;

        void Destroy(VmaAllocator allocator);
    private:
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT + 1> m_meshBuffers;
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT + 1> m_instanceBuffers;
    };
}

#endif
