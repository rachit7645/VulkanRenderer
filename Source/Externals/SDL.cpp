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

#include "Externals/SDL.h"

#include "Util/Log.h"

namespace SDL
{
    void SetHint(const char* name, const char* value)
    {
        if (SDL_SetHint(name, value))
        {
            return;
        }

        Logger::Warning
        (
            "SDL_SetHint failed! [Hint={}] [Value={}] [Error={}]\n",
            name != nullptr  ? name  : "nullptr",
            value != nullptr ? value : "nullptr",
            SDL_GetError()
        );
    }

    std::span<const char* const> GetInstanceExtensions()
    {
        u32 SDLExtensionCount = 0;

        const auto* SDLInstanceExtensions = SDL_Vulkan_GetInstanceExtensions(&SDLExtensionCount);

        if (SDLInstanceExtensions == nullptr)
        {
            Logger::Error("Failed to load SDL3 extensions!: {}\n", SDL_GetError());
        }

        return std::span(SDLInstanceExtensions, SDLInstanceExtensions + SDLExtensionCount);
    }
}
