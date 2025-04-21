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

#ifndef FRAME_COUNTER_H
#define FRAME_COUNTER_H

#include <chrono>

#include "Util.h"

namespace Util
{
    class FrameCounter
    {
    public:
        using Clock     = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        void Reset();
        void Update();

        f32 FPS          = 0.0f;
        f32 avgFrameTime = 0.0f;
        f32 frameDelta   = 0.0f;
    private:
        TimePoint m_startTime      = {};
        TimePoint m_frameStartTime = {};
        TimePoint m_endTime        = {};

        f32 m_currentFPS = 0.0f;
    };

}

#endif
