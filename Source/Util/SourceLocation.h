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

#ifndef SOURCELOCATION_H
#define SOURCELOCATION_H

#include <string_view>
#include <source_location>

#include "Util.h"

namespace Util
{
    constexpr std::string_view GetFunctionName(const std::source_location& location)
    {
        std::string_view fnName = location.function_name();

        // Find parameter parentheses
        usize startPos = fnName.find('(');

        // Backtrack to find the space character before the function name
        while (startPos > 0 && std::isspace(fnName[startPos - 1]))
        {
            --startPos;
        }

        // Find the space character before the function name
        usize spacePos = fnName.rfind(' ', startPos);

        return fnName.substr(spacePos + 1, startPos - spacePos - 1);
    }
}

#endif
