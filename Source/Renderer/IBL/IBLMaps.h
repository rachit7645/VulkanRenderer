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

#ifndef IBL_MAPS_H
#define IBL_MAPS_H

#include "Vulkan/GeometryBuffer.h"
#include "Vulkan/TextureManager.h"
#include "Vulkan/FormatHelper.h"
#include "Vulkan/MegaSet.h"

namespace Renderer::IBL
{
    class IBLMaps
    {
    public:
        void Generate
        (
            const std::string_view hdrMap,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            const Vk::GeometryBuffer& geometryBuffer,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        std::optional<u32> brdfLutID    = std::nullopt;
        std::optional<u32> skyboxID     = std::nullopt;
        std::optional<u32> irradianceID = std::nullopt;
        std::optional<u32> preFilterID  = std::nullopt;
    private:
        Vk::Buffer SetupMatrixBuffer(const Vk::Context& context);

        void CreateCubeMap
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            const Vk::GeometryBuffer& geometryBuffer,
            const Vk::Buffer& matrixBuffer,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager,
            u32 hdrMapID
        );

        void CreateIrradianceMap
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            const Vk::GeometryBuffer& geometryBuffer,
            const Vk::Buffer& matrixBuffer,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        void CreatePreFilterMap
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            const Vk::GeometryBuffer& geometryBuffer,
            const Vk::Buffer& matrixBuffer,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        void CreateBRDFLUT
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        Util::DeletionQueue m_deletionQueue;
    };
}

#endif
