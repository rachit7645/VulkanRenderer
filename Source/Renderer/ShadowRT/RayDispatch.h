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

#ifndef SHADOW_RT_PASS_H
#define SHADOW_RT_PASS_H

#include "Renderer/Buffers/MeshBuffer.h"
#include "Vulkan/GeometryBuffer.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/AccelerationStructure.h"
#include "Vulkan/ShaderBindingTable.h"
#include "Renderer/Buffers/SceneBuffer.h"
#include "Renderer/Objects/GlobalSamplers.h"

namespace Renderer::ShadowRT
{
    class RayDispatch
    {
    public:
        RayDispatch
        (
            const Vk::MegaSet& megaSet,
            Vk::PipelineManager& pipelineManager,
            Vk::FramebufferManager& framebufferManager
        );

        void TraceRays
        (
            usize FIF,
            usize frameIndex,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            const Vk::MegaSet& megaSet,
            const Models::ModelManager& modelManager,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Buffers::SceneBuffer& sceneBuffer,
            const Buffers::MeshBuffer& meshBuffer,
            const Objects::GlobalSamplers& samplers,
            const Vk::AccelerationStructure& accelerationStructure,
            Util::DeletionQueue& deletionQueue
        );

        void Destroy(VmaAllocator allocator);
    private:
        std::optional<Vk::ShaderBindingTable> m_shaderBindingTable = std::nullopt;
    };
}

#endif
