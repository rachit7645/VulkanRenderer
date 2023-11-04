/*
 *    Copyright 2023 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef LOG_H
#define LOG_H

#include <string>
#include <string_view>
#include <source_location>
#include <cstdlib>

#include <fmt/format.h>
#include <fmt/color.h>

#include "Time.h"
#include "Util.h"

namespace Logger
{
    // Internal namespace
    namespace Detail
    {
        // Get file name from directory
        constexpr std::string_view GetFileName(std::string_view fileName)
        {
            // Get last slash
            usize lastSlash = fileName.find_last_of(std::filesystem::path::preferred_separator);

            // Check
            if (lastSlash != std::string_view::npos)
            {
                // Return name
                return fileName.substr(lastSlash + 1);
            }
            else
            {
                // Return already fine name
                return fileName;
            }
        }

        template <typename... Args>
        void Log
        (
            const fmt::color& fgColor,
            const fmt::color& bgColor,
            const std::string_view type,
            const std::source_location location,
            const std::string_view format,
            Args&& ... args
        )
        {
            // Format & print additional data
            fmt::print
            (
                stderr,
                fmt::fg(fgColor) | fmt::bg(bgColor),
                std::string("[{}] [{}] [{}:{}] ") + format.data(),
                type,
                Util::GetTime(),
                GetFileName(location.file_name()),
                location.line(),
                args...
            );
        }

        // Error logger
        template <s32 ErrorCode, typename... Args>
        [[noreturn]] void LogAndExit
        (
            const fmt::color& fgColor,
            const fmt::color& bgColor,
            const std::string_view type,
            const std::source_location location,
            const std::string_view format,
            Args&& ... args
        )
        {
            // Call regular logger
            Log
            (
                fgColor,
                bgColor,
                type,
                location,
                format,
                args...
            );
            // Exit
            std::exit(ErrorCode);
        }
    }

    // Info logger
    template <typename... Args>
    struct Info
    {
        explicit Info
        (
            const std::string_view format,
            Args&&... args,
			const std::source_location location = std::source_location::current()
        )
        {
            // Log
            Detail::Log
            (
                fmt::color::green,
                fmt::color::black,
                "INFO",
                location,
                format,
                args...
            );
        }
    };

    // Warning logger
    template <typename... Args>
    struct Warning
    {
        explicit Warning
        (
            const std::string_view format,
            Args&&... args,
			const std::source_location location = std::source_location::current()
        )
        {
            // Log
            Detail::Log
            (
                fmt::color::yellow,
                fmt::color::black,
                "WARNING",
                location,
                format,
                args...
            );
        }
    };

    // Debug logger
    template <typename... Args>
    struct Debug
    {
        explicit Debug
        (
            const std::string_view format,
            Args&&... args,
			const std::source_location location = std::source_location::current()
        )
        {
            #ifdef ENGINE_DEBUG
            // Log
            Detail::Log
            (
                fmt::color::cyan,
                fmt::color::black,
                "DEBUG",
                location,
                format,
                args...
            );
            #endif
        }
    };

    // Vulkan logger
    template <typename... Args>
    struct Vulkan
    {
        explicit Vulkan
        (
            const std::string_view format,
            Args&&... args,
			const std::source_location location = std::source_location::current()
        )
        {
            #ifdef ENGINE_DEBUG
            // Log
            Detail::Log
            (
                fmt::color::orange,
                fmt::color::black,
                "VULKAN",
                location,
                format,
                args...
            );
            #endif
        }
    };

    // Error logger
    template <typename... Args>
    struct Error
    {
        [[noreturn]] explicit Error
        (
            const std::string_view format,
            Args&&... args,
			const std::source_location location = std::source_location::current()
        )
        {
            // Log
            Detail::LogAndExit<EXIT_FAILURE>
            (
                fmt::color::red,
                fmt::color::black,
                "ERROR",
                location,
                format,
                args...
            );
        }
    };

    // Deduction guides
    template <typename... Args>
    Info(std::string_view, Args&&...) -> Info<Args...>;
    template <typename... Args>
    Warning(std::string_view, Args&&...) -> Warning<Args...>;
    template <typename... Args>
    Debug(std::string_view, Args&&...) -> Debug<Args...>;
    template <typename... Args>
    Vulkan(std::string_view, Args&&...) -> Vulkan<Args...>;
    template <typename... Args>
    Error(std::string_view, Args&&...) -> Error<Args...>;
}

#endif