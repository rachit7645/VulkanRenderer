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

#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#include "Model.h"
#include "Vulkan/TextureManager.h"
#include "Vulkan/GeometryBuffer.h"
#include "Vulkan/CommandBufferAllocator.h"
#include "Util/Types.h"
#include "Externals/UnorderedDense.h"

namespace Models
{
    using ModelID = u64;

    class ModelManager
    {
    public:
        explicit ModelManager(const Vk::Context& context);

        void Destroy(VkDevice device, VmaAllocator allocator);

        [[nodiscard]] Models::ModelID AddModel
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const std::string_view path
        );

        void DestroyModel
        (
            ModelID id,
            VkDevice device,
            VmaAllocator allocator,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        );

        [[nodiscard]] const Model& GetModel(Models::ModelID id) const;

        void Update
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        );

        void ImGuiDisplay();

        Vk::GeometryBuffer geometryBuffer;
        Vk::TextureManager textureManager;
    private:
        struct ModelInfo
        {
            Models::Model model;
            u64           referenceCount = 0;
        };

        ankerl::unordered_dense::map<Models::ModelID, ModelManager::ModelInfo> m_modelMap;
    };
}

#endif
