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
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Models::ModelManager& modelManager,
            Vk::MegaSet& megaSet,
            Renderer::IBL::Generator& iblGenerator,
            Util::DeletionQueue& deletionQueue
        );

        void Update
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Util::FrameCounter& frameCounter,
            Engine::Inputs& inputs,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Models::ModelManager& modelManager,
            Vk::MegaSet& megaSet,
            Renderer::IBL::Generator& iblGenerator,
            Util::DeletionQueue& deletionQueue
        );

        void Destroy
        (
            const Vk::Context& context,
            Vk::TextureManager& textureManager,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        );

        std::vector<Renderer::RenderObject>    renderObjects;
        GPU::DirLight                sun;
        std::vector<GPU::PointLight> pointLights;
        std::vector<GPU::SpotLight>  spotLights;
        Renderer::Objects::FreeCamera          camera;
        Renderer::IBL::IBLMaps                 iblMaps;
    private:
        std::string m_hdrMap;
    };
}

#endif
