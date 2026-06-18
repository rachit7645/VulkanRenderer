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

#include "Scene.h"

namespace Engine
{
    void Scene::Destroy
    (
        const Vk::Context& context,
        Models::ModelManager& modelManager,
        Renderer::IBL::Generator& iblGenerator,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (!isLoaded)
        {
            return;
        }

        iblGenerator.DestroyIBL
        (
            iblMapsID,
            context,
            modelManager.textureManager,
            megaSet,
            deletionQueue
        );

        for (auto& renderObject : renderObjects)
        {
            renderObject.Destroy(modelManager);
        }
    }
}