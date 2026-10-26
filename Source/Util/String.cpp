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

#include "String.h"

#include "Util/Types.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Util
{
    std::string ToLower(const std::string_view string)
    {
        auto result = std::string(string);

        std::ranges::transform(result, result.begin(), [] (unsigned char character)
        {
            return std::tolower(character);
        });

        return result;
    }

    std::wstring MultiByteToWideChar(const std::string_view string)
    {
        #ifdef _WIN32
        if (string.empty())
        {
            return {};
        }

        const s32 required = ::MultiByteToWideChar
        (
            CP_UTF8,
            0,
            string.data(),
            static_cast<s32>(string.size()),
            nullptr,
            0
        );

        if (required == 0)
        {
            return {};
        }

        std::wstring result(required, L'\0');

        const s32 converted = ::MultiByteToWideChar
        (
            CP_UTF8,
            0,
            string.data(),
            static_cast<s32>(string.size()),
            result.data(),
            required
        );

        if (converted == 0)
        {
            return {};
        }

        return result;
        #else
        ENGINE_TODO();

        return L"";
        #endif
    }
}