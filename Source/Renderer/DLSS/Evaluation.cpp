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

#include "Evaluation.h"

#include "Renderer/Jitter.h"
#include "Renderer/RenderConstants.h"
#include "Util/Log.h"

namespace Renderer::DLSS
{
    void Evaluation::Evaluate
    (
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Util::FrameCounter& frameCounter,
        DLSS::DLSSConfig& config
    )
    {
        const auto& sceneColorView         = framebufferManager.GetFramebufferView("SceneColorView");
        const auto& resolvedSceneColorView = framebufferManager.GetFramebufferView("ResolvedSceneColorView");
        const auto& sceneDepthView         = framebufferManager.GetFramebufferView("SceneDepthView");
        const auto& gMotionVectorsView     = framebufferManager.GetFramebufferView("GMotionVectorsView");
        const auto& exposureValueView      = framebufferManager.GetFramebufferView("Exposure/ValueView");

        const auto& sceneColor         = framebufferManager.GetFramebuffer(sceneColorView.framebuffer);
        const auto& resolvedSceneColor = framebufferManager.GetFramebuffer(resolvedSceneColorView.framebuffer);
        const auto& sceneDepth         = framebufferManager.GetFramebuffer(sceneDepthView.framebuffer);
        const auto& gMotionVectors     = framebufferManager.GetFramebuffer(gMotionVectorsView.framebuffer);
        const auto& exposureValue      = framebufferManager.GetFramebuffer(exposureValueView.framebuffer);

        NVSDK_NGX_Resource_VK sceneColorNV =
        {
            .Resource = {
                .ImageViewInfo = {
                    .ImageView        = sceneColorView.view.handle,
                    .Image            = sceneColor.image.handle,
                    .SubresourceRange = {
                        .aspectMask     = sceneColor.image.aspect,
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1
                    },
                    .Format           = sceneColor.image.format,
                    .Width            = sceneColor.image.width,
                    .Height           = sceneColor.image.height
                }
            },
            .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
            .ReadWrite = false
        };

        NVSDK_NGX_Resource_VK resolvedSceneColorNV =
        {
            .Resource = {
                .ImageViewInfo = {
                    .ImageView        = resolvedSceneColorView.view.handle,
                    .Image            = resolvedSceneColor.image.handle,
                    .SubresourceRange = {
                        .aspectMask     = resolvedSceneColor.image.aspect,
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1
                    },
                    .Format           = resolvedSceneColor.image.format,
                    .Width            = resolvedSceneColor.image.width,
                    .Height           = resolvedSceneColor.image.height
                }
            },
            .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
            .ReadWrite = true
        };

        NVSDK_NGX_Resource_VK sceneDepthNV =
        {
            .Resource = {
                .ImageViewInfo = {
                    .ImageView        = sceneDepthView.view.handle,
                    .Image            = sceneDepth.image.handle,
                    .SubresourceRange = {
                        .aspectMask     = sceneDepth.image.aspect,
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1
                    },
                    .Format           = sceneDepth.image.format,
                    .Width            = sceneDepth.image.width,
                    .Height           = sceneDepth.image.height
                }
            },
            .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
            .ReadWrite = false
        };

        NVSDK_NGX_Resource_VK gMotionVectorsNV =
        {
            .Resource = {
                .ImageViewInfo = {
                    .ImageView        = gMotionVectorsView.view.handle,
                    .Image            = gMotionVectors.image.handle,
                    .SubresourceRange = {
                        .aspectMask     = gMotionVectors.image.aspect,
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1
                    },
                    .Format           = gMotionVectors.image.format,
                    .Width            = gMotionVectors.image.width,
                    .Height           = gMotionVectors.image.height
                }
            },
            .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
            .ReadWrite = false
        };

        NVSDK_NGX_Resource_VK exposureValueNV =
        {
            .Resource = {
                .ImageViewInfo = {
                    .ImageView        = exposureValueView.view.handle,
                    .Image            = exposureValue.image.handle,
                    .SubresourceRange = {
                        .aspectMask     = exposureValue.image.aspect,
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1
                    },
                    .Format           = exposureValue.image.format,
                    .Width            = exposureValue.image.width,
                    .Height           = exposureValue.image.height
                }
            },
            .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
            .ReadWrite = false
        };

        const auto jitter = Renderer::GetJitterInPixels(frameIndex, framebufferManager.renderExtent, framebufferManager.displayExtent);

        NVSDK_NGX_VK_DLSS_Eval_Params DLSSEvaluationParameters =
        {
            .Feature                       = {
                .pInColor    = &sceneColorNV,
                .pInOutput   = &resolvedSceneColorNV,
                .InSharpness = 0.0f
            },
            .pInDepth                      = &sceneDepthNV,
            .pInMotionVectors              = &gMotionVectorsNV,
            .InJitterOffsetX               = jitter.x,
            .InJitterOffsetY               = jitter.y,
            .InRenderSubrectDimensions     = {.Width = config.optimalResolution.x, .Height = config.optimalResolution.y},
            .InReset                       = config.resetNeeded ? 1 : 0,
            .InMVScaleX                    = -static_cast<f32>(config.optimalResolution.x),
            .InMVScaleY                    = -static_cast<f32>(config.optimalResolution.y),
            .pInTransparencyMask           = nullptr,
            .pInExposureTexture            = &exposureValueNV,
            .pInBiasCurrentColorMask       = nullptr,
            .InColorSubrectBase            = {.X = 0, .Y = 0},
            .InDepthSubrectBase            = {.X = 0, .Y = 0},
            .InMVSubrectBase               = {.X = 0, .Y = 0},
            .InTranslucencySubrectBase     = {.X = 0, .Y = 0},
            .InBiasCurrentColorSubrectBase = {.X = 0, .Y = 0},
            .InOutputSubrectBase           = {.X = 0, .Y = 0},
            .InPreExposure                 = 1.0f,
            .InExposureScale               = 1.0f,
            .InIndicatorInvertXAxis        = 0,
            .InIndicatorInvertYAxis        = 0,
            .GBufferSurface                = {},
            .InToneMapperType              = NVSDK_NGX_TONEMAPPER_ACES,
            .pInMotionVectors3D            = nullptr,
            .pInIsParticleMask             = nullptr,
            .pInAnimatedTextureMask        = nullptr,
            .pInDepthHighRes               = nullptr,
            .pInPositionViewSpace          = nullptr,
            .InFrameTimeDeltaInMsec        = 1000.0f * frameCounter.frameDelta,
            .pInRayTracingHitDistance      = nullptr,
            .pInMotionVectorsReflections   = nullptr
        };

        if (config.resetNeeded)
        {
            config.resetNeeded = false;
        }

        resolvedSceneColor.image.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = resolvedSceneColor.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = resolvedSceneColor.image.arrayLayers
            }
        );

        NVSDK_NGX_Result result = NGX_VULKAN_EVALUATE_DLSS_EXT
        (
            cmdBuffer.handle,
            config.handle,
            config.parameters,
            &DLSSEvaluationParameters
        );

        if (result != NVSDK_NGX_Result_Success)
        {
            Logger::Error("Failed to evaluate DLSS Feature! [Error={}]", static_cast<u64>(result));
        }

        resolvedSceneColor.image.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = resolvedSceneColor.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = resolvedSceneColor.image.arrayLayers
            }
        );
    }
}