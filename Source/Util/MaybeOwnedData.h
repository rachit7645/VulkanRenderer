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

#ifndef MAYBE_OWNED_VECTOR_H
#define MAYBE_OWNED_VECTOR_H

#include <span>
#include <variant>
#include <vector>

#include "Util/Types.h"
#include "Util/Visitor.h"

namespace Util
{
    class MaybeOwnedData
    {
    public:
        constexpr MaybeOwnedData() = default;

        constexpr MaybeOwnedData(std::vector<u8>&& vector)
            : m_data{std::move(vector)}
        {
        }

        constexpr MaybeOwnedData(const std::span<const u8> span)
            : m_data{span}
        {
        }

        constexpr const u8* data() const
        {
            return std::visit(Util::Visitor{
                [] (const std::vector<u8>& vector) -> const u8*
                {
                    return vector.data();
                },
                [] (const std::span<const u8> span) -> const u8*
                {
                    return span.data();
                }
            }, m_data);
        }

        constexpr usize size() const
        {
            return std::visit(Util::Visitor{
                [] (const std::vector<u8>& vector)
                {
                    return vector.size();
                },
                [] (const std::span<const u8> span)
                {
                    return span.size();
                }
            }, m_data);
        }

        constexpr bool IsOwned() const
        {
            return std::holds_alternative<std::vector<u8>>(m_data);
        }
    private:
        std::variant<std::vector<u8>, std::span<const u8>> m_data = {};
    };
}

#endif
