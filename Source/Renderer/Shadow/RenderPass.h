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

#ifndef SHADOW_PASS_H
#define SHADOW_PASS_H

#include "Pipeline.h"
#include "CascadeBuffer.h"
#include "Vulkan/Constants.h"
#include "Vulkan/GeometryBuffer.h"
#include "Vulkan/FramebufferManager.h"
#include "Renderer/Buffers/IndirectBuffer.h"
#include "Renderer/Buffers/MeshBuffer.h"
#include "Renderer/Objects/Camera.h"
#include "Renderer/Objects/DirLight.h"

namespace Renderer::Shadow
{
    class RenderPass
    {
    public:
        RenderPass
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Vk::FramebufferManager& framebufferManager
        );

        void Destroy(VkDevice device, VmaAllocator allocator, VkCommandPool cmdPool);

        void Render
        (
            usize FIF,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::GeometryBuffer& geometryBuffer,
            const Buffers::MeshBuffer& meshBuffer,
            const Buffers::IndirectBuffer& indirectBuffer,
            const Objects::Camera& camera,
            const Objects::DirLight& light
        );

        Shadow::Pipeline pipeline;

        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers;

        Shadow::CascadeBuffer cascadeBuffer;
    private:
        std::array<Shadow::Cascade, CASCADE_COUNT> CalculateCascades
        (
            f32 aspectRatio,
            const Objects::Camera& camera,
            const Objects::DirLight& light
        );

        f32 m_cascadeSplitLambda = 0.95f;
        f32 m_cascadeOffset      = 1.3f;
    };
}

#endif
