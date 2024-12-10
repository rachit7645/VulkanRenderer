//    Copyright 2023 - 2024 Rachit Khandelwal
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#include "ModelManager.h"

#include "Util/Log.h"

namespace Models
{
    ModelManager::ModelManager(VkDevice device, VmaAllocator allocator)
        : geometryBuffer(device, allocator)
    {
    }

    usize ModelManager::AddModel(const Vk::Context& context, Vk::TextureManager& textureManager, const std::string_view path)
    {
        usize pathHash = std::hash<std::string_view>()(path);

        if (!modelMap.contains(pathHash))
        {
            modelMap.emplace(pathHash, Model(context, geometryBuffer, textureManager, path));
        }

        return pathHash;
    }

    const Model& ModelManager::GetModel(usize modelID) const
    {
        const auto iter = modelMap.find(modelID);

        if (iter == modelMap.end())
        {
            Logger::Error("Invalid model ID! [ID={}]\n", modelID);
        }

        return iter->second;
    }

    void ModelManager::Destroy(VmaAllocator allocator)
    {
        geometryBuffer.Destroy(allocator);
    }
}
