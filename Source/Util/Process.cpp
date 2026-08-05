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

#include "Process.h"

#include "Engine/Containers.h"
#include "Util/Log.h"

namespace Util
{
    Process::Process(SDL_Process* process)
        : m_handle(process)
    {
    }

    Process::~Process()
    {
        SDL_DestroyProcess(m_handle);
    }

    Process::Process(Process&& other) noexcept
        : m_handle{std::exchange(other.m_handle, nullptr)}
    {
    }

    Process& Process::operator=(Process&& other) noexcept
    {
        std::swap(m_handle, other.m_handle);

        return *this;
    }

    std::expected<s32, std::string> Process::WaitForProcess()
    {
        if (m_handle == nullptr)
        {
            return std::unexpected("Invalid process handle!");
        }

        s32 errorCode = 0;

        if (!SDL_WaitProcess(m_handle, true, &errorCode))
        {
            return std::unexpected(fmt::format("Failed to wait for process! [Error={}]", SDL_GetError()));
        }

        return errorCode;
    }

    std::expected<Util::Process, std::string> ProcessBuilder::Create()
    {
        if (m_arguments.empty())
        {
            return std::unexpected("No arguments!");
        }

        std::vector<const char*> arguments = {};

        arguments.reserve(m_arguments.size() + 1);

        for (const auto& argument : m_arguments)
        {
            arguments.emplace_back(argument.c_str());
        }

        arguments.emplace_back(nullptr);

        SDL_Process* handle = SDL_CreateProcess(arguments.data(), false);

        if (handle == nullptr)
        {
            return std::unexpected(fmt::format("Failed to create process! [Error={}]", SDL_GetError()));
        }

        return Util::Process(handle);
    }

    ProcessBuilder& ProcessBuilder::AddArgument(const std::string_view argument)
    {
        m_arguments.emplace_back(argument);

        return *this;
    }
}
