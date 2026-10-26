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

#include "FastGLTF.h"

#include "Util/Log.h"
#include "Util/Maths.h"
#include "Util/Visitor.h"

namespace fastgltf
{
    glm::mat4 GetTransformMatrix(const fastgltf::Node& node, const glm::mat4& base)
    {
        return std::visit(Util::Visitor{
            [&] (const fastgltf::math::fmat4x4& matrix)
            {
                return base * glm::fastgltf_cast(matrix);
            },
            [&] (const fastgltf::TRS& trs)
            {
                return base * Maths::TransformMatrix
                (
                    glm::fastgltf_cast(trs.translation),
                    glm::eulerAngles(glm::fastgltf_cast(trs.rotation)),
                    glm::fastgltf_cast(trs.scale)
                );
            }
        }, node.transform);
    }

    const fastgltf::Accessor& GetAccessor
    (
        const fastgltf::Asset& asset,
        const fastgltf::Primitive& primitive,
        const std::string_view attribute,
        fastgltf::AccessorType type
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const auto iter = primitive.findAttribute(attribute);

        if (iter == primitive.attributes.cend())
        {
            Logger::Error("Failed to find attribute! [Attribute={}]\n", attribute);
        }

        const auto& accessor = asset.accessors[iter->accessorIndex];

        if (accessor.type != type)
        {
            Logger::Error
            (
                "Invalid accessor type! [AccessorType={}] [Required={}]\n",
                static_cast<std::underlying_type_t<fastgltf::AccessorType>>(accessor.type),
                static_cast<std::underlying_type_t<fastgltf::AccessorType>>(type)
            );
        }

        return accessor;
    }

    std::optional<usize> GetUVAccessorIndex
    (
        const fastgltf::Asset& asset,
        const fastgltf::Primitive& primitive,
        const std::string_view attribute
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const auto iter = primitive.findAttribute(attribute);

        if (iter == primitive.attributes.cend())
        {
            return std::nullopt;
        }

        const auto& accessor = asset.accessors[iter->accessorIndex];

        if (accessor.type != fastgltf::AccessorType::Vec2)
        {
            Logger::Error
            (
                "Invalid UV accessor type! [AccessorType={}] [Required={}]\n",
                static_cast<std::underlying_type_t<fastgltf::AccessorType>>(accessor.type),
                static_cast<std::underlying_type_t<fastgltf::AccessorType>>(fastgltf::AccessorType::Vec2)
            );
        }

        return iter->accessorIndex;
    }
}
