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

#ifndef SIMD_H
#define SIMD_H

#include "Types.h"

namespace Util
{
    // `source` and `destination` must not be the same
    // Requires AVX2
    void ConvertF32ToF16(const f32* __restrict__ source, f16* __restrict__ destination, usize count);

    // `source` and `destination` must not be the same
    // Requires AVX2
    void ConvertF16ToF32(const f16* __restrict__ source, f32* __restrict__ destination, usize count);
}

#endif
