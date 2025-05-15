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

#ifndef SCENE_BUFFER_H
#define SCENE_BUFFER_H

#include "LightsBuffer.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Constants.h"
#include "Engine/Scene.h"
#include "GPU/Scene.h"

namespace Renderer::Buffers
{
    class SceneBuffer
    {
    public:
        SceneBuffer(VkDevice device, VmaAllocator allocator);

        void WriteScene
        (
            usize FIF,
            usize frameIndex,
            VmaAllocator allocator,
            VkExtent2D swapchainExtent,
            const Engine::Scene& scene
        );

        void Destroy(VmaAllocator allocator);

        GPU::SceneBuffer gpuScene = {};

        Buffers::LightsBuffer lightsBuffer;

        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> buffers;
    };
}

#endif
