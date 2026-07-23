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

#include "Log.h"

#include "Files.h"
#include "Debug.h"
#include "Time.h"

namespace Logger::Detail
{
    void Log
    (
        const fmt::color& fgColor,
        const std::string_view type,
        const std::source_location& location,
        const std::string_view format,
        const fmt::format_args args
    )
    {
        const auto foreground = fmt::fg(fgColor);

        fmt::print
        (
            stdout,
            foreground,
            "[{}] [{}] [{}:{}] ",
            type,
            Util::GetTime(),
            Files::GetName(location.file_name()),
            location.line()
        );

        fmt::vprint(stdout, foreground, format, args);
    }

    void LogAndExit
    (
        ENGINE_UNUSED const s32 errorCode,
        const fmt::color& fgColor,
        const std::string_view type,
        const std::source_location& location,
        const std::stacktrace& stacktrace,
        const std::string_view format,
        const fmt::format_args args
    )
    {
        const auto foreground = fmt::fg(fgColor);

        fmt::print
        (
            stderr,
            foreground,
            "[{}] [{}] [{}:{}] ",
            type,
            Util::GetTime(),
            Files::GetName(location.file_name()),
            location.line()
        );

        fmt::vprint(stderr, foreground, format, args);

        fmt::print
        (
            stderr,
            foreground,
            "\n{}\n",
            std::to_string(stacktrace)
        );

        Util::TriggerBreakpoint();

        #ifdef ENGINE_DEBUG
        while (true) {}
        #else
        std::exit(errorCode);
        #endif

    }
}