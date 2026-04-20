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

#include "FrameCounter.h"

#include "Externals/ImGui.h"
#include "Externals/Tracy.h"

namespace Util
{
    void FrameCounter::Reset()
    {
        m_FPS          = 0.0f;
        m_frameCount = 0;

        m_startTime      = Clock::now();
        m_frameStartTime = m_startTime;
    }

    void FrameCounter::Update()
    {
        const auto now = Clock::now();

        const auto frameDuration = now - m_frameStartTime;
        const auto cycleDuration = now - m_startTime;

        frameDelta       = std::chrono::duration<f32>(frameDuration).count();
        m_frameStartTime = now;

        ++m_frameCount;

        const f32 frameTime = std::chrono::duration<f32, std::milli>(frameDuration).count();

        if (cycleDuration >= std::chrono::seconds(1))
        {
            const auto seconds = std::chrono::duration<f64>(cycleDuration).count();

            m_FPS = static_cast<f32>(static_cast<f64>(m_frameCount) / seconds);

            m_frameCount = 0;
            m_startTime  = now;
        }

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Profiler"))
            {
                ImGui::Text("FPS        | %.3f",    m_FPS);
                ImGui::Text("Frame Time | %.4f ms", frameTime);

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        #ifdef ENGINE_PROFILE
        TracyPlot("Frame Time", frameTime);
        #endif
    }
}