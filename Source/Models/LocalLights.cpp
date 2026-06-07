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

#include "LocalLights.h"

namespace Models
{
    GPU::PointLight LocalPointLight::Transform(const glm::mat4& matrix) const
    {
        GPU::PointLight transformedLight = light;

        const glm::mat4 combined = matrix * transform;

        transformedLight.position = glm::vec3(combined[3]);

        return transformedLight;
    }

    GPU::SpotLight LocalSpotLight::Transform(const glm::mat4& matrix) const
    {
        GPU::SpotLight transformedLight = light;

        const glm::mat4 combined = matrix * transform;

        transformedLight.position  = glm::vec3(combined[3]);
        transformedLight.direction = glm::normalize(glm::mat3(combined) * glm::vec3(0.0f, 0.0f, -1.0f));

        return transformedLight;
    }
}
