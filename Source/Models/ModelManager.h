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

#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#include "Model.h"
#include "ModelID.h"
#include "MeshIdentifier.h"
#include "Vulkan/TextureManager.h"
#include "Vulkan/GeometryBuffer.h"
#include "Vulkan/CommandBufferAllocator.h"
#include "Util/Types.h"
#include "Externals/UnorderedDense.h"

namespace Models
{
    class ModelManager
    {
    public:
        [[nodiscard]] Models::ModelID Load(const std::string_view path);

        void Free(Models::ModelID id);

        [[nodiscard]] bool         IsModelLoaded(Models::ModelID id) const;
        [[nodiscard]] const Model* TryGetModel  (Models::ModelID id) const;
        [[nodiscard]] const Model& GetModel     (Models::ModelID id) const;

        void Update
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            Vk::MegaSet& megaSet,
            Vk::StagingPool& stagingPool,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            Scratch::Allocator& scratchAllocator,
            tf::Executor& executor,
            Engine::DeletionQueue& deletionQueue
        );

        void ImGuiDisplay();
    private:
        struct ModelInfo
        {
            Models::Model model          = {};
            u64           referenceCount = 0;
        };

        struct ModelLoadInfo
        {
            std::string path;
            u64         referenceCount = 0;
        };

        struct LoadedModel
        {
            Models::ModelID         id        = {};
            ModelManager::ModelInfo modelInfo = {};
        };

        ankerl::unordered_dense::map<Models::ModelID, ModelManager::ModelInfo>     m_loadedModels;
        ankerl::unordered_dense::map<Models::ModelID, ModelManager::ModelLoadInfo> m_pendingModels;

        std::vector<Models::ModelID> m_requestedModelDeletions;
    };
}

#endif
