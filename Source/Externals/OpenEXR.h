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

#ifndef EXTERNALS_OPENEXR_H
#define EXTERNALS_OPENEXR_H

#if defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic push
     #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "openexr/src/lib/OpenEXR/ImfRgbaFile.h"
#include "openexr/src/lib/OpenEXR/ImfArray.h"

#if defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic pop
#elifdef __clang__
    #pragma clang diagnostic pop
#endif

#endif
