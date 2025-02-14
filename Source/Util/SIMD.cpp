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

#include "SIMD.h"

#include <immintrin.h>

namespace Util
{
    void ConvertF32ToF16(const f32* __restrict__ source, f16* __restrict__ destination, usize count)
    {
        usize i = 0;

        for (; (i + 8) < count; i += 8)
        {
            const __m256  src = _mm256_loadu_ps(source + i);
            const __m128i dst = _mm256_cvtps_ph(src, _MM_FROUND_TO_NEAREST_INT);

            _mm_storeu_si128(std::bit_cast<__m128i*>(destination + i), dst);
        }

        for (; (i + 4) < count; i += 4)
        {
            const __m128  src = _mm_loadu_ps(source + i);
            const __m128i dst = _mm_cvtps_ph(src, _MM_FROUND_TO_NEAREST_INT);

            _mm_storeu_si64(destination + i, dst);
        }
    }
}
