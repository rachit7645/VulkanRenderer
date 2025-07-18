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

#ifndef RENDER_OBJECT_H
#define RENDER_OBJECT_H

#include "Externals/GLM.h"
#include "Models/ModelManager.h"

namespace Renderer
{
    struct RenderObject
    {
        void Destroy
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::MegaSet& megaSet,
            Models::ModelManager& modelManager,
            Util::DeletionQueue& deletionQueue
        );

        Models::ModelID modelID  = 0;
        glm::vec3       position = {};
        glm::vec3       rotation = {};
        glm::vec3       scale    = {1.0f, 1.0f, 1.0f};
    };
}

#endif
