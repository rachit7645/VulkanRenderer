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

#ifndef JSON_H
#define JSON_H

#include "GPU/Lights.h"
#include "Renderer/Objects/FreeCamera.h"
#include "Externals/SIMDJSON.h"

namespace JSON
{
    void CheckError(simdjson::error_code result, const std::string_view message);

    template<typename T>
    void CheckError(const simdjson::simdjson_result<T>& result, const std::string_view message)
    {
        CheckError(result.error(), message);
    };

    template<glm::length_t L>
    simdjson::simdjson_result<glm::vec<L, f32, glm::defaultp>> ParseVector(simdjson::ondemand::array& array)
    {
        glm::vec<L, f32, glm::defaultp> output;

        for (usize i = 0; i < L; ++i)
        {
            array.reset();

            auto value = array.at(i);

            if (value.error() == simdjson::error_code::INDEX_OUT_OF_BOUNDS)
            {
                break;
            }

            if (value.error() != simdjson::error_code::SUCCESS)
            {
                return value.error();
            }

            auto valueAsDouble = value.get_double();

            if (valueAsDouble.error() != simdjson::error_code::SUCCESS)
            {
                return valueAsDouble.error();
            }

            output[i] = static_cast<f32>(valueAsDouble.value());
        }

        return output;
    }
}

namespace simdjson
{
    template <typename simdjson_value>
    auto tag_invoke(deserialize_tag, simdjson_value& val, glm::vec2& vector)
    {
        ondemand::array array;

        if (const auto error = val.get_array().get(array); error != error_code::SUCCESS)
        {
            return error;
        }

        auto parsed = JSON::ParseVector<glm::vec2::length()>(array);

        if (parsed.error() != error_code::SUCCESS)
        {
            return parsed.error();
        }

        vector = parsed.value();

        return error_code::SUCCESS;
    }

    template <typename simdjson_value>
    auto tag_invoke(deserialize_tag, simdjson_value& val, glm::vec3& vector)
    {
        ondemand::array array;

        if (const auto error = val.get_array().get(array); error != error_code::SUCCESS)
        {
            return error;
        }

        auto parsed = JSON::ParseVector<glm::vec3::length()>(array);

        if (parsed.error() != error_code::SUCCESS)
        {
            return parsed.error();
        }

        vector = parsed.value();

        return error_code::SUCCESS;
    }

    template <typename simdjson_value>
    auto tag_invoke(deserialize_tag, simdjson_value& val, GPU::DirLight& light)
    {
        ondemand::object object;

        if (const auto error = val.get_object().get(object); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Position"].get<glm::vec3>(light.position); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Color"].get<glm::vec3>(light.color); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Intensity"].get<glm::vec3>(light.intensity); error != error_code::SUCCESS)
        {
            return error;
        }

        return error_code::SUCCESS;
    }

    template <typename simdjson_value>
    auto tag_invoke(deserialize_tag, simdjson_value& val, GPU::PointLight& light)
    {
        ondemand::object object;

        if (const auto error = val.get_object().get(object); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Position"].get<glm::vec3>(light.position); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Color"].get<glm::vec3>(light.color); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Intensity"].get<glm::vec3>(light.intensity); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Range"].get<f32>(light.range); error != error_code::SUCCESS)
        {
            return error;
        }

        return error_code::SUCCESS;
    }

    template <typename simdjson_value>
    auto tag_invoke(deserialize_tag, simdjson_value& val, GPU::SpotLight& light)
    {
        ondemand::object object;

        if (const auto error = val.get_object().get(object); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Position"].get<glm::vec3>(light.position); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Color"].get<glm::vec3>(light.color); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Intensity"].get<glm::vec3>(light.intensity); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Direction"].get<glm::vec3>(light.direction); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["CutOff"].get<glm::vec2>(light.cutOff); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Range"].get<f32>(light.range); error != error_code::SUCCESS)
        {
            return error;
        }

        light.cutOff = glm::radians(light.cutOff);

        return error_code::SUCCESS;
    }

    template <typename simdjson_value>
    auto tag_invoke(deserialize_tag, simdjson_value& val, Renderer::Objects::FreeCamera& camera)
    {
        ondemand::object object;

        if (const auto error = val.get_object().get(object); error != error_code::SUCCESS)
        {
            return error;
        }

        // Camera data
        glm::vec3 position    = {};
        glm::vec3 rotation    = {};
        f32       FOV         = 0.0f;
        f32       speed       = 0.0f;
        f32       sensitivity = 0.0f;
        f32       zoom        = 0.0f;

        if (const auto error = object["Position"].get<glm::vec3>(position); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Rotation"].get<glm::vec3>(rotation); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["FOV"].get<f32>(FOV); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Speed"].get<f32>(speed); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Sensitivity"].get<f32>(sensitivity); error != error_code::SUCCESS)
        {
            return error;
        }

        if (const auto error = object["Zoom"].get<f32>(zoom); error != error_code::SUCCESS)
        {
            return error;
        }

        rotation = glm::radians(rotation);
        FOV      = glm::radians(FOV);

        camera = Renderer::Objects::FreeCamera(position, rotation, FOV, speed, sensitivity, zoom);

        return error_code::SUCCESS;
    }
}

#endif
