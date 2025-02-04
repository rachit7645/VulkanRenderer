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

#ifndef FORWARD_PASS_H
#define FORWARD_PASS_H

#include "ForwardPipeline.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/DepthBuffer.h"
#include "Vulkan/GeometryBuffer.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/Constants.h"
#include "Renderer/Camera.h"
#include "Renderer/IndirectBuffer.h"
#include "Renderer/MeshBuffer.h"
#include "Renderer/SceneBuffer.h"

namespace Renderer::Forward
{
    class ForwardPass
    {
    public:
        ForwardPass
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager,
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
            const Vk::MegaSet& megaSet,
            const Vk::GeometryBuffer& geometryBuffer,
            const Renderer::SceneBuffer& sceneBuffer,
            const Renderer::MeshBuffer& meshBuffer,
            const Renderer::IndirectBuffer& indirectBuffer,
            const Vk::DepthBuffer& depthBuffer
        );

        Forward::ForwardPipeline pipeline;

        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers;

        Vk::Image     colorAttachment;
        Vk::ImageView colorAttachmentView;
    private:
        void InitData(const Vk::Context& context, const Vk::FormatHelper& formatHelper, VkExtent2D extent);

        VkExtent2D          m_resolution;
        Util::DeletionQueue m_deletionQueue;
    };
}

#endif