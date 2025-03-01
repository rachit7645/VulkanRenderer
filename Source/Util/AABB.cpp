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

#include "AABB.h"

namespace Maths
{
    AABB::AABB(const std::span<const glm::vec3> positions)
    {
        if (positions.empty())
        {
            min = max = glm::vec3(0.0f);
            return;
        }

        min = glm::vec3(std::numeric_limits<f32>::max());
        max = glm::vec3(std::numeric_limits<f32>::lowest());

        for (const auto& vertex : positions)
        {
            min = glm::min(min, vertex);
            max = glm::max(max, vertex);
        }
    }

    AABB::AABB(const glm::vec3& min, const glm::vec3& max)
        : min(min),
          max(max)
    {
    }

    glm::vec3 AABB::GetCenter() const
    {
        return (max + min) / 2.0f;
    }

    glm::vec3 AABB::GetExtent() const
    {
        return (max - min) / 2.0f;
    }

    AABB AABB::Transform(const glm::mat4& transform) const
    {
        const std::array corners =
        {
            glm::vec3(min.x, min.y, min.z),
            glm::vec3(min.x, min.y, max.z),
            glm::vec3(min.x, max.y, min.z),
            glm::vec3(min.x, max.y, max.z),
            glm::vec3(max.x, min.y, min.z),
            glm::vec3(max.x, min.y, max.z),
            glm::vec3(max.x, max.y, min.z),
            glm::vec3(max.x, max.y, max.z),
        };

        auto newMin = glm::vec3(std::numeric_limits<f32>::max());
        auto newMax = glm::vec3(std::numeric_limits<f32>::lowest());

        for (const auto& corner : corners)
        {
            const auto transformedCorner = glm::vec3(transform * glm::vec4(corner, 1.0f));

            newMin = glm::min(newMin, transformedCorner);
            newMax = glm::max(newMax, transformedCorner);
        }

        return {min, max};
    }
}
