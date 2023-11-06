/*
 *    Copyright 2023 Rachit Khandelwal
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
    std::string GetDirectory(const std::string_view path)
    {
        // Get directory
        auto directory = filesystem::path(path).parent_path().string();
        // Get separator
        auto separator = static_cast<char>(filesystem::path::preferred_separator);
        // Add and return
        return directory + separator;
    }

    usize GetFileSize(const std::string_view path)
    {
        // Assert
        static_assert(sizeof(usize) >= sizeof(std::uintmax_t), "How???");
        // Get file size
        return filesystem::file_size(path);
    }

    std::vector<u8> ReadBytes(const std::string_view path)
    {
        // Create file
        auto bin = std::ifstream(path.data(), std::ios::binary);

        // Make sure files was opened
        if (!bin.is_open())
        {
            // Log
            Logger::Error("Failed to load shader binary {}!\n", path);
        }

        // Get file size
        auto fileSize = GetFileSize(path);
        // Binary data
        auto binary = std::vector<u8>();
        binary.resize(fileSize);

        // Read data into buffer
        bin.read(reinterpret_cast<char*>(binary.data()), static_cast<std::streamsize>(binary.size()));

        // Return
        return binary;
    }
}