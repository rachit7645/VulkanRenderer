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

#include "Util/Unused.h"
#include "Vulkan/Context.h"
#include "Vulkan/Extensions.h"
#include "Vulkan/QueueFamilies.h"

#ifdef ENGINE_DLSS
#include "DLSS/DLSSConfig.h"
#endif

namespace Renderer
{
    class RenderConfig
    {
    public:
        enum class AntiAliasingMode : u8
        {
            // For the r/ftaa bros (not recommended!)
            None,
            TAA,
            #ifdef ENGINE_DLSS
            DLSS
            #endif
        };

        struct Entry
        {
            void Validate();

            bool isSupported = false;
            bool isEnabled   = false;
        };

        explicit RenderConfig(const Vk::Context& context);

        void Update();

        void Destroy(VkDevice device);

        #ifdef ENGINE_DLSS
        DLSS::DLSSConfig DLSSConfig;
        #endif

        RenderConfig::Entry multiQueue = {};
        RenderConfig::Entry DLSS       = {};

        AntiAliasingMode antiAliasingMode = AntiAliasingMode::TAA;

        f32 resolutionScale = 1.0f;
    private:
        void Validate();

        s32 m_currentAntiAliasingMode = 0;
    };
}

#endif