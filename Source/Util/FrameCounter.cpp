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

#include "FrameCounter.h"

#include "Externals/ImGui.h"

namespace Util
{
    // Alias for chrono
    namespace chrono = std::chrono;

    void FrameCounter::Reset()
    {
        FPS          = 0.0f;
        avgFrameTime = 0.0f;
        m_currentFPS = 0.0f;

        m_startTime      = Clock::now();
        m_frameStartTime = m_startTime;

        m_endTime = {};
    }

    void FrameCounter::Update()
    {
        m_endTime = Clock::now();

        const auto duration      = m_endTime - m_frameStartTime;
        const auto cycleDuration = m_endTime - m_startTime;

        // This is in us (I think)
        frameDelta       = static_cast<f32>(static_cast<f64>(duration.count()) / 1000.0);
        m_frameStartTime = m_endTime;

        // A cycle is a second long
        if (cycleDuration >= chrono::seconds(1))
        {
            m_startTime  = m_endTime;
            FPS          = m_currentFPS;
            avgFrameTime = static_cast<f32>(1000.0 / FPS);
            m_currentFPS = 0.0f;
        }
        else
        {
            ++m_currentFPS;
        }

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Profiler"))
            {
                // Frame stats
                ImGui::Text("FPS         | %.2f",    FPS);
                ImGui::Text("Frame Time  | %.2f ms", avgFrameTime);
                ImGui::Text("Frame Delta | %.2f us", frameDelta);

                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }
}