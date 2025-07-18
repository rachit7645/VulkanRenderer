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

#include <SDL3/SDL_main.h>

#include "Util/Unused.h"
#include "Engine/AppInstance.h"
#include "Externals/Tracy.h"

int main(ENGINE_UNUSED int argc, ENGINE_UNUSED char** argv)
{
    #ifdef ENGINE_DEBUG
    // Set stderr to line buffering mode (does this even work on windows lol)
    setvbuf(stderr, nullptr, _IOLBF, 0);
    #endif

    #ifdef ENGINE_PROFILE
    while(!tracy::GetProfiler().IsConnected())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    #endif

    Engine::AppInstance().Run();

    return EXIT_SUCCESS;
}