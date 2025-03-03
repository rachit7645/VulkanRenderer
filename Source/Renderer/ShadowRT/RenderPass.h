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

#include "SBTBuffer.h"
#include "Pipeline.h"
#include "Vulkan/Constants.h"
#include "Vulkan/GeometryBuffer.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/AccelerationStructure.h"

namespace Renderer::ShadowRT
{
    class RenderPass
    {
    public:
        RenderPass
        (
            const Vk::Context& context,
            const Vk::MegaSet& megaSet,
            Vk::FramebufferManager& framebufferManager
        );

        void Destroy(VkDevice device, VmaAllocator allocator, VkCommandPool cmdPool);

        void Render
        (
            usize FIF,
            VkDevice device,
            VmaAllocator allocator,
            const Vk::MegaSet& megaSet,
            const Vk::FramebufferManager& framebufferManager,
            Vk::AccelerationStructure& accelerationStructure,
            const std::span<const Renderer::RenderObject> renderObjects
        );

        ShadowRT::Pipeline pipeline;

        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers;

        ShadowRT::SBTBuffer sbtBuffer;
    };
}

#endif
