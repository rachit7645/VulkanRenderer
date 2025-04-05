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

#ifndef LIGHTS_H
#define LIGHTS_H

#include "Externals/GLM.h"
#include "Util/Util.h"

namespace Renderer::Objects
{
    constexpr u32 MAX_DIR_LIGHT_COUNT = 1;

    constexpr u32 MAX_POINT_LIGHT_COUNT          = 16;
    constexpr u32 MAX_SHADOWED_POINT_LIGHT_COUNT = 4;

    constexpr u32 MAX_SPOT_LIGHT_COUNT           = 16;
    constexpr u32 MAX_SHADOWED_SPOT_LIGHT_COUNT  = 4;

    constexpr u32 MAX_TOTAL_POINT_LIGHT_COUNT = MAX_SHADOWED_POINT_LIGHT_COUNT + MAX_POINT_LIGHT_COUNT;
    constexpr u32 MAX_TOTAL_SPOT_LIGHT_COUNT  = MAX_SHADOWED_SPOT_LIGHT_COUNT  + MAX_SPOT_LIGHT_COUNT;

    constexpr u32 MAX_LIGHT_COUNT = MAX_DIR_LIGHT_COUNT + MAX_TOTAL_POINT_LIGHT_COUNT + MAX_TOTAL_SPOT_LIGHT_COUNT;

    constexpr glm::uvec2 POINT_SHADOW_DIMENSIONS   = {512,  512};
    constexpr glm::vec2  POINT_LIGHT_SHADOW_PLANES = {1.0f, 25.0f};

    constexpr glm::uvec2 SPOT_LIGHT_SHADOW_DIMENSIONS = {1024, 1024};
    constexpr glm::vec2  SPOT_LIGHT_SHADOW_PLANES     = {0.1f, 100.0f};

    struct DirLight
    {
        glm::vec3 position  = {0.0f, 0.0f, 0.0f};
        glm::vec3 color     = {0.0f, 0.0f, 0.0f};
        glm::vec3 intensity = {0.0f, 0.0f, 0.0f};
    };

    struct PointLight
    {
        glm::vec3 position    = {0.0f, 0.0f, 0.0f};
        glm::vec3 color       = {0.0f, 0.0f, 0.0f};
        glm::vec3 intensity   = {0.0f, 0.0f, 0.0f};
        glm::vec3 attenuation = {0.0f, 0.0f, 0.0f};
    };

    struct ShadowedPointLight
    {
        ShadowedPointLight() = default;

        ShadowedPointLight(const PointLight& pointLight)
            : position(pointLight.position),
              color(pointLight.color),
              intensity(pointLight.intensity),
              attenuation(pointLight.attenuation)
        {
            const auto projection = glm::perspectiveRH_ZO
            (
                glm::radians(90.0f),
                static_cast<f32>(Objects::POINT_SHADOW_DIMENSIONS.x) / static_cast<f32>(Objects::POINT_SHADOW_DIMENSIONS.y),
                Objects::POINT_LIGHT_SHADOW_PLANES.x,
                Objects::POINT_LIGHT_SHADOW_PLANES.y
            );

            matrices[0] = projection * glm::lookAtRH(position, position + glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f));
            matrices[1] = projection * glm::lookAtRH(position, position + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f));
            matrices[2] = projection * glm::lookAtRH(position, position + glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f));
            matrices[3] = projection * glm::lookAtRH(position, position + glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f));
            matrices[4] = projection * glm::lookAtRH(position, position + glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f));
            matrices[5] = projection * glm::lookAtRH(position, position + glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f));
        }
        
        glm::vec3 position    = {0.0f, 0.0f, 0.0f};
        glm::vec3 color       = {0.0f, 0.0f, 0.0f};
        glm::vec3 intensity   = {0.0f, 0.0f, 0.0f};
        glm::vec3 attenuation = {0.0f, 0.0f, 0.0f};
        glm::mat4 matrices[6] = {};
    };

    struct SpotLight
    {
        glm::vec3 position    = {0.0f, 0.0f, 0.0f};
        glm::vec3 color       = {0.0f, 0.0f, 0.0f};
        glm::vec3 intensity   = {0.0f, 0.0f, 0.0f};
        glm::vec3 attenuation = {0.0f, 0.0f, 0.0f};
        glm::vec3 direction   = {0.0f, 0.0f, 0.0f};
        glm::vec2 cutOff      = {0.0f, 0.0f};
    };

    struct ShadowedSpotLight
    {
        ShadowedSpotLight() = default;

        ShadowedSpotLight(const SpotLight& spotLight)
            : position(spotLight.position),
              color(spotLight.color),
              intensity(spotLight.intensity),
              attenuation(spotLight.attenuation),
              direction(spotLight.direction),
              cutOff(spotLight.cutOff)
        {
            const glm::mat4 projection = glm::perspectiveRH_ZO
            (
                glm::radians(90.0f),
                1.0f,
                Objects::SPOT_LIGHT_SHADOW_PLANES.x,
                Objects::SPOT_LIGHT_SHADOW_PLANES.y
            );

            const glm::mat4 view = glm::lookAt
            (
               position,
               glm::vec3(0.0f, 0.0f, 0.0f),
               glm::vec3(0.0f, 1.0f, 0.0f)
            );

            matrix = projection * view;
        }

        glm::vec3 position    = {0.0f, 0.0f, 0.0f};
        glm::vec3 color       = {0.0f, 0.0f, 0.0f};
        glm::vec3 intensity   = {0.0f, 0.0f, 0.0f};
        glm::vec3 attenuation = {0.0f, 0.0f, 0.0f};
        glm::vec3 direction   = {0.0f, 0.0f, 0.0f};
        glm::vec2 cutOff      = {0.0f, 0.0f};
        glm::mat4 matrix      = glm::identity<glm::mat4>();
    };
}

#endif