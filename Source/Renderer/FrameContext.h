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

#ifndef FRAME_CONTEXT_H
#define FRAME_CONTEXT_H

#include "Externals/Taskflow.h"
#include "Util/DeletionQueue.h"
#include "Util/Scratch.h"
#include "Util/Types.h"

namespace Renderer
{
    struct FrameContext
    {
        usize FIF        = 0;
        usize frameIndex = 0;

        tf::Executor&        executor;
        Scratch::Allocator&    scratchAllocator;
        Util::DeletionQueue& deletionQueue;
    };
}

#endif
