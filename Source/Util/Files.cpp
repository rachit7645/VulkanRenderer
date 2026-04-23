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
#include "Files.h"
#include "Log.h"

#include <filesystem>
#include <fstream>

namespace Util::Files
{
    constexpr auto ASSETS_DIRECTORY = "Assets/";

    std::string GetAssetPath(const std::string_view prefix, const std::string_view fileName)
    {
        return fmt::format("{}{}{}", ASSETS_DIRECTORY, prefix, fileName);
    }

    std::string GetDirectory(const std::string_view path)
    {
        return std::filesystem::path(path).parent_path().string();
    }

    usize GetSize(const std::string_view path)
    {
        static_assert(sizeof(usize) >= sizeof(std::uintmax_t), "How???");

        return static_cast<usize>(std::filesystem::file_size(path));
    }

    std::string GetNameWithoutExtension(const std::string_view fileName)
    {
        return std::filesystem::path(fileName).stem().string();
    }

    std::string GetExtension(const std::string_view fileName)
    {
        return std::filesystem::path(fileName).extension().string();
    }
    
    std::time_t GetLastWriteTime(const std::string_view fileName)
    {
        const auto path                     = std::filesystem::path(fileName);
        const auto lastWriteTime            = std::filesystem::last_write_time(path);
        const auto lastWriteTimeSystemClock = std::chrono::clock_cast<std::chrono::system_clock>(lastWriteTime);

        return std::chrono::system_clock::to_time_t(lastWriteTimeSystemClock);
    }

    bool Exists(const std::string_view fileName)
    {
        return std::filesystem::exists(std::filesystem::path(fileName));
    }

    std::vector<u8> ReadBytes(const std::string_view path)
    {
        auto bin = std::ifstream(path.data(), std::ios::binary | std::ios::in);

        if (!bin.is_open())
        {
            Logger::Error("Failed to load binary {}!\n", path);
        }

        std::vector<u8> binary(Files::GetSize(path));

        bin.read(reinterpret_cast<char*>(binary.data()), static_cast<std::streamsize>(binary.size()));

        return binary;
    }

    void WriteBytes(const std::string_view path, const std::span<const u8> binary)
    {
        auto bin = std::ofstream(path.data(), std::ios::binary | std::ios::out);

        if (!bin.is_open())
        {
            Logger::Error("Failed to load binary {}!\n", path);
        }

        bin.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
    }

    std::vector<std::string> GetFilesInDirectory(const std::string_view path)
    {
        std::vector<std::string> files = {};

        for (const auto& file : std::filesystem::recursive_directory_iterator(path))
        {
            if (std::filesystem::is_regular_file(file))
            {
                files.emplace_back(file.path().string());
            }
        }

        return files;
    }
}
