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

#ifndef PIPELINE_ID_H
#define PIPELINE_ID_H

namespace Vk
{
    enum class PipelineID : u8
    {
        Tonemap,
        BloomDownSample,
        BloomDownSampleFirstSample,
        BloomUpSample,
        BloomCombine,
        DepthOpaque,
        DepthAlphaMasked,
        DearImGui,
        Lighting,
        Skybox,
        PointShadowOpaque,
        PointShadowAlphaMasked,
        GBufferSingleSided,
        GBufferDoubleSided,
        FrustumCulling,
        SpotShadowOpaque,
        SpotShadowAlphaMasked,
        AutoExposureHistogram,
        AutoExposureAverage,
        AutoExposureCombine,
        TAA,
        VBAODepthPreFilter,
        VBAO,
        VBAOSpatialDenoise,
        TiledLightCullingBounds,
        TiledLightCulling,
        DebugAABBGenerateDrawCalls,
        DebugAABB,
        DebugLightSphere,
        DebugFrustumCullingStatistics,
        DebugTiledLightCullingStatistics,
        ShadowRayTraced,
        IBLEquirectangularToCubemap,
        IBLIrradiance,
        IBLPreFilter,
        IBLBRDF
    };
}

#endif
