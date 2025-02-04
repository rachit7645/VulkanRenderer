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

#ifndef DEPTH_PASS_H
#define DEPTH_PASS_H

#include "DepthPipeline.h"
#include "Vulkan/DepthBuffer.h"
#include "Vulkan/Constants.h"
#include "Vulkan/GeometryBuffer.h"
#include "Renderer/IndirectBuffer.h"
#include "Renderer/MeshBuffer.h"
#include "Renderer/SceneBuffer.h"

namespace Renderer::Depth
{
    class DepthPass
    {
    public:
        DepthPass
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            const Vk::MegaSet& megaSet,
            VkExtent2D extent
        );

        void Recreate
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            VkExtent2D extent
        );

        void Destroy(VkDevice device, VkCommandPool cmdPool);

        void Render
        (
            usize FIF,
            const Vk::GeometryBuffer& geometryBuffer,
            const Renderer::SceneBuffer& sceneBuffer,
            const Renderer::MeshBuffer& meshBuffer,
            const Renderer::IndirectBuffer& indirectBuffer
        );

        Depth::DepthPipeline pipeline;

        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers;

        Vk::DepthBuffer depthBuffer;
    private:
        void InitData
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            VkExtent2D extent
        );

        VkExtent2D          m_resolution;
        Util::DeletionQueue m_deletionQueue;
    };
}

#endif
