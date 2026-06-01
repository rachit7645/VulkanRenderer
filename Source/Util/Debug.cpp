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

#include "Debug.h"

#include "Util/Types.h"

#ifdef _WIN32
#include <Windows.h>
#elif defined(__linux__)
#include <fstream>
#include <string>
#endif

namespace Util
{
    bool IsDebuggerPresent()
    {
        #ifdef _WIN32
        return ::IsDebuggerPresent();
        #elif defined(__linux__)
        constexpr std::string_view kTracerPid = "TracerPid:";

        auto status = std::ifstream("/proc/self/status");

        if (!status.is_open())
        {
            return false;
        }

        for (std::string line; std::getline(status, line);)
        {
            if (!line.starts_with(kTracerPid))
            {
                continue;
            }

            const auto value   = std::string_view(line).substr(kTracerPid.size());
            const auto trimmed = value.substr(value.find_first_not_of(" \t"));

            s32 processID = 0;

            const auto [_, error] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), processID);

            if (error != std::errc{})
            {
                return false;
            }

            return processID != 0;
        }

        return false;
        #else
        return false;
        #endif
    }

    void TriggerBreakpoint()
    {
        #ifdef ENGINE_DEBUG
        if (!IsDebuggerPresent())
        {
            return;
        }

        __builtin_debugtrap();
        #endif
    }
}