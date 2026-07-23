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

#ifndef LOG_H
#define LOG_H

#include <string_view>
#include <source_location>
#include <stacktrace>

#include "Types.h"
#include "Unused.h"
#include "Externals/FMT.h"

namespace Logger
{
    namespace Detail
    {
        void Log
        (
            const fmt::color& fgColor,
            const std::string_view type,
            const std::source_location& location,
            const std::string_view format,
            const fmt::format_args args
        );

        [[noreturn]] void LogAndExit
        (
            const s32 errorCode,
            const fmt::color& fgColor,
            const std::string_view type,
            const std::source_location& location,
            const std::stacktrace& stacktrace,
            const std::string_view format,
            const fmt::format_args args
        );
    }

    template <typename... Args>
    struct Info
    {
        explicit Info
        (
            const std::string_view format,
            Args&&... args,
			const std::source_location& location = std::source_location::current()
        )
        {
            Detail::Log
            (
                fmt::color::forest_green,
                "INFO",
                location,
                format,
                fmt::make_format_args(args...)
            );
        }
    };

    template <typename... Args>
    struct Warning
    {
        explicit Warning
        (
            const std::string_view format,
            Args&&... args,
			const std::source_location& location = std::source_location::current()
        )
        {
            Detail::Log
            (
                fmt::color::yellow,
                "WARNING",
                location,
                format,
                fmt::make_format_args(args...)
            );
        }
    };

    template <typename... Args>
    struct Debug
    {
        explicit Debug
        (
            ENGINE_UNUSED const std::string_view format,
            ENGINE_UNUSED Args&&... args,
            ENGINE_UNUSED const std::source_location& location = std::source_location::current()
        )
        {
            #ifdef ENGINE_DEBUG
            Detail::Log
            (
                fmt::color::cyan,
                "DEBUG",
                location,
                format,
                fmt::make_format_args(args...)
            );
            #endif
        }
    };

    template <typename... Args>
    struct Vulkan
    {
        explicit Vulkan
        (
            ENGINE_UNUSED const std::string_view format,
            ENGINE_UNUSED Args&&... args,
            ENGINE_UNUSED const std::source_location& location = std::source_location::current()
        )
        {
            #ifdef ENGINE_DEBUG
            Detail::Log
            (
                fmt::color::orange,
                "VULKAN",
                location,
                format,
                fmt::make_format_args(args...)
            );
            #endif
        }
    };

    template <typename... Args>
    struct Error
    {
        [[noreturn]] explicit Error
        (
            const std::string_view format,
            Args&&... args,
			const std::source_location& location = std::source_location::current(),
			const std::stacktrace& stacktrace = std::stacktrace::current()
        )
        {
            constexpr s32 ERROR_CODE = -1;

            Detail::LogAndExit
            (
                ERROR_CODE,
                fmt::color::red,
                "ERROR",
                location,
                stacktrace,
                format,
                fmt::make_format_args(args...)
            );
        }
    };

    template <typename... Args>
    struct VulkanError
    {
        [[noreturn]] explicit VulkanError
        (
            const std::string_view format,
            Args&&... args,
            const std::source_location& location = std::source_location::current(),
            const std::stacktrace& stacktrace = std::stacktrace::current()
        )
        {
            constexpr s32 ERROR_CODE = -2;

            Detail::LogAndExit
            (
                ERROR_CODE,
                fmt::color::orange_red,
                "VKERROR",
                location,
                stacktrace,
                format,
                fmt::make_format_args(args...)
            );
        }
    };

    template <typename... Args>
    struct Assert
    {
        [[noreturn]] explicit Assert
        (
            const std::string_view format,
            Args&&... args,
            const std::source_location& location = std::source_location::current(),
            const std::stacktrace& stacktrace = std::stacktrace::current()
        )
        {
            constexpr s32 ERROR_CODE = -3;

            Detail::LogAndExit
            (
                ERROR_CODE,
                fmt::color::dark_red,
                "ASSERT",
                location,
                stacktrace,
                format,
                fmt::make_format_args(args...)
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
    template <typename... Args>
    Assert(std::string_view, Args&&...) -> Assert<Args...>;
}

#endif