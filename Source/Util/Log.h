#ifndef LOG_H
#define LOG_H

#include <string>
#include <string_view>
#include <cstdlib>

#include <fmt/format.h>
#include <fmt/color.h>

#include "Files.h"
#include "Time.h"

// TODO: Convert all loggers to templates

namespace Logger
{
    template<typename... Args>
    void Log
    (
        const fmt::color& fgColor,
        const fmt::color& bgColor,
        const std::string_view type,
        const std::string_view time,
        const std::string_view file,
        size_t line,
        fmt::string_view format,
        Args&&... args
    )
    {
        // Format & print additional data
        fmt::print
        (
            stderr,
            fmt::fg(fgColor) | fmt::bg(bgColor),
            "[{}] [{}] [{}:{}] ",
            type, time, file, line
        );

        // Format & print
        fmt::print
        (
            stderr,
            fmt::fg(fgColor) | fmt::bg(bgColor),
            format,
            args...
        );
    }
}

// Implementation

#define IMPL_LOG(fgColor, bgColor, type, format, ...) \
    do \
    { \
        Logger::Log \
        ( \
            fgColor, \
            bgColor, \
            type, \
            Util::GetTime(), \
            Engine::Files::GetInstance().GetName(__FILE__), \
            __LINE__, \
            FMT_STRING(format), \
            __VA_ARGS__ \
        ); \
    } \
    while (0)

#define IMPL_LOG_EXIT(fgColor, bgColor, type, format, ...) \
    do \
    { \
        IMPL_LOG(fgColor, bgColor, type, format, __VA_ARGS__); \
        std::exit(-1); \
    } \
    while (0)

// Regular loggers
#define LOG_INFO(format, ...)    IMPL_LOG(fmt::color::green,  fmt::color::black, "INFO",    format, __VA_ARGS__)
#define LOG_WARNING(format, ...) IMPL_LOG(fmt::color::yellow, fmt::color::black, "WARNING", format, __VA_ARGS__)

// Debug loggers
#ifdef ENGINE_DEBUG
    #define LOG_DEBUG(format, ...)   IMPL_LOG(fmt::color::cyan,   fmt::color::black, "DEBUG",   format, __VA_ARGS__)
    #define LOG_VK(format, ...)      IMPL_LOG(fmt::color::orange, fmt::color::black, "VULKAN",  format, __VA_ARGS__)
#else
    #define LOG_DEBUG(format, ...)
    #define LOG_VK(format, ...)
#endif

// Error loggers
#define LOG_ERROR(format, ...) IMPL_LOG_EXIT(fmt::color::red, fmt::color::black, "ERROR", format, __VA_ARGS__)

#endif