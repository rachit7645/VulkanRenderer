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

#include "FreeCamera.h"
#include "Renderer/RenderObject.h"
#include "Renderer/IBL/IBLMaps.h"
#include "Renderer/IBL/Generator.h"
#include "Models/ModelManager.h"
#include "GPU/Lights.h"

namespace Engine
{
    struct Scene
    {
        std::vector<Renderer::RenderObject> renderObjects = {};
        GPU::DirLight                       sun           = {};
        std::vector<GPU::PointLight>        pointLights   = {};
        std::vector<GPU::SpotLight>         spotLights    = {};
        Engine::FreeCamera                  camera        = {};
        Renderer::IBL::IBLID                iblMapsID     = 0;

        // TODO: Better render object update flags
        // This does not account for render object internal changes
        // Only addition/deletion of render objects will update this
        bool haveRenderObjectsChanged = false;
    };
}

#endif
