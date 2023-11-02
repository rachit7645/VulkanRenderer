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

#ifndef FILES_H
#define FILES_H

#include <string_view>
#include <string>

#include "Util/Util.h"

namespace Engine
{
    class Files
    {
    private:
        // Default constructor
        Files() = default;
    public:
        // Get instance
        [[nodiscard]] static Files& GetInstance();
        // Set resource directory
        void SetResources(const std::string_view relPath);
        // Get resource directory
        [[nodiscard]] const std::string& GetResources();
        // Get file name from path
        [[nodiscard]] std::string GetName(const std::string_view path);
        // Get directory from path
        [[nodiscard]] std::string GetDirectory(const std::string_view path);
        // Get file size
        [[nodiscard]] usize GetFileSize(const std::string_view path);
        // Reads binary data into vector
        [[nodiscard]] std::vector<u8> ReadBytes(const std::string_view path);
    private:
        // Resource directory
        std::string m_resDir;
    };
}

#endif