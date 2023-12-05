/*
 *    Copyright 2023 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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
        // Usings
        using Clock     = std::chrono::steady_clock;
        using TimePoint = std::chrono::time_point<Clock>;
        using Duration  = std::chrono::duration<f32, std::chrono::seconds::period>;

        // Reset counter
        void Reset();
        // Update frame counter
        void Update();

        // FPS
        f32 FPS = 0.0f;
        // Frame time
        f32 avgFrameTime = 0.0f;
    private:
        // Cycle start time
        TimePoint m_startTime = {};
        // Frame start time
        TimePoint m_frameStartTime = {};
        // Frame end time
        TimePoint m_endTime = {};
        // Current FPS
        f32 m_currentFPS = 0.0f;
    };

} // Util

#endif
