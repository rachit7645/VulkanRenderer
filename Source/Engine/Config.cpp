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

#include "Config.h"

#include <simdjson.h>

#include "Util/JSON.h"
#include "Util/Log.h"

namespace Engine
{
    Config::Config()
    {
        try
        {
            simdjson::ondemand::parser parser;

            const auto path = Util::Files::GetAssetPath("", "Config.json");
            const auto json = simdjson::padded_string::load(path);

            JSON::CheckError(json, "Failed to load json file!");

            auto document = parser.iterate(json);

            JSON::CheckError(document, "Failed to parse json file!");

            JSON::CheckError(document["Scene"].get_string(scene), "Failed to get scene file!");
        }
        catch (const std::exception& e)
        {
            Logger::Error("Failed to load config file! [Error={}]\n", e.what());
        }
    }
}