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
    constexpr usize PLOT_FRAME_TIME_MAX_COUNT = 25;
    constexpr auto  PLOT_UPDATE_FREQUENCY     = std::chrono::milliseconds(200);

    void FrameCounter::Reset()
    {
        m_FPS          = 0.0f;
        m_frameCount = 0;

        m_plotFrameTimes.clear();

        m_startTime      = Clock::now();
        m_frameStartTime = m_startTime;
        m_plotStartTime  = m_startTime;
    }

    void FrameCounter::Update()
    {
        const auto now = Clock::now();

        const auto frameDuration = now - m_frameStartTime;
        const auto cycleDuration = now - m_startTime;
        const auto plotDuration  = now - m_plotStartTime;

        frameDelta       = std::chrono::duration<f32>(frameDuration).count();
        m_frameStartTime = now;

        ++m_frameCount;

        const f32 frameTime = std::chrono::duration<f32, std::milli>(frameDuration).count();

        if (plotDuration >= PLOT_UPDATE_FREQUENCY)
        {
            if (m_plotFrameTimes.size() >= PLOT_FRAME_TIME_MAX_COUNT)
            {
                m_plotFrameTimes.erase(m_plotFrameTimes.begin());
            }

            m_plotFrameTimes.emplace_back(frameTime);

            m_plotStartTime = now;
        }

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

                ImGui::Separator();

                if (ImPlot::BeginPlot("Frame Time", ImVec2(ImGui::GetContentRegionAvail().x, 0), ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText))
                {
                    ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupAxisLimits(ImAxis_X1, 0, PLOT_FRAME_TIME_MAX_COUNT, ImGuiCond_Always);
                    ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0f, 50.0f);

                    ImPlotSpec spec;
                    spec.FillColor = IMPLOT_AUTO_COL;
                    spec.FillAlpha = 0.3f;
                    spec.Flags     = ImPlotLineFlags_Shaded;

                    ImPlot::PlotLine
                    (
                        "##FrameTime",
                        m_plotFrameTimes.data(),
                        static_cast<s32>(m_plotFrameTimes.size()),
                        1.0,
                        0.0,
                        spec
                    );

                    ImPlot::EndPlot();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        #ifdef ENGINE_PROFILE
        TracyPlot("Frame Time", frameTime);
        #endif
    }
}