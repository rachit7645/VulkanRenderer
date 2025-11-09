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

#ifndef TAA_PASS_H
#define TAA_PASS_H

#include "Renderer/Objects/GlobalSamplers.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/PipelineManager.h"

namespace Renderer::TAA
{
    class RenderPass
    {
    public:
        RenderPass
        (
            const Vk::FormatHelper& formatHelper,
            const Vk::MegaSet& megaSet,
            Vk::PipelineManager& pipelineManager,
            Vk::FramebufferManager& framebufferManager
        );

        void Render
        (
            usize frameIndex,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Objects::GlobalSamplers& samplers
        );

        void ResetHistory();
    private:
        bool m_hasToResetHistory = true;
    };
}

#endif