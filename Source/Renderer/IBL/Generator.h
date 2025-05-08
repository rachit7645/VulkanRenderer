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
#include "Convolution/Pipeline.h"
#include "BRDF/Pipeline.h"
#include "Converter/Pipeline.h"
#include "PreFilter/Pipeline.h"
#include "Models/ModelManager.h"

namespace Renderer::IBL
{
    class Generator
    {
    public:
        Generator
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        IBL::IBLMaps Generate
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Models::ModelManager& modelManager,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue,
            const std::string_view hdrMapAssetPath
        );

        void Destroy(VkDevice device, VmaAllocator allocator);
    private:
        [[nodiscard]] u32 LoadHDRMap
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            Models::ModelManager& modelManager,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue,
            const std::string_view hdrMapAssetPath
        );

        [[nodiscard]] u32 GenerateSkybox
        (
            const Vk::CommandBuffer& cmdBuffer,
            u32 hdrMapID,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Models::ModelManager& modelManager,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        );

        [[nodiscard]] u32 GenerateIrradianceMap
        (
            const Vk::CommandBuffer& cmdBuffer,
            u32 skyboxID,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Models::ModelManager& modelManager,
            Vk::MegaSet& megaSet
        );

        [[nodiscard]] u32 GeneratePreFilterMap
        (
            const Vk::CommandBuffer& cmdBuffer,
            u32 skyboxID,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Models::ModelManager& modelManager,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        );

        [[nodiscard]] u32 GenerateBRDFLUT
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Vk::TextureManager& textureManager,
            Vk::MegaSet& megaSet
        );

        Converter::Pipeline   m_converterPipeline;
        Convolution::Pipeline m_convolutionPipeline;
        PreFilter::Pipeline   m_preFilterPipeline;
        BRDF::Pipeline        m_brdfLutPipeline;

        Vk::Buffer m_matrixBuffer;

        // Cache BRDF LUT
        std::optional<u32> m_brdfLutID = std::nullopt;
    };
}

#endif