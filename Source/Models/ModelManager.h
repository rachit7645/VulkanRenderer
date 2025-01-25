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

#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include <unordered_map>

#include "Model.h"
#include "Vulkan/TextureManager.h"
#include "Vulkan/GeometryBuffer.h"
#include "Util/Util.h"

namespace Models
{
    class ModelManager
    {
    public:
        explicit ModelManager(const Vk::Context& context);
        void Destroy(VkDevice device, VmaAllocator allocator);

        [[nodiscard]] usize AddModel(const Vk::Context& context, Vk::MegaSet& megaSet, const std::string_view path);
        [[nodiscard]] const Model& GetModel(usize modelID) const;

        void Update(const Vk::Context& context);

        std::unordered_map<u32, Models::Model> modelMap;

        Vk::GeometryBuffer geometryBuffer;
        Vk::TextureManager textureManager;
    };
}

#endif
