#include "Time.h"

#include <chrono>
#include <fmt/format.h>
#include <fmt/chrono.h>

// Aliases
using TimeClock = std::chrono::system_clock;

namespace Util
{
    std::string GetTime()
    {
        // Format time
        return fmt::format("{:%H:%M:%S}", TimeClock::now());
    }
}