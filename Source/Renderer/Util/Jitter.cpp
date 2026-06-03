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

#include "Jitter.h"

#include "Renderer/RenderConfig.h"
#include "Renderer/RenderConstants.h"
#include "Util/Maths.h"

namespace Renderer
{
    glm::vec2 CalculateJitter(usize frameIndex, usize phaseCount)
    {
        const usize index = frameIndex % phaseCount;

        auto jitter  = glm::vec2(Maths::Halton(index, 2), Maths::Halton(index, 3));
             jitter -= glm::vec2(0.5f);

        return jitter;
    }

    usize GetPhaseCount(RenderConfig::AntiAliasingMode antiAliasingMode, VkExtent2D renderExtent, VkExtent2D displayExtent)
    {
        constexpr usize BASE_JITTER_PHASE_COUNT      = 64;
        constexpr usize BASE_DLSS_JITTER_PHASE_COUNT = 8;

        switch (antiAliasingMode)
        {
        case RenderConfig::AntiAliasingMode::None:
        case RenderConfig::AntiAliasingMode::TAA:
            return BASE_JITTER_PHASE_COUNT;

        #ifdef ENGINE_DLSS
        case RenderConfig::AntiAliasingMode::DLSS:
        {
            const f32 renderXResolution  = Maths::Max2(glm::vec2(glm::vk_cast(renderExtent)));
            const f32 displayXResolution = Maths::Max2(glm::vec2(glm::vk_cast(displayExtent)));

            const f32 multiplier  = displayXResolution / renderXResolution;
            const f32 multiplier2 = multiplier * multiplier;

            return std::ceil(static_cast<f32>(BASE_DLSS_JITTER_PHASE_COUNT) * multiplier2);
        }
        #endif

        default:
            Logger::Error("Unknown anti-aliasing mode!");
        }
    }

    glm::vec2 GetJitterInPixels
    (
        usize frameIndex,
        RenderConfig::AntiAliasingMode antiAliasingMode,
        VkExtent2D renderExtent,
        VkExtent2D displayExtent
    )
    {
        if (antiAliasingMode == RenderConfig::AntiAliasingMode::None)
        {
            return glm::vec2(0.0f);
        }

        const usize phaseCount = GetPhaseCount(antiAliasingMode, renderExtent, displayExtent);

        return CalculateJitter(frameIndex, phaseCount);
    }

    glm::vec2 GetJitter
    (
        usize frameIndex,
        RenderConfig::AntiAliasingMode antiAliasingMode,
        VkExtent2D renderExtent,
        VkExtent2D displayExtent
    )
    {
        const auto jitter = GetJitterInPixels
        (
            frameIndex,
            antiAliasingMode,
            renderExtent,
            displayExtent
        );

        return jitter / glm::vec2(glm::vk_cast(renderExtent));
    }
}