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

#ifndef APP_INSTANCE_H
#define APP_INSTANCE_H

#include <memory>

#include "Window.h"
#include "Renderer/RenderManager.h"

namespace Engine
{
    class AppInstance
    {
    public:
         AppInstance();
        ~AppInstance();

        // No copying
        AppInstance(const AppInstance&)            = delete;
        AppInstance& operator=(const AppInstance&) = delete;

        // Only moving
        AppInstance(AppInstance&& other)            = default;
        AppInstance& operator=(AppInstance&& other) = default;

        void Run();
    private:
        // Shared window (FIXME: why??)
        std::shared_ptr<Engine::Window> m_window = nullptr;
        // Renderer
        Renderer::RenderManager m_renderer;
    };
}

#endif
