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

#ifndef IBL_PASS_H
#define IBL_PASS_H

#include "IBLMaps.h"
#include "Engine/Cache.h"
#include "Renderer/Objects/Samplers.h"
#include "Vulkan/PipelineManager.h"
#include "Vulkan/FormatHelper.h"
#include "Vulkan/GeometryBuffer.h"
#include "Vulkan/ImageDownloader.h"
#include "Vulkan/TextureManager.h"

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

        void Update
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Context& context,
            const Objects::Samplers& samplers,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            Vk::MegaSet& megaSet,
            Vk::StagingPool& stagingPool,
            Vk::ImageDownloader& imageDownloader,
            tf::Executor& executor,
            Scratch::Allocator& scratchAllocator,
            Engine::DeletionQueue& deletionQueue
        );

        [[nodiscard]] IBL::IBLID GenerateIBL(const std::string_view hdrMapAssetPath);

        void DestroyIBL(IBL::IBLID id);

        IBL::IBLMaps GetIBLMaps(IBL::IBLID id);

        void Destroy(VmaAllocator allocator);
    private:
        struct LoadedIBLMaps
        {
            IBL::IBLID   id      = 0;
            IBL::IBLMaps iblMaps = {};
        };

        IBL::IBLMaps LoadIBLMaps
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Context& context,
            const Objects::Samplers& samplers,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            Vk::MegaSet& megaSet,
            Vk::StagingPool& stagingPool,
            Vk::ImageDownloader& imageDownloader,
            tf::Executor& executor,
            Scratch::Allocator& scratchAllocator,
            Engine::DeletionQueue& deletionQueue
        );

        [[nodiscard]] Vk::TextureID LoadHDRMap
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            Vk::MegaSet& megaSet,
            Vk::StagingPool& stagingPool,
            tf::Executor& executor,
            Scratch::Allocator& scratchAllocator,
            Engine::DeletionQueue& deletionQueue
        );

        [[nodiscard]] Vk::TextureID GenerateSkybox
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Context& context,
            const Objects::Samplers& samplers,
            const Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            Vk::MegaSet& megaSet,
            Vk::TextureID hdrMapID,
            Scratch::Allocator& scratchAllocator,
            Engine::DeletionQueue& deletionQueue
        );

        [[nodiscard]] Vk::TextureID GenerateIrradianceMap
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Context& context,
            const Objects::Samplers& samplers,
            const Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            Vk::MegaSet& megaSet,
            Vk::TextureID skyboxID
        );

        [[nodiscard]] Vk::TextureID GeneratePreFilterMap
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Context& context,
            const Objects::Samplers& samplers,
            const Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            Vk::MegaSet& megaSet,
            Vk::TextureID skyboxID,
            Engine::DeletionQueue& deletionQueue
        );

        [[nodiscard]] Vk::TextureID ShrinkSkybox
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            Vk::TextureManager& textureManager,
            Vk::MegaSet& megaSet,
            Vk::TextureID skyboxID,
            Scratch::Allocator& scratchAllocator,
            Engine::DeletionQueue& deletionQueue
        );

        [[nodiscard]] Vk::TextureID GenerateBRDFLookupTable
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Context& context,
            Vk::TextureManager& textureManager,
            Vk::StagingPool& stagingPool,
            Vk::MegaSet& megaSet,
            Vk::ImageDownloader& imageDownloader,
            tf::Executor& executor,
            Engine::DeletionQueue& deletionQueue
        );

        Vk::Buffer m_matrixBuffer = {};

        std::optional<std::string> m_pathToLoad = std::nullopt;

        std::optional<Generator::LoadedIBLMaps> m_loadedIBLMaps = std::nullopt;

        std::optional<Vk::TextureID> m_brdfLutID = std::nullopt;

        std::optional<IBL::IBLMaps> m_mapsToDestroy = std::nullopt;
    };
}

#endif