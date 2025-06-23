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

#ifndef IBL_PASS_H
#define IBL_PASS_H

#include "IBLMaps.h"
#include "Models/ModelManager.h"
#include "Renderer/Objects/GlobalSamplers.h"
#include "Vulkan/PipelineManager.h"
#include "Vulkan/FormatHelper.h"

namespace Renderer::IBL
{
    class Generator
    {
    public:
        Generator
        (
            VkDevice device,
            VmaAllocator allocator,
            const Vk::FormatHelper& formatHelper,
            const Vk::MegaSet& megaSet,
            Vk::PipelineManager& pipelineManager
        );

        IBL::IBLMaps Generate
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            const Objects::GlobalSamplers& samplers,
            Models::ModelManager& modelManager,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue,
            const std::string_view hdrMapAssetPath
        );

        void Destroy(VmaAllocator allocator);
    private:
        [[nodiscard]] Vk::TextureID LoadHDRMap
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            Models::ModelManager& modelManager,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue,
            const std::string_view hdrMapAssetPath
        );

        [[nodiscard]] Vk::TextureID GenerateSkybox
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            const Objects::GlobalSamplers& samplers,
            Models::ModelManager& modelManager,
            Vk::MegaSet& megaSet,
            Vk::TextureID hdrMapID,
            Util::DeletionQueue& deletionQueue
        );

        [[nodiscard]] Vk::TextureID GenerateIrradianceMap
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            const Objects::GlobalSamplers& samplers,
            Models::ModelManager& modelManager,
            Vk::MegaSet& megaSet,
            Vk::TextureID skyboxID
        );

        [[nodiscard]] Vk::TextureID GeneratePreFilterMap
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            const Objects::GlobalSamplers& samplers,
            Models::ModelManager& modelManager,
            Vk::MegaSet& megaSet,
            Vk::TextureID skyboxID,
            Util::DeletionQueue& deletionQueue
        );

        [[nodiscard]] Vk::TextureID GenerateBRDFLUT
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Context& context,
            Vk::TextureManager& textureManager,
            Vk::MegaSet& megaSet
        );

        Vk::Buffer m_matrixBuffer;

        // Cache BRDF LUT
        std::optional<Vk::TextureID> m_brdfLutID = std::nullopt;
    };
}

#endif