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

#include "FrameCounter.h"

#include "Externals/ImGui.h"

namespace Util
{
    // Aliases
    namespace chrono = std::chrono;

    void FrameCounter::Reset()
    {
        // Reset floats
        FPS = avgFrameTime = m_currentFPS = 0.0f;
        // Reset times
        m_startTime = m_frameStartTime = Clock::now();
        // Reset end time
        m_endTime = {};
    }

    void FrameCounter::Update()
    {
        // Calculate end time
        m_endTime = Clock::now();

        // Calculate frame duration
        auto duration = m_endTime - m_frameStartTime;
        // Calculate cycle duration
        auto cycleDuration = m_endTime - m_startTime;

        // Set frame delta
        frameDelta = static_cast<f32>(static_cast<f64>(duration.count()) / 1000.0);
        // Set this/next frame's start time
        m_frameStartTime = m_endTime;

        // If a second has passed
        if (cycleDuration >= chrono::seconds(1))
        {
            // Set this cycle's start time
            m_startTime = m_endTime;
            // Update displayed FPS
            FPS = m_currentFPS;
            // Calculate frame time
            avgFrameTime = static_cast<f32>(1000.0 / FPS);
            // Reset FPS
            m_currentFPS = 0.0f;
        }
        else
        {
            // Increment FPS
            ++m_currentFPS;
        }

        // Render ImGui
        if (ImGui::BeginMainMenuBar())
        {
            // Profiler
            if (ImGui::BeginMenu("Profiler"))
            {
                // Frame stats
                ImGui::Text("FPS: %.2f", FPS);
                ImGui::Text("Frame Time: %.2f ms", avgFrameTime);
                ImGui::Text("Frame Delta: %.2f", frameDelta);
                ImGui::EndMenu();
            }
            // End menu bar
            ImGui::EndMainMenuBar();
        }
    }
}