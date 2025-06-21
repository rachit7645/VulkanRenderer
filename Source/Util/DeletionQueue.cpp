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

#include "DeletionQueue.h"

namespace Util
{
    void DeletionQueue::PushDeletor(Deletor&& deletor)
    {
        std::lock_guard lock(m_mutex);

        m_deletors.push(std::move(deletor));
    }

    void DeletionQueue::FlushQueue()
    {
        if (m_deletors.empty())
        {
            return;
        }

        std::stack<Deletor> temp = {};
        {
            std::lock_guard lock(m_mutex);
            std::swap(m_deletors, temp);
        }

        while (!temp.empty())
        {
            temp.top()();
            temp.pop();
        }
    }
}