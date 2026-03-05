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

#include "RenderConstants.h"
#include "Util/Maths.h"

namespace Renderer
{
    glm::vec2 GetJitterInPixels(usize frameIndex, VkExtent2D renderExtent, VkExtent2D displayExtent)
    {
        const f32 renderXResolution  = Maths::Max2(glm::vec2(glm::vk_cast(renderExtent)));
        const f32 displayXResolution = Maths::Max2(glm::vec2(glm::vk_cast(displayExtent)));

        const f32 multiplier  = renderXResolution / displayXResolution;
        const f32 multiplier2 = multiplier * multiplier;

        const auto phaseCount = static_cast<usize>(static_cast<f32>(Renderer::JITTER_SAMPLE_COUNT) * multiplier2);

        const usize index = frameIndex % phaseCount;

        auto jitter  = glm::vec2(Maths::Halton(index, 2), Maths::Halton(index, 3));
             jitter -= glm::vec2(0.5f);

        return jitter;
    }

    glm::vec2 GetJitter(usize frameIndex, VkExtent2D renderExtent, VkExtent2D displayExtent)
    {
        const auto jitter = GetJitterInPixels(frameIndex, renderExtent, displayExtent);

        return jitter / glm::vec2(renderExtent.width, renderExtent.height);
    }
}