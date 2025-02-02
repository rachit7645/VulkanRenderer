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

#ifndef RENDER_PASS_H
#define RENDER_PASS_H

#include "Pipeline.h"
#include "Vulkan/DepthBuffer.h"
#include "Vulkan/Constants.h"
#include "Vulkan/GeometryBuffer.h"
#include "Renderer/IndirectBuffer.h"
#include "Renderer/MeshBuffer.h"
#include "Renderer/SceneBuffer.h"

namespace Renderer::RenderPass
{
    class Pass
    {
    public:
        Pass
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

        void Render();

        RenderPass::Pipeline pipeline;

        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers = {};
    private:
        void InitData
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            VkExtent2D extent
        );

        glm::uvec2          m_renderSize    = {0, 0};
        Util::DeletionQueue m_deletionQueue = {};
    };
}

#endif
