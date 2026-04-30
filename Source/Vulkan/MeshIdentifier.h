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

#ifndef ACCELERATION_STRUCTURE_MESH_INDENTIFIER_H
#define ACCELERATION_STRUCTURE_MESH_INDENTIFIER_H

#include "Models/ModelManager.h"

namespace Vk
{
    struct MeshIdentifier
    {
        bool operator==(const MeshIdentifier& other) const noexcept;

        Models::ModelID modelID        = 0;
        usize           localMeshIndex = 0;
    };
}

template <>
struct std::hash<Vk::MeshIdentifier>
{
    std::size_t operator()(const Vk::MeshIdentifier& identifier) const noexcept
    {
        std::size_t hash = 0;

        hash = Util::HashCombine(hash, identifier.modelID);
        hash = Util::HashCombine(hash, identifier.localMeshIndex);

        return hash;
    }
};

#endif