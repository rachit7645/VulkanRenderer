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

#ifndef CULLING_DISPATCH_H
#define CULLING_DISPATCH_H

#include "FrustumBuffer.h"
#include "Pipeline.h"
#include "Renderer/Buffers/SceneBuffer.h"
#include "Renderer/Buffers/IndirectBuffer.h"
#include "Renderer/Buffers/MeshBuffer.h"

namespace Renderer::Culling
{
    class Dispatch
    {
    public:
        explicit Dispatch(const Vk::Context& context);

        void Destroy(VkDevice device, VmaAllocator allocator);

        void Execute
        (
            usize FIF,
            usize frameIndex,
            const glm::mat4& projectionView,
            const Vk::CommandBuffer& cmdBuffer,
            const Buffers::MeshBuffer& meshBuffer,
            const Buffers::IndirectBuffer& indirectBuffer
        );
    private:
        Culling::Pipeline      m_frustumPipeline;
        Culling::FrustumBuffer m_frustumBuffer;
    };
}

#endif
