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

#ifndef LOG_H
#define LOG_H

#include <string_view>
#include <source_location>
#include <cstdlib>

#include <fmt/format.h>
#include <fmt/color.h>

#include "Time.h"
#include "Util.h"
#include "Files.h"

namespace Logger
{
    // Internal namespace
    namespace Detail
    {
         /// @brief Internal logging function
         /// @param fgColor  Foreground color for the terminal
         /// @param type     Logger Type
         /// @param location Source Location Information
         /// @param format   Format string
         /// @param args     Variable arguments
        template <typename... Args>
        void Log
        (
            const fmt::color& fgColor,
            const std::string_view type,
            const std::source_location location,
            const std::string_view format,
            Args&&... args
        )
        {
            // Format & print additional data
            fmt::print
            (
                stderr,
                fmt::fg(fgColor),
                fmt::runtime(std::string("[{}] [{}] [{}:{}] ") + format.data()),
                type,
                Util::GetTime(),
                Util::Files::GetName(location.file_name()),
                location.line(),
                std::forward<Args>(args)...
            );
        }

        /// @brief Internal error logger
        /// @param fgColor  Foreground color for the terminal
        /// @param type     Logger Type
        /// @param location Source Location Information
        /// @param format   Format string
        /// @param args     Variable arguments
        template <s32 ErrorCode = EXIT_FAILURE, typename... Args>
        [[noreturn]] void LogAndExit
        (
            const fmt::color& fgColor,
            const std::string_view type,
            const std::source_location location,
            const std::string_view format,
            Args&&... args
        )
        {
            // Call regular logger
            Log
            (
                fgColor,
                type,
                location,
                format,
                std::forward<Args>(args)...
            );

            #ifdef ENGINE_DEBUG
            while (true) {}
            #else
            std::exit(ErrorCode);
            #endif
        }
    }

    // Info logger
    template <typename... Args>
    struct Info
    {
        /// @brief Info logging function
        /// @param format   Format string
        /// @param args     Variable arguments
        /// @param location Source location information
        explicit Info
        (
            const std::string_view format,
            Args&&... args,
			const std::source_location location = std::source_location::current()
        )
        {
            Detail::Log
            (
                fmt::color::forest_green,
                "INFO",
                location,
                format,
                std::forward<Args>(args)...
            );
        }
    };

    // Warning logger
    template <typename... Args>
    struct Warning
    {
        /// @brief Warning logging function
        /// @param format   Format string
        /// @param args     Variable arguments
        /// @param location Source location information
        explicit Warning
        (
            const std::string_view format,
            Args&&... args,
			const std::source_location location = std::source_location::current()
        )
        {
            Detail::Log
            (
                fmt::color::yellow,
                "WARNING",
                location,
                format,
                std::forward<Args>(args)...
            );
        }
    };

    // Debug logger
    template <typename... Args>
    struct Debug
    {
        /// @brief Debug logging function
        /// @note  Enabled only if ENGINE_DEBUG is defined
        /// @param format   Format string
        /// @param args     Variable arguments
        /// @param location Source location information
        explicit Debug
        (
            ENGINE_UNUSED const std::string_view format,
            ENGINE_UNUSED Args&&... args,
            ENGINE_UNUSED const std::source_location location = std::source_location::current()
        )
        {
            #ifdef ENGINE_DEBUG
            Detail::Log
            (
                fmt::color::cyan,
                "DEBUG",
                location,
                format,
                std::forward<Args>(args)...
            );
            #endif
        }
    };

    // Vulkan logger
    template <typename... Args>
    struct Vulkan
    {
        /// @brief Vulkan validation layer logging function
        /// @note  Enabled only if ENGINE_DEBUG is defined
        /// @param format   Format string
        /// @param args     Variable arguments
        /// @param location Source location information
        explicit Vulkan
        (
            ENGINE_UNUSED const std::string_view format,
            ENGINE_UNUSED Args&&... args,
            ENGINE_UNUSED const std::source_location location = std::source_location::current()
        )
        {
            #ifdef ENGINE_DEBUG
            Detail::Log
            (
                fmt::color::orange,
                "VULKAN",
                location,
                format,
                std::forward<Args>(args)...
            );
            #endif
        }
    };

    // Error logger
    template <typename... Args>
    struct Error
    {
        /// @brief Error logging function
        /// @note  Will exit the program
        /// @param format   Format string
        /// @param args     Variable arguments
        /// @param location Source location information
        [[noreturn]] explicit Error
        (
            const std::string_view format,
            Args&&... args,
			const std::source_location location = std::source_location::current()
        )
        {
            Detail::LogAndExit<-1>
            (
                fmt::color::red,
                "ERROR",
                location,
                format,
                std::forward<Args>(args)...
            );
        }
    };

    // Vulkan error logger
    template <typename... Args>
    struct VulkanError
    {
        /// @brief Vulkan error logging function
        /// @param format   Format string
        /// @param args     Variable arguments
        /// @param location Source location information
        [[noreturn]] explicit VulkanError
        (
            const std::string_view format,
            Args&&... args,
            const std::source_location location = std::source_location::current()
        )
        {
            Detail::LogAndExit<-1>
            (
                fmt::color::orange_red,
                "VKERROR",
                location,
                format,
                std::forward<Args>(args)...
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
    template <typename... Args>
    VulkanError(std::string_view, Args&&...) -> VulkanError<Args...>;
}

#endif