//    Copyright 2023 - 2024 Rachit Khandelwal
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#ifndef FORWARD_INDIRECT_BUFFER_H
#define FORWARD_INDIRECT_BUFFER_H

#include "Renderer/RenderObject.h"
#include "Util/Util.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Constants.h"
#include "Externals/VMA.h"
#include "Models/ModelManager.h"

namespace Renderer::Forward
{
    class IndirectBuffer
    {
    public:
        IndirectBuffer() = default;
        IndirectBuffer(VmaAllocator allocator);

        u32 WriteDrawCalls
        (
            usize FIF,
            const Models::ModelManager& modelManager,
            const std::vector<Renderer::RenderObject>& renderObjects
        );

        void Destroy(VmaAllocator allocator);

        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> buffers;
    };
}

#endif
