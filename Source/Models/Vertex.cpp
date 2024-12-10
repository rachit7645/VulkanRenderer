/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
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

#include "Vertex.h"

namespace Models
{
    Vertex::Vertex
    (
        const glm::vec3& position,
        const glm::vec2& texCoords,
        const glm::vec3& normal,
        const glm::vec3& tangent
    )
    {
        attrib0 = {position.x,  position.y, position.z, texCoords.x};
        attrib1 = {texCoords.y, normal.x,   normal.y,   normal.z};
        attrib2 = {tangent.x,   tangent.y,  tangent.z,  0.5f};
    }
}