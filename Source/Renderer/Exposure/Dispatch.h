/*
 * Copyright (c) 2023 - 2026 Rachit
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

#ifndef EXPOSURE_DISPATCH_H
#define EXPOSURE_DISPATCH_H

#include "Renderer/Buffers/ExposureBuffers.h"
#include "Renderer/Objects/Samplers.h"
#include "Util/FrameCounter.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/PipelineManager.h"
#include "Vulkan/TextureManager.h"

namespace Renderer::Exposure
{
    class Dispatch
    {
    public:
        Dispatch
        (
            const Vk::FormatHelper& formatHelper,
            const Vk::MegaSet& megaSet,
            Vk::PipelineManager& pipelineManager,
            Vk::FramebufferManager& framebufferManager
        );

        void Execute
        (
            usize FIF,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Buffers::ExposureBuffers& exposureBuffer,
            const Objects::Samplers& samplers,
            const Util::FrameCounter& frameCounter,
            Scratch::Allocator& scratchAllocator
        );

        void ResetLuminance();
    private:
        void Histogram
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Buffers::ExposureBuffers& exposureBuffer,
            const Objects::Samplers& samplers
        );

        void Average
        (
            usize FIF,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Buffers::ExposureBuffers& exposureBuffer,
            const Util::FrameCounter& frameCounter,
            Scratch::Allocator& scratchAllocator
        );

        void Combine
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Objects::Samplers& samplers
        );

        bool m_hasLuminanceBeenReset = false;
        f32  m_adaptationSpeed       = 1.5f;
        f32  m_exposureBias          = 0.0f;
    };
}

#endif
