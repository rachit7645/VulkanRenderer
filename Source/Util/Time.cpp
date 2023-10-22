#include "Time.h"

#include <chrono>
#include <fmt/format.h>
#include <fmt/chrono.h>

// Shorten chrono
namespace Chrono = std::chrono;

// Aliases
using TimeClock = Chrono::system_clock;

namespace Util
{
    std::string GetTime()
    {
        // Format time
        return fmt::format("{:%T}", TimeClock::now());
    }
}