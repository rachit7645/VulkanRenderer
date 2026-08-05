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

#include "Threads.h"

#include "Util/String.h"
#include "Util/Unused.h"

#ifdef _WIN32
#include <Windows.h>
#elif __linux__
#include <pthread.h>
#endif

namespace Util
{
    u64 GetWorkerThreadCount()
    {
        const u64 concurrentThreadCount = std::thread::hardware_concurrency();

        if (concurrentThreadCount <= 1)
        {
            return 1;
        }

        // Reserve the Main thread
        return concurrentThreadCount - 1;
    }

    void SetThreadName(std::thread& thread, const std::string_view name)
    {
        #ifdef _WIN32
        const auto wideName = Util::MultiByteToWideChar(name);

        ENGINE_UNUSED const HRESULT result = SetThreadDescription(thread.native_handle(), wideName.c_str());
        #elif __linux__
        constexpr usize PTHREAD_MAX_THREAD_NAME_LENGTH = 15;

        const auto convertedName = std::string(name.substr(0, PTHREAD_MAX_THREAD_NAME_LENGTH));

        ENGINE_UNUSED s32 result = pthread_setname_np(thread.native_handle(), convertedName.c_str());
        #endif
    }
}
