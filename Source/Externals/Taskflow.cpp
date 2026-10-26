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

#include "Taskflow.h"

#include "Util/Threads.h"
#include "Externals/FMT.h"
#include "Util/Unused.h"

namespace TaskFlow
{
    void WorkerInterface::scheduler_prologue(tf::Worker& worker)
    {
        Util::SetThreadName(worker.thread(), fmt::format("Worker #{}", worker.id()));
    }

    void WorkerInterface::scheduler_epilogue(ENGINE_UNUSED tf::Worker& worker, ENGINE_UNUSED std::exception_ptr ptr)
    {
    }
}
