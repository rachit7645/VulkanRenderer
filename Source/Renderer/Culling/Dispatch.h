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

#include "Pipeline.h"
#include "Vulkan/Constants.h"
#include "Renderer/Buffers/IndirectBuffer.h"
#include "Renderer/Buffers/MeshBuffer.h"
#include "Renderer/Buffers/SceneBuffer.h"

namespace Renderer::Culling
{
    class Dispatch
    {
    public:
        explicit Dispatch(const Vk::Context& context);

        void Destroy(VkDevice device);

        void ComputeDispatch
        (
            usize FIF,
            const Vk::CommandBuffer& cmdBuffer,
            const Buffers::MeshBuffer& meshBuffer,
            const Buffers::IndirectBuffer& indirectBuffer
        );

        Culling::Pipeline pipeline;
    };
}

#endif
