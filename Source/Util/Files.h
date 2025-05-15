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

#ifndef FILES_H
#define FILES_H

#include <string_view>
#include <string>

#include "Util/Types.h"

namespace Util::Files
{
    [[nodiscard]] std::string GetAssetPath(const std::string_view prefix, const std::string_view fileName);

    [[nodiscard]] std::string GetDirectory(const std::string_view path);
    [[nodiscard]] usize GetSize(const std::string_view path);
    [[nodiscard]] std::string GetNameWithoutExtension(const std::string_view fileName);
    [[nodiscard]] std::string GetExtension(const std::string_view fileName);

    [[nodiscard]] bool Exists(const std::string_view fileName);

    [[nodiscard]] std::vector<u8> ReadBytes(const std::string_view path);

    [[nodiscard]] constexpr std::string_view GetName(const std::string_view fileName)
    {
        const auto lastSlashPosition = fileName.find_last_of("/\\");

        return (lastSlashPosition == std::string_view::npos)
            ? fileName.data()
            : fileName.substr(lastSlashPosition + 1);
    }
}

#endif