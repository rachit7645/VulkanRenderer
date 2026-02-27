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

#include "SIMD.h"

#include <immintrin.h>

#include "Externals/GLM.h"

namespace Util
{
    constexpr f32 F16_MIN = -65504.0f;
    constexpr f32 F16_MAX =  65504.0f;
    
    f16 ConvertF32ToF16(f32 value)
    {
        return _cvtss_sh(glm::clamp(value, F16_MIN, F16_MAX), _MM_FROUND_TO_NEAREST_INT);
    }

    void ConvertF32ToF16(const f32* __restrict__ source, f16* __restrict__ destination, usize count)
    {
        usize i = 0;

        const __m256 F16_MIN_256_BIT = _mm256_set1_ps(F16_MIN);
        const __m256 F16_MAX_256_BIT = _mm256_set1_ps(F16_MAX);

        for (; (i + 8) <= count; i += 8)
        {
            const __m256  src     = _mm256_loadu_ps(source + i);
            const __m256  clamped = _mm256_min_ps(_mm256_max_ps(src, F16_MIN_256_BIT), F16_MAX_256_BIT);
            const __m128i dst     = _mm256_cvtps_ph(clamped, _MM_FROUND_TO_NEAREST_INT);

            _mm_storeu_si128(reinterpret_cast<__m128i*>(destination + i), dst);
        }

        const __m128 F16_MIN_128_BIT = _mm_set1_ps(F16_MIN);
        const __m128 F16_MAX_128_BIT = _mm_set1_ps(F16_MAX);

        for (; (i + 4) <= count; i += 4)
        {
            const __m128  src     = _mm_loadu_ps(source + i);
            const __m128  clamped = _mm_min_ps(_mm_max_ps(src, F16_MIN_128_BIT), F16_MAX_128_BIT);
            const __m128i dst     = _mm_cvtps_ph(clamped, _MM_FROUND_TO_NEAREST_INT);

            _mm_storeu_si64(destination + i, dst);
        }

        for (; i < count; ++i)
        {
            destination[i] = _cvtss_sh(glm::clamp(source[i], F16_MIN, F16_MAX), _MM_FROUND_TO_NEAREST_INT);
        }
    }

    void ConvertF16ToF32(const f16* __restrict__ source, f32* __restrict__ destination, usize count)
    {
        usize i = 0;

        for (; (i + 8) <= count; i += 8)
        {
            const __m128i src = _mm_loadu_si128(reinterpret_cast<__m128i const*>(source + i));
            const __m256  dst = _mm256_cvtph_ps(src);

            _mm256_storeu_ps(destination + i, dst);
        }

        for (; (i + 4) <= count; i += 4)
        {
            const __m128i src = _mm_loadu_si64(source + i);
            const __m128  dst = _mm_cvtph_ps(src);

            _mm_storeu_ps(destination + i, dst);
        }

        for (; i < count; ++i)
        {
            destination[i] = _cvtsh_ss(source[i]);
        }
    }
}
