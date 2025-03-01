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

#include "Files.h"
#include "Log.h"

#include <filesystem>
#include <fstream>

namespace filesystem = std::filesystem;

namespace Engine::Files
{
    constexpr auto ASSETS_DIRECTORY = "Assets/";

    std::string GetAssetPath(const std::string_view prefix, const std::string_view fileName)
    {
        return fmt::format("{}{}{}", ASSETS_DIRECTORY, prefix, fileName);
    }

    std::string GetDirectory(const std::string_view path)
    {
        return filesystem::path(path).parent_path().string();
    }

    usize GetFileSize(const std::string_view path)
    {
        // This should always work
        static_assert(sizeof(usize) >= sizeof(std::uintmax_t), "How???");
        return static_cast<usize>(filesystem::file_size(path));
    }

    std::vector<u8> ReadBytes(const std::string_view path)
    {
        // Open in binary mode
        auto bin = std::ifstream(path.data(), std::ios::binary | std::ios::in);

        if (!bin.is_open())
        {
            Logger::Error("Failed to load shader binary {}!\n", path);
        }

        std::vector<u8> binary = {};
        binary.resize(GetFileSize(path));

        bin.read(reinterpret_cast<char*>(binary.data()), static_cast<std::streamsize>(binary.size()));

        return binary;
    }

    std::string GetNameWithoutExtension(const std::string_view fileName)
    {
        return filesystem::path(fileName).stem().string();
    }

    std::string GetExtension(const std::string_view fileName)
    {
        return filesystem::path(fileName).extension().string();
    }
}
