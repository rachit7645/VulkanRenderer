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

#ifndef LOCAL_LIGHTS_H
#define LOCAL_LIGHTS_H

#include "GPU/Lights.h"

namespace Models
{
    struct LocalPointLight
    {
        GPU::PointLight Transform(const glm::mat4& matrix) const;

        glm::mat4       transform;
        GPU::PointLight light;
    };

    struct LocalSpotLight
    {
        GPU::SpotLight Transform(const glm::mat4& matrix) const;

        glm::mat4      transform;
        GPU::SpotLight light;
    };
}

#endif