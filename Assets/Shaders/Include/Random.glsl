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

#ifndef RANDOM_GLSL
#define RANDOM_GLSL

// https://www.pcg-random.org/
// https://github.com/riccardoscalco/glsl-pcg-prng/blob/main/index.glsl
uint PCG(inout uint state)
{
    state = state * 747796405u + 2891336453u;

    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;

    return (word >> 22u) ^ word;
}

float Random(inout uint state)
{
    return float(PCG(state)) / float(0xFFFFFFFFu);
}

#endif