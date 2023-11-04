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

#include "Inputs.h"

namespace Engine
{
    Inputs::Inputs()
        : m_keys(SDL_GetKeyboardState(nullptr))
    {
    }

    glm::ivec2& Inputs::GetMousePos()
    {
        // Return mouse position
        return m_mousePos;
    }

    glm::ivec2& Inputs::GetMouseScroll()
    {
        // Return mouse scroll
        return m_mouseScroll;
    }

    bool Inputs::IsKeyPressed(SDL_Scancode key)
    {
        // Return key state
        return static_cast<bool>(m_keys[key]);
    }

    Inputs& Inputs::GetInstance()
    {
        // Static storage
        static Inputs inputs;
        // Return
        return inputs;
    }
}