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

#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include <string_view>

#include "Mesh.h"
#include "Vulkan/Context.h"
#include "Vulkan/TextureManager.h"
#include "Vulkan/GeometryBuffer.h"
#include "Externals/FastGLTF.h"

namespace Models
{
    class Model
    {
    public:
        void LoadFromFile
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            Vk::StagingPool& stagingPool,
            tf::Executor& executor,
            Util::DeletionQueue& deletionQueue,
            const std::string_view path
        );

        void Destroy
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager,
            Vk::GeometryBuffer& geometryBuffer,
            Util::DeletionQueue& deletionQueue
        );

        std::string               name     = "Null/Model";
        std::vector<Models::Mesh> meshes   = {};
        bool                      isLoaded = false;
    private:
        struct TextureInfo
        {
            Vk::TextureID id         = std::numeric_limits<Vk::TextureID>::max();
            u32           uvMapIndex = 0;
        };

        void ProcessScenes
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            Vk::StagingPool& stagingPool,
            tf::Executor& executor,
            Util::DeletionQueue& deletionQueue,
            const std::string_view directory,
            const fastgltf::Asset& asset
        );

        void ProcessNode
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            Vk::StagingPool& stagingPool,
            tf::Executor& executor,
            Util::DeletionQueue& deletionQueue,
            const std::string_view directory,
            const fastgltf::Asset& asset,
            usize nodeIndex,
            glm::mat4 nodeMatrix
        );

        void LoadMesh
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            Vk::StagingPool& stagingPool,
            tf::Executor& executor,
            Util::DeletionQueue& deletionQueue,
            const std::string_view directory,
            const fastgltf::Asset& asset,
            const fastgltf::Mesh& mesh,
            const glm::mat4& nodeMatrix
        );

        template<typename T>
        requires fastgltf::IsTextureInfo<T>
        [[nodiscard]] Model::TextureInfo LoadTexture
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::TextureManager& textureManager,
            Vk::StagingPool& stagingPool,
            tf::Executor& executor,
            Util::DeletionQueue& deletionQueue,
            const std::string_view directory,
            const fastgltf::Asset& asset,
            const std::optional<T>& textureInfo,
            const std::string_view defaultTexture
        );
    };

    namespace Detail
    {

    };
}

#endif