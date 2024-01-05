/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
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

#include "Files.h"
#include "Log.h"

#include <filesystem>
#include <fstream>

// Namespace aliases
namespace filesystem = std::filesystem;

namespace Engine::Files
{
    // Assets directory
    constexpr auto ASSETS_DIRECTORY = "Assets/";

    std::string GetAssetPath(const std::string_view prefix, const std::string_view fileName)
    {
        return fmt::format("{}{}{}", ASSETS_DIRECTORY, prefix, fileName);
    }

    std::string GetDirectory(const std::string_view path)
    {
        auto directory = filesystem::path(path).parent_path().string();
        auto separator = static_cast<char>(filesystem::path::preferred_separator);
        return directory + separator;
    }

    usize GetFileSize(const std::string_view path)
    {
        // This should always work
        static_assert(sizeof(usize) >= sizeof(std::uintmax_t), "How???");
        return filesystem::file_size(path);
    }

    std::vector<u8> ReadBytes(const std::string_view path)
    {
        // Open in binary mode
        auto bin = std::ifstream(path.data(), std::ios::binary | std::ios::in);

        if (!bin.is_open())
        {
            Logger::Error("Failed to load shader binary {}!\n", path);
        }

        // Pre-allocate memory for SPEED
        std::vector<u8> binary = {};
        binary.resize(GetFileSize(path));

        bin.read(reinterpret_cast<char*>(binary.data()), static_cast<std::streamsize>(binary.size()));

        return binary;
    }
}