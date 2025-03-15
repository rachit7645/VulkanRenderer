// Inputs: Screen XY, Viewspace Depth
// Output: Viewspace Position
vec3 XeGTAO_ComputeViewspacePosition(vec2 screenPos, float viewspaceDepth, vec4 ndcToView)
{
    vec3 ret;

    vec2 ndcToViewMul = ndcToView.xy;
    vec2 ndcToViewAdd = ndcToView.zw;

    ret.xy = (ndcToViewMul * screenPos + ndcToViewAdd) * viewspaceDepth;
    ret.z  = viewspaceDepth;

    return ret;
}

vec4 XeGTAO_CalculateEdges(float centerZ, float leftZ, float rightZ, float topZ, float bottomZ)
{
    vec4 edgesLRTB = vec4(leftZ, rightZ, topZ, bottomZ) - centerZ;

    float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5f;
    float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5f;

    vec4 edgesLRTBSlopeAdjusted = edgesLRTB + vec4(slopeLR, -slopeLR, slopeTB, -slopeTB);
    edgesLRTB                   = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));

    return vec4(saturate((1.25f - edgesLRTB / (centerZ * 0.011f))));
}

// Packing/unpacking for edges; 2 bits per edge mean 4 gradient values (0, 0.33, 0.66, 1) for smoother transitions!
float XeGTAO_PackEdges(vec4 edgesLRTB)
{
    edgesLRTB = round(saturate(edgesLRTB) * 2.9f);

    return dot(edgesLRTB, vec4(64.0f / 255.0f, 16.0f / 255.0f, 4.0f / 255.0f, 1.0f / 255.0f));
}

// http://h14s.p5r.org/2012/09/0x5f3759df.html, [Drobot2014a] Low Level Optimizations for GCN, https://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf slide 63
float XeGTAO_FastSqrt(float x)
{
    return intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(x) >> 1));
}

// https://seblagarde.wordpress.com/2014/12/01/inverse-trigonometric-functions-gpu-optimization-for-amd-gcn-architecture/
// Input:  [-1, 1]
// Output: [0, PI]
float XeGTAO_FastArcCos(float inX)
{ 
    const float FAST_ACOS_PI      = 3.141593;
    const float FAST_ACOS_HALF_PI = 1.570796;
    
    float x    = abs(inX);
    float res  = -0.156583f * x + FAST_ACOS_HALF_PI;
          res *= XeGTAO_FastSqrt(1.0f - x);
    
    return (inX >= 0) ? res : FAST_ACOS_PI - res;
}

// TODO: FIX!!!
void XeGTAO_OutputWorkingTerm(uvec2 pixelCoord, float visibility, vec3 bentNormal, RWTexture2D<uint> outWorkingAOTerm)
{
    visibility = saturate(visibility / float(XE_GTAO_OCCLUSION_TERM_SCALE));

    outWorkingAOTerm[pixelCoord] = uint(visibility * 255.0 + 0.5);
}

void XeGTAO_MainPass
(
    uvec2 pixelCoord,
    uint sliceCount,
    uint stepsPerSlice,
    vec2 localNoise,
    vec3 viewspaceNormal,
    texture2D sourceViewspaceDepth,
    sampler depthSampler,
    RWTexture2D<uint> outWorkingAOTerm,
    RWTexture2D<unorm float> outWorkingEdges
)
{                                                                       
    vec2 normalizedScreenPos = (pixelCoord + 0.5f) * ViewportPixelSize;

    vec4 valuesUL = sourceViewspaceDepth.GatherRed(depthSampler, vec2(pixelCoord * ViewportPixelSize));
    vec4 valuesBR = sourceViewspaceDepth.GatherRed(depthSampler, vec2(pixelCoord * ViewportPixelSize), ivec2(1, 1));

    // viewspace Z at the center
    float viewspaceZ = valuesUL.y; // sourceViewspaceDepth.SampleLevel( depthSampler, normalizedScreenPos, 0 ).x;

    // viewspace Zs left top right bottom
    float pixLZ = valuesUL.x;
    float pixTZ = valuesUL.z;
    float pixRZ = valuesBR.z;
    float pixBZ = valuesBR.x;

    vec4 edgesLRTB = XeGTAO_CalculateEdges(viewspaceZ, pixLZ, pixRZ, pixTZ, pixBZ);
    outWorkingEdges[pixelCoord] = XeGTAO_PackEdges(edgesLRTB);

    // Move center pixel slightly towards camera to avoid imprecision artifacts due to depth buffer imprecision; offset depends on depth texture format used
    viewspaceZ *= 0.99999; // this is good for FP32 depth buffer

    vec3 pixCenterPos = XeGTAO_ComputeViewspacePosition(normalizedScreenPos, viewspaceZ, NDCToView);
    vec3 viewVector   = normalize(-pixCenterPos);

    float effectRadius             = EffectRadius * RadiusMultiplier;
    float sampleDistributionPower  = SampleDistributionPower;
    float thinOccluderCompensation = ThinOccluderCompensation;
    float falloffRange             = EffectFalloffRange * effectRadius;

    float falloffFrom = effectRadius * (1.0f - EffectFalloffRange);

    // Fadeout precompute optimisation
    float falloffMul = -1.0f / falloffRange;
    float falloffAdd = falloffFrom / (falloffRange) + 1.0f;

    float visibility = 0.0f;
    vec3  bentNormal = vec3(0.0f);

    // see "Algorithm 1" in https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf
    {
        float noiseSlice  = localNoise.x;
        float noiseSample = localNoise.y;

        // quality settings / tweaks / hacks
        float pixelTooCloseThreshold = 1.3f; // if the offset is under approx pixel size (pixelTooCloseThreshold), push it out to the minimum distance

        // approx viewspace pixel size at pixelCoord; approximation of NDCToViewspace( normalizedScreenPos.xy + consts.ViewportPixelSize.xy, pixCenterPos.z ).xy - pixCenterPos.xy;
        vec2 pixelDirRBViewspaceSizeAtCenterZ = viewspaceZ.xx * NDCToViewMul_x_PixelSize;

        float screenspaceRadius = effectRadius / pixelDirRBViewspaceSizeAtCenterZ.x;

        // fade out for small screen radii 
        visibility += saturate((10.0f - screenspaceRadius) / 100.0f) * 0.5f;

        // this is the min distance to start sampling from to avoid sampling from the center pixel (no useful data obtained from sampling center pixel)
        float minS = pixelTooCloseThreshold / screenspaceRadius;

        for (uint slice = 0; slice < sliceCount; ++slice)
        {
            float sliceK = (slice + noiseSlice) / sliceCount;

            // lines 5, 6 from the paper
            float phi    = sliceK * PI;
            float cosPhi = cos(phi);
            float sinPhi = sin(phi);
            vec2  omega  = vec2(cosPhi, -sinPhi); // vec2 on omega causes issues with big radii

            // convert to screen units (pixels) for later use
            omega *= screenspaceRadius;

            // line 8 from the paper
            vec3 directionVector = vec3(cosPhi, sinPhi, 0);

            // line 9 from the paper
            vec3 orthoDirectionVector = directionVector - (dot(directionVector, viewVector) * viewVector);

            // line 10 from the paper
            // axisVector is orthogonal to directionVector and viewVector, used to define projectedNormal
            vec3 axisVector = normalize( cross(orthoDirectionVector, viewVector) );

            // line 11 from the paper
            vec3 projectedNormalVector = viewspaceNormal - axisVector * dot(viewspaceNormal, axisVector);

            // line 13 from the paper
            float signNorm = sign(dot(orthoDirectionVector, projectedNormalVector));

            // line 14 from the paper
            float projectedNormalVecLength = length(projectedNormalVector);
            float cosNorm                  = saturate(dot(projectedNormalVector, viewVector) / projectedNormalVecLength);

            // line 15 from the paper
            float n = signNorm * XeGTAO_FastArcCos(cosNorm);

            // this is a lower weight target; not using -1 as in the original paper because it is under horizon, so a 'weight' has different meaning based on the normal
            float lowHorizonCos0 = cos(n + (PI / 2.0f));
            float lowHorizonCos1 = cos(n - (PI / 2.0f));

            // lines 17, 18 from the paper, manually unrolled the 'side' loop
            float horizonCos0 = lowHorizonCos0; // -1;
            float horizonCos1 = lowHorizonCos1; // -1;
            
            for (uint stepIndex = 0; stepIndex < stepsPerSlice; ++stepIndex)
            {
                // R1 sequence (http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/)
                float stepBaseNoise = (slice + stepIndex * stepsPerSlice) * 0.6180339887498948482; // <- this should unroll
                float stepNoise     = fract(noiseSample + stepBaseNoise);

                // approx line 20 from the paper, with added noise
                float s = (stepIndex+stepNoise) / (stepsPerSlice); // + 1e-6f);

                // additional distribution modifier
                s  = pow(s, sampleDistributionPower);

                // avoid sampling center pixel
                s += minS;

                // approx lines 21-22 from the paper, unrolled
                vec2 sampleOffset = s * omega;

                float sampleOffsetLength = length( sampleOffset );

                // note: when sampling, using point_point_point or point_point_linear sampler works, but linear_linear_linear will cause unwanted interpolation between neighbouring depth values on the same MIP level!
                float mipLevel = clamp(log2(sampleOffsetLength) - DepthMIPSamplingOffset, 0, XE_GTAO_DEPTH_MIP_LEVELS);

                // Snap to pixel center (more correct direction math, avoids artifacts due to sampling pos not matching depth texel center - messes up slope - but adds other 
                // artifacts due to them being pushed off the slice). Also use full precision for high res cases.
                sampleOffset = round(sampleOffset) * ViewportPixelSize;

                vec2  sampleScreenPos0 = normalizedScreenPos + sampleOffset;
                float SZ0              = sourceViewspaceDepth.SampleLevel( depthSampler, sampleScreenPos0, mipLevel ).x;
                vec3  samplePos0       = XeGTAO_ComputeViewspacePosition( sampleScreenPos0, SZ0, NDCToView);

                vec2  sampleScreenPos1 = normalizedScreenPos - sampleOffset;
                float SZ1              = sourceViewspaceDepth.SampleLevel( depthSampler, sampleScreenPos1, mipLevel).x;
                vec3  samplePos1       = XeGTAO_ComputeViewspacePosition( sampleScreenPos1, SZ1, NDCToView);

                vec3  sampleDelta0 = (samplePos0 - vec3(pixCenterPos)); // using float for sampleDelta causes precision issues
                vec3  sampleDelta1 = (samplePos1 - vec3(pixCenterPos)); // using float for sampleDelta causes precision issues
                float sampleDist0  = length(sampleDelta0);
                float sampleDist1  = length(sampleDelta1);

                // approx lines 23, 24 from the paper, unrolled
                vec3 sampleHorizonVec0 = sampleDelta0 / sampleDist0;
                vec3 sampleHorizonVec1 = sampleDelta1 / sampleDist1;
                
                // this is our own thickness heuristic that relies on sooner discarding samples behind the center
                float falloffBase0 = length(vec3(sampleDelta0.x, sampleDelta0.y, sampleDelta0.z * (1.0f + thinOccluderCompensation) ) );
                float falloffBase1 = length(vec3(sampleDelta1.x, sampleDelta1.y, sampleDelta1.z * (1.0f + thinOccluderCompensation) ) );
                float weight0      = saturate(falloffBase0 * falloffMul + falloffAdd);
                float weight1      = saturate(falloffBase1 * falloffMul + falloffAdd);

                // sample horizon cos
                float shc0 = dot(sampleHorizonVec0, viewVector);
                float shc1 = dot(sampleHorizonVec1, viewVector);

                // discard unwanted samples
                shc0 = mix(lowHorizonCos0, shc0, weight0); // this would be more correct but too expensive: cos(mix(acos(lowHorizonCos0), acos(shc0), weight0));
                shc1 = mix(lowHorizonCos1, shc1, weight1); // this would be more correct but too expensive: cos(mix(acos(lowHorizonCos1), acos(shc1), weight1));
                
                // this is a version where thicknessHeuristic is completely disabled
                horizonCos0 = max(horizonCos0, shc0);
                horizonCos1 = max(horizonCos1, shc1);
            }

            // I can't figure out the slight overdarkening on high slopes, so I'm adding this fudge - in the training set, 0.05 is close (PSNR 21.34) to disabled (PSNR 21.45)
            projectedNormalVecLength = mix( projectedNormalVecLength, 1, 0.05 );

            // line ~27, unrolled
            float h0 = -XeGTAO_FastArcCos(horizonCos1);
            float h1 =  XeGTAO_FastArcCos(horizonCos0);

            float iarc0 = (cosNorm + 2.0f * h0 * sin(n) - cos(2.0f * h0 - n)) / 4.0f;
            float iarc1 = (cosNorm + 2.0f * h1 * sin(n) - cos(2.0f * h1 - n)) / 4.0f;
            
            float localVisibility = projectedNormalVecLength * (iarc0 + iarc1);
            visibility           += localVisibility;
        }
        
        visibility /= float(sliceCount);
        visibility  = pow(visibility, FinalValuePower);
        visibility  = max(visibility, 0.03f); // disallow total occlusion (which wouldn't make any sense anyhow since pixel is visible but also helps with packing bent normals)
    }

    XeGTAO_OutputWorkingTerm(pixelCoord, visibility, bentNormal, outWorkingAOTerm);
}