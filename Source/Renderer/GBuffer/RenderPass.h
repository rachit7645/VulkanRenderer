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

#ifndef GBUFFER_PASS_H
#define GBUFFER_PASS_H

#include "SingleSided/Pipeline.h"
#include "DoubleSided/Pipeline.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/GeometryBuffer.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/FramebufferManager.h"
#include "Renderer/Buffers/IndirectBuffer.h"
#include "Renderer/Buffers/MeshBuffer.h"
#include "Renderer/Buffers/SceneBuffer.h"

namespace Renderer::GBuffer
{
    class RenderPass
    {
    public:
        RenderPass
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Vk::FramebufferManager& framebufferManager,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        void Destroy(VkDevice device);

        void Render
        (
            usize FIF,
            usize frameIndex,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Models::ModelManager& modelManager,
            const Buffers::SceneBuffer& sceneBuffer,
            const Buffers::MeshBuffer& meshBuffer,
            const Buffers::IndirectBuffer& indirectBuffer
        );
    private:
        SingleSided::Pipeline m_singleSidedPipeline;
        DoubleSided::Pipeline m_doubleSidedPipeline;
    };
}

#endif