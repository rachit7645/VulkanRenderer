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

#ifndef AABB_H
#define AABB_H

#include "Util.h"

namespace Maths
{
    class AABB
    {
    public:
        AABB() = default;
        explicit AABB(const std::span<const glm::vec3> positions);
        AABB(const glm::vec3& min, const glm::vec3& max);

        [[nodiscard]] glm::vec3 GetCenter() const;
        [[nodiscard]] glm::vec3 GetExtent() const;

        [[nodiscard]] AABB Transform(const glm::mat4& transform) const;

        glm::vec3 min = glm::vec3(std::numeric_limits<f32>::max());
        glm::vec3 max = glm::vec3(std::numeric_limits<f32>::lowest());
    };
}

#endif
