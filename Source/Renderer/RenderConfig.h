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

#ifndef RENDER_CONFIG_H
#define RENDER_CONFIG_H
#include "Vulkan/Extensions.h"
#include "Vulkan/QueueFamilies.h"

namespace Renderer
{
    class RenderConfig
    {
    public:
        struct Entry
        {
            void Validate();

            bool isSupported = false;
            bool isEnabled   = false;
        };

        RenderConfig(const Vk::QueueFamilies& queueFamilies, const Vk::Extensions& extensions);

        void Update();

        RenderConfig::Entry rayTracing = {};
        RenderConfig::Entry multiQueue = {};
    private:
        void Validate();
    };
}

#endif