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

#include "Util/Util.h"

namespace Engine::Files
{
    /// @brief Get asset file path
    /// @param prefix Prefix directory of file
    /// @param fileName Name of file
    /// @returns The file's directory as a std::string
    [[nodiscard]] std::string GetAssetPath(const std::string_view prefix, const std::string_view fileName);
    /// @brief Get file directory from path
    /// @param path Path of file
    /// @returns The file's directory as a std::string
    [[nodiscard]] std::string GetDirectory(const std::string_view path);
    /// @brief Get file size from path
    /// @param path Path of file
    /// @returns Size (in bytes)
    [[nodiscard]] usize GetSize(const std::string_view path);
    /// @brief Read file data as binary
    /// @param path Path of file
    /// @returns std::vector of binary data
    [[nodiscard]] std::vector<u8> ReadBytes(const std::string_view path);
    /// @brief Get file name without extension from file name
    /// @param fileName Name of file
    /// @returns std::string of file name without extension
    [[nodiscard]] std::string GetNameWithoutExtension(const std::string_view fileName);
    /// @brief Get extension from file name
    /// @param fileName Name of file
    /// @returns std::string of extension
    [[nodiscard]] std::string GetExtension(const std::string_view fileName);
    /// @brief Check whether if the file exists or not
    /// @param fileName Name of file
    /// @returns Does the file exist or not
    [[nodiscard]] bool Exists(const std::string_view fileName);
    /// @brief Get file name from path (constexpr version)
    /// @param path Path of file
    /// @returns std::string_view of file name from original path
    [[nodiscard]] constexpr std::string_view GetName(const std::string_view fileName)
    {
        usize lastSlash = fileName.find_last_of(std::filesystem::path::preferred_separator);

        if (lastSlash != std::string_view::npos)
        {
            return fileName.substr(lastSlash + 1);
        }

        return fileName.data();
    }
}

#endif