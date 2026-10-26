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

#ifndef DLSS_EVALUATION_H
#define DLSS_EVALUATION_H

#include "DLSSConfig.h"
#include "Engine/FrameCounter.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/FramebufferManager.h"

namespace Renderer::DLSS
{
    class Evaluation
    {
    public:
        void Evaluate
        (
            usize frameIndex,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Engine::FrameCounter& frameCounter,
            DLSS::DLSSConfig& config
        );
    };
}

#endif