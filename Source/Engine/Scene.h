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

#ifndef SCENE_H
#define SCENE_H

#include "Config.h"
#include "Renderer/RenderObject.h"
#include "Renderer/Objects/FreeCamera.h"
#include "Renderer/IBL/IBLMaps.h"
#include "Renderer/IBL/Generator.h"
#include "Models/ModelManager.h"
#include "Util/FrameCounter.h"
#include "GPU/Lights.h"

namespace Engine
{
    class Scene
    {
    public:
        Scene
        (
            const Engine::Config& config,
            Models::ModelManager& modelManager,
            Renderer::IBL::Generator& iblGenerator
        );

        void Update
        (
            const Util::FrameCounter& frameCounter,
            Engine::Inputs& inputs,
            Models::ModelManager& modelManager,
            Renderer::IBL::Generator& iblGenerator
        );

        void Destroy
        (
            const Vk::Context& context,
            Models::ModelManager& modelManager,
            Renderer::IBL::Generator& iblGenerator,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        );

        std::vector<Renderer::RenderObject> renderObjects = {};
        GPU::DirLight                       sun           = {};
        std::vector<GPU::PointLight>        pointLights   = {};
        std::vector<GPU::SpotLight>         spotLights    = {};
        Renderer::Objects::FreeCamera       camera        = {};
        Renderer::IBL::IBLID                iblMapsID     = 0;

        // TODO: Better render object update flags
        // This does not account for render object internal changes
        // Only addition/deletion of render objects will update this
        bool haveRenderObjectsChanged = false;
    private:
        std::string            m_loadedHDRMapPath   = {};
        std::string            m_loadedModelPath    = {};
        Renderer::RenderObject m_loadedRenderObject = {};
        GPU::PointLight        m_loadedPointLight   = {};
        GPU::SpotLight         m_loadedSpotLight    = {};

        bool m_normalizeSpotLights = true;
    };
}

#endif
