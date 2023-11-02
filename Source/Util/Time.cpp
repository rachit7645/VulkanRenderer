/*
 * Copyright 2023 Rachit Khandelwal
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