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

#ifndef SCENE_EDITOR_H
#define SCENE_EDITOR_H

#include "Scene.h"
#include "Config.h"

namespace Engine
{
    class SceneEditor
    {
    public:
        SceneEditor();

        bool Update
        (
            const Vk::Context& context,
            const Util::FrameCounter& frameCounter,
            const Engine::Inputs& inputs,
            Models::ModelManager& modelManager,
            Renderer::IBL::Generator& iblGenerator,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager,
            Util::DeletionQueue& deletionQueue
        );

        Engine::Config config = {};

        std::optional<Engine::Scene> scene = std::nullopt;
    private:
        void Load
        (
            Models::ModelManager& modelManager,
            Renderer::IBL::Generator& iblGenerator
        );

        void Destroy
        (
            const Vk::Context& context,
            Models::ModelManager& modelManager,
            Renderer::IBL::Generator& iblGenerator,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager,
            Util::DeletionQueue& deletionQueue
        );

        void Save(const Models::ModelManager& modelManager);

        std::string m_sceneToLoad = "Null/Scene";

        std::string            m_loadedHDRMapPath   = {};
        std::string            m_loadedModelPath    = {};
        Renderer::RenderObject m_loadedRenderObject = {};
        GPU::PointLight        m_loadedPointLight   = {};
        GPU::SpotLight         m_loadedSpotLight    = {};

        bool m_normalizeSpotLightDirection = true;

        std::string m_savedHDRMapPath = {};
    };
}

#endif
