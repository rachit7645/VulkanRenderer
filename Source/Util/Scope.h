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

#ifndef SCOPE_H
#define SCOPE_H

#include <utility>

namespace Util
{
    template<typename Function>
    class ScopeGuard
    {
    public:
        explicit ScopeGuard(Function&& function)
            : m_function(std::forward<Function>(std::move(function)))
        {
        }
    
        ~ScopeGuard()
        {
            if (m_active)
            {
                m_function();
            }
        }

        ScopeGuard(ScopeGuard&& other) noexcept
            : m_function(std::move(other.m_function)),
              m_active(other.m_active)
        {
            other.Release();
        }

        ScopeGuard(const ScopeGuard&)            = delete;
        ScopeGuard& operator=(const ScopeGuard&) = delete;
        ScopeGuard& operator=(ScopeGuard&&)      = delete;
    
        void Release()
        {
            m_active = false;
        }
    private:
        Function m_function;

        bool m_active = true;
    };
    
    template<typename Function>
    auto MakeScopeGuard(Function&& function)
    {
        return ScopeGuard<std::decay_t<Function>>(std::forward<Function>(function));
    }
}

#endif
