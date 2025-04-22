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
#include <thread>

namespace Util
{
    void ConvertF32ToF16Range(const f32* __restrict__ source, f16* __restrict__ destination, usize start, usize end)
    {
        usize i = start;

        for (; (i + 8) < end; i += 8)
        {
            const __m256  src = _mm256_loadu_ps(source + i);
            const __m128i dst = _mm256_cvtps_ph(src, _MM_FROUND_TO_NEAREST_INT);

            _mm_storeu_si128(reinterpret_cast<__m128i*>(destination + i), dst);
        }

        for (; (i + 4) < end; i += 4)
        {
            const __m128  src = _mm_loadu_ps(source + i);
            const __m128i dst = _mm_cvtps_ph(src, _MM_FROUND_TO_NEAREST_INT);

            _mm_storeu_si64(reinterpret_cast<void*>(destination + i), dst);
        }

        for (; i < end; ++i)
        {
            destination[i] = _cvtss_sh(source[i], _MM_FROUND_TO_NEAREST_INT);
        }
    }

    void ConvertF32ToF16(const f32* __restrict__ source, f16* __restrict__ destination, usize count)
    {
        const usize threadCount = std::max(std::thread::hardware_concurrency(), 1u);

        const usize chunkSize = count / threadCount;
        const usize countLeft = count % threadCount;

        std::vector<std::thread> threads = {};
        threads.reserve(threadCount);

        for (usize i = 0; i < threadCount; ++i)
        {
            const usize start = i * chunkSize;
            const usize end   = (i == threadCount - 1) ? (start + chunkSize + countLeft) : (start + chunkSize);

            threads.emplace_back(ConvertF32ToF16Range, source, destination, start, end);
        }

        for (auto& thread : threads)
        {
            thread.join();
        }
    }
}
