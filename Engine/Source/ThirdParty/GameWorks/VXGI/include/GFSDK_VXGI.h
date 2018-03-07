/*
* Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef GFSDK_VXGI_H_
#define GFSDK_VXGI_H_

/*
* Maxwell [*]: in this file, the name Maxwell refers to NVIDIA GPU architecture used in GM20x and later chips.
* Current products using these chips are GeForce GTX 960, 970 and 980, based on GM204 and GM206 GPUs.
* GM10x GPUs also have the architecture name "Maxwell", but they do not have the hardware features required to accelerate VXGI.
*/

// Define this symbol to 1 in order to replace the static DLL binding with function pointers obtained at run time.
#ifndef VXGI_DYNAMIC_LOAD_LIBRARY
#define VXGI_DYNAMIC_LOAD_LIBRARY 0
#endif

#ifndef CONFIGURATION_TYPE_DLL
#define CONFIGURATION_TYPE_DLL 0
#endif

#if VXGI_DYNAMIC_LOAD_LIBRARY
#include <Windows.h>
#endif

#include "GFSDK_NVRHI.h"
#include "GFSDK_VXGI_MathTypes.h"

#define VXGI_FAILED(status) ((status) != ::VXGI::Status::OK)
#define VXGI_SUCCEEDED(status) ((status) == ::VXGI::Status::OK)
#define VXGI_VERSION_STRING "1.0.0.20785853"

GI_BEGIN_PACKING
namespace VXGI
{
    struct Version
    {
        Version() : Major(1), Minor(0), Branch(0), Revision(20785853)
        { }

        uint32_t Major;
        uint32_t Minor;
        uint32_t Branch;
        uint32_t Revision;
    };

    struct Status
    {
        enum Enum
        {
            OK = 0,
            WRONG_INTERFACE_VERSION = 1,

            D3D_COMPILER_UNAVAILABLE,
            INSUFFICIENT_BINDING_SLOTS,
            INTERNAL_ERROR,
            INVALID_ARGUMENT,
            INVALID_CONFIGURATION,
            INVALID_SHADER_BINARY,
            INVALID_SHADER_SOURCE,
            INVALID_STATE,
            NULL_ARGUMENT,
            RESOURCE_CREATION_FAILED,
            SHADER_COMPILATION_ERROR,
            SHADER_MISSING,
            FUNCTION_MISSING,
            BUFFER_TOO_SMALL,
            NOT_SUPPORTED
        };
    };

    struct OpacityDirections
    {
        enum Enum
        {
            THREE_DIMENSIONAL = 3,
            SIX_DIMENSIONAL = 6,
        };
    };

    struct EmittanceFormat
    {
        enum Enum
        {
            NONE = 0,     // no emittance - ambient occlusion mode
            PERFORMANCE = 1,  // use FLOAT16 if it is supported, UNORM8 otherwise
            QUALITY = 2,      // use FLOAT16 if it is supported, FLOAT32 otherwise
            UNORM8 = 3,   // use RGBA8_UNORM_SRGB - lowest quality mode
            FLOAT16 = 4,  // use RGBA16_FLOAT; only supported on Maxwell [*] GPUs when enableNvidiaExtensions == true
            FLOAT32 = 5   // use 3x R32_FLOAT textures
        };
    };

    struct DebugRenderMode
    {
        enum Enum
        {
            DISABLED = 0,
            ALLOCATION_MAP,
            OPACITY_TEXTURE,
            EMITTANCE_TEXTURE,
            INDIRECT_IRRADIANCE_TEXTURE
        };
    };

    struct VoxelSizeFunction
    {
        enum Enum
        {
            EXACT = 0,
            LINEAR_UNDERESTIMATE = 1,
            LINEAR_OVERESTIMATE = 2
        };
    };

    struct CommonTracingParameters
    {
        // Maximum number of samples that can be fetched for each cone
        uint32_t    maxSamples;

        // Tracing step.
        // Reasonable values [0.5, 1]
        // Sampling with lower step produces more stable results at Performance cost.
        float       tracingStep;


        // Opacity correction factor.
        // Reasonable values [0.1, 10]
        // Higher values produce more contrast rendering, overall picture looks darker.
        float       opacityCorrectionFactor;

        // Multiplier for the incoming light intensity.
        float       irradianceScale;

        // Flips the direction in which geometry blocks light - sometimes it helps improve occlusion quality, 
        // for example it reduces "peter panning" of objects in specular reflections.
        bool        flipOpacityDirections;

        // These should be set to zero and normally have no effect
        Vector4f    debugParameters;

        // Projection parameters: near and far clip planes' post-projection Z values
        float       nearClipZ;  // 0.0 for regular projections
        float       farClipZ;   // 1.0 for regular projections

        CommonTracingParameters()
            : maxSamples(128)
            , tracingStep(1.0f)
            , opacityCorrectionFactor(1.0f)
            , irradianceScale(1.0f)
            , flipOpacityDirections(false)
            , debugParameters(0.0f)
            , nearClipZ(0.0f)
            , farClipZ(1.0f)
        { }
    };

    struct DiffuseTracingParameters : public CommonTracingParameters
    {
        // Number of diffuse cones to trace for each fragment, 4 or more.
        // Balances Quality (more cones) vs Performance
        uint32_t    numCones;

        // Automatic diffuse angle computation based on the number of cones
        // Overrides the value set in coneAngle.
        // Use IViewTracer::getDiffuseConeAngle(numCones) to get the actual diffuse angle.
        bool        autoConeAngle;

        // Cone angle for GI diffuse component evaluation, in degrees
        // This value has no effect if autoConeAngle == true
        float       coneAngle;

        // Optional color for adding occluded ambient lighting.
        // To get a normalized ambient occlusion only rendering, set ambientColor to 1.0 and irradianceScale to 0.
        Vector3f    ambientColor;

        // World-space distance at which the contribution of geometry to ambient occlusion will be 10x smaller than near the surface.
        float       ambientRange;

        // Other parameters for the ambient term.
        // correctedConeAmbient = pow(saturate(cone.ambient * ambientScale + ambientBias, ambientPower))
        float       ambientScale;
        float       ambientBias;
        float       ambientPower;

        // Parameter that controls how much darker to make ambient occlusion at distance. [0..1]
        // With ambientDistanceDarkening == 0, occlusion tends to disappear close to clip-map edges because of large initial offsets.
        float       ambientDistanceDarkening;

        // Diffuse tracing results can be alpha-blended over this color, where alpha is indirect / AO confidence.
        // If this color is black, diffuse RGB are not pre-multiplied by alpha.
        Vector3f    backgroundColor;

        // Environment map to use when diffuse cones don't hit any geometry. Optional.
        NVRHI::TextureHandle environmentMap;

        // Multiplier for the environment map colors.
        // The environment map samples are multiplied by cones' final transmittance values.
        // In ambient occlusion mode, i.e. when VoxelizationParameters::emittanceFormat == NONE, the environment map is ignored.
        Vector3f    environmentMapTint;

        // Random per-pixel rotation of the diffuse cone set - it helps reduce banding but costs some performance
        bool        enableConeRotation;

        // Random per-pixel adjustment of initial tracing offsets for diffuse tracing, also helps reduce banding
        bool        enableRandomConeOffsets;

        // A factor that controls linear interpolation between smoothNormal and ray direction.
        // The interpolated value is used to direct the initial cone offset.
        // Accepted values are [0, 1]
        float       normalOffsetFactor;

        // Diffuse tracing sparsity. 
        // 1 = dense tracing (for every pixel),
        // 2..4 = sparse tracing (for one pixel in every NxN square).
        // Using sparse tracing greatly improves performance in exchange for fine detail quality.
        uint32_t    tracingSparsity;

        // Bigger factor would move the diffuse cones closer to the surface normal
        // Reasonable values [0, 1]
        float       coneNormalGroupingFactor;

        // Parameters that control the distance of the first sample from the surface
        float       initialOffsetBias;
        float       initialOffsetDistanceFactor;

        // Enables reuse of diffuse tracing results from the previous frame.
        // For this mode to work, different G-buffer textures have to be used for consecutive frames,
        // at least the depth and normal channels. Otherwise the tracing calls will fail.
        bool        enableTemporalReprojection;

        // Weight of the reprojected irradiance data relative to newly computed data, (0..1),
        // where 0 means do not use reprojection, and values closer to 1 mean accumulate data over more previous frames.
        // Higher values result in better filtering of random rotation/offset noise, but also introduce a lag of GI from moving objects.
        // 1.0 or higher will produce an unstable result.
        float       temporalReprojectionWeight;

        // Maximum distance between two samples for which they're still considered to be the same surface, expressed in voxels.
        float       temporalReprojectionMaxDistanceInVoxels;

        // The exponent used for the dot product of old and new normals in the temporal reprojection filter
        float       temporalReprojectionNormalWeightExponent;

        // Normally, the computeDiffuseChannel method will test that the previous-frame input buffers are
        // different from the current-frame ones, and disable reprojection if they are the same.
        // Setting enableReprojectionFromSameFrame == true removes this test.
        bool        enableReprojectionFromSameFrame;

        // Enables a second tracing pass, performed only on pixels that do not have enough information from the sparse tracing pass
        bool        enableSparseTracingRefinement;

        // Minimum pixel weight for sparse tracing interpolation. When a pixel is below that weight, it is considered to be
        // a hole and submitted for refinement. When refinement is disabled, such pixels will be black with zero confidence.
        // Using a higher threshold improves image quality and reduces temporal noise at the cost of refinement pass performance.
        // Clamped to [0, 1].
        float       interpolationWeightThreshold;

        // Versions of the settings for objects marked in the stencil buffer.
        // See IViewTracer::InputBuffers::altSettingsStencilMask and altSettingsStencilRefValue for more info.
        float       altInitialOffsetBias;
        float       altInitialOffsetDistanceFactor;
        float       altNormalOffsetFactor;
        float       altTracingStep;

        // Enables a built-in SSAO pass that is multiplied into the diffuse tracing results.
        // The SSAO algorithm used here and its parameters are very similar to HBAO+.
        bool        enableSSAO;
        float       SSAO_SurfaceBias;
        float       SSAO_RadiusWorld;
        float       SSAO_BackgroundViewDepth;
        float       SSAO_Scale;
        float       SSAO_PowerExponent;

        DiffuseTracingParameters()
            : numCones(8)
            , autoConeAngle(true)
            , coneAngle(60.0f)
            , ambientColor(0.f)
            , ambientRange(512.0f)
            , ambientScale(1.0f)
            , ambientBias(0.0f)
            , ambientPower(1.0f)
            , ambientDistanceDarkening(0.25f)
            , environmentMap(NULL)
            , environmentMapTint(0.f)
            , enableConeRotation(false)
            , enableRandomConeOffsets(false)
            , normalOffsetFactor(0.5f)
            , tracingSparsity(2)
            , coneNormalGroupingFactor(0.0f)
            , initialOffsetBias(2.0f)
            , initialOffsetDistanceFactor(1.0f)
            , altInitialOffsetBias(2.0f)
            , altInitialOffsetDistanceFactor(1.0f)
            , altNormalOffsetFactor(0.5f)
            , altTracingStep(0.5f)
            , enableTemporalReprojection(false)
            , temporalReprojectionWeight(0.9f)
            , temporalReprojectionMaxDistanceInVoxels(0.25f)
            , temporalReprojectionNormalWeightExponent(20.f)
            , enableReprojectionFromSameFrame(false)
            , enableSparseTracingRefinement(true)
            , interpolationWeightThreshold(1e-4f)
            , enableSSAO(false)
            , SSAO_SurfaceBias(0.2f)
            , SSAO_RadiusWorld(50.f)
            , SSAO_BackgroundViewDepth(1000.f)
            , SSAO_Scale(1.f)
            , SSAO_PowerExponent(2.f)
        {
            // Override the default step of 1.0
            tracingStep = 0.5f;
        }
    };

    struct SpecularTracingParameters : public CommonTracingParameters
    {
        enum Filter
        {
            FILTER_NONE,
            FILTER_TEMPORAL,
            FILTER_SIMPLE
        };

        // Selects the filter that is used on the specular surface after tracing in order to reduce noise introduced by cone jitter.
        Filter      filter;

        // Parameters that control the distance of the first specular cone sample from the surface
        float       initialOffsetBias;
        float       initialOffsetDistanceFactor;

        // Environment map to use when specular cones don't hit any geometry. Optional.
        NVRHI::TextureHandle environmentMap;

        // Multiplier for environment map reflections in the specular channel.
        // The environment map will only be visible on pixels that do not reflect any solid geometry.
        Vector3f    environmentMapTint;

        // Weight of the reprojected irradiance data relative to newly computed data, [0..1).
        // Only applicable if filter == FILTER_TEMPORAL.
        // Also see the comment for DiffuseTracingParameters::enableTemporalReprojection and temporalReprojectionWeight.
        float       temporalReprojectionWeight;

        // Maximum distance between two samples for which they're still considered to be the same surface, expressed in voxels.
        float       temporalReprojectionMaxDistanceInVoxels;

        // The exponent used for the dot product of old and new normals in the temporal reprojection filter
        float       temporalReprojectionNormalWeightExponent;

        // Normally, the computeSpecularChannel method will test that the previous-frame input buffers are
        // different from the current-frame ones, and disable reprojection if they are the same.
        // Setting enableReprojectionFromSameFrame == true removes this test.
        bool        enableReprojectionFromSameFrame;

        // Scale of the jitter can be added to specular sample positions to reduce blockiness of the reflections, [0..1]
        // When filter == FILTER_TEMPORAL, the noise pattern is different on every frame. 
        float       tangentJitterScale;

        SpecularTracingParameters()
            : filter(FILTER_SIMPLE)
            , initialOffsetBias(2.0f)
            , initialOffsetDistanceFactor(1.0f)
            , environmentMap(NULL)
            , environmentMapTint(0.0f)
            , temporalReprojectionWeight(0.8f)
            , tangentJitterScale(0.0f)
            , temporalReprojectionMaxDistanceInVoxels(0.25f)
            , temporalReprojectionNormalWeightExponent(20.f)
            , enableReprojectionFromSameFrame(false)
        { }
    };

    struct TracerVisionParameters : public CommonTracingParameters
    {
        // Cone angle for the Tracer Vision debug mode
        float       coneAngle;

        TracerVisionParameters()
            : coneAngle(1.0f)
        { }
    };

    struct IndirectIrradianceMapTracingParameters : public CommonTracingParameters
    {
        float       coneAngle;

        // Auto-normalization is a safeguard algorithm that attempts to prevent irradiance from blowing up.
        // In some cases, such as when the value of irradianceScale in this structure is too large, or when multiple 
        // surfaces with the same orientation, one behind another, receive indirect illumination,
        // multibounce tracing feedback factor may become greater than 1, which means irradiance will blow up 
        // exponentially, causing numeric overflows in the voxel textures (or saturation in case UNORM8 emittance is used).
        // The normalization algorithm analyzes average irradiance over multiple frames, and adjusts irradianceScale
        // when it detects that an exponential blow-up is happening.
        // Adjustment is done internally, not visible to the user, and the adjustment factor is reset to 1.0 when 
        // the value of irradianceScale in this structure is different from one used on the previous frame.
        bool        useAutoNormalization;

        // Hard limit for indirect irradiance values. Useful in cases when auto-normalization doesn't work properly.
        // When irradianceClampValue <= 0 or >= 65504.0, the internal limit is set to 65504.0 because that's the maximum
        // value representable as float16 that's not infinity.
        float       irradianceClampValue;

        IndirectIrradianceMapTracingParameters()
            : coneAngle(40.f)
            , useAutoNormalization(true)
            , irradianceClampValue(0.f)
        { }
    };

    struct VoxelizationParameters
    {
        // Controls voxelization density, must be a power of 2 and within range [16, 256]
        uint32_t mapSize;

        // Controls allocation granularity, bigger values result in coarser allocation map being used
        uint32_t allocationMapLodBias;

        // Number of levels in a clipmap stack used for scene representation.
        // Reasonable values [2, log2(MapSize) - 1]
        uint32_t stackLevels;

        // Number of levels in a mipmap stack used for scene representation.
        // These levels do not include stack levels, and there can be 0 of them.
        uint32_t mipLevels;

        // Controls whether opacity and emittance voxel data can be preserved between frames. 
        // If not, the whole clipmap will be automatically invalidated during the prepareForOpacityVoxelization call,
        // and some passes of the VXGI algorithm will become faster.
        bool persistentVoxelData;

        // Enables a mode where VXGI does not optimize the invalidation regions before processing them on the GPU.
        // Updates to voxel data on the GPU will still happen with one page granularity, but the application will
        // receive just one "optimized" region which is a union of all submitted regions, rounded to page size.
        // The value of simplifiedInvalidate does not matter if persistentVoxelData == false.
        bool simplifiedInvalidate;

        // The number of opacity directions stored per voxel.
        OpacityDirections::Enum opacityDirectionCount;

        // Controls whether the library should try to use NVIDIA-specific hardware features
        bool enableNvidiaExtensions;

        // Enables the use of Maxwell Geometry Shader Pass-Through feature for voxelization.
        // Only effective when enableNvidiaExtensions == true.
        // Sometimes pass-through shaders do not work properly (e.g. broken texture coordinates in PS) while other Maxwell features do,
        // so this flag allows applications to work around the issues at a small performance cost.
        bool enableGeometryShaderPassthrough;

        // Controls the format of the textures used to store emittance.
        EmittanceFormat::Enum emittanceFormat;

        // Global multiplier for emittance voxels - adjust it according to your light intensities to avoid clamping or quantization.
        // As long as neither of these effects takes place, changing this parameter has no effect on rendered images.
        float emittanceStorageScale;

        // Enables a mode wherein transitions from downsampled to directly voxelized emittance are smoothed
        // by blending the two with a factor that depends on the exact clipmap anchor, not the quantized clipmap center.
        // This mode requires persistentVoxelData = false to operate properly.
        bool useEmittanceInterpolation;

        // Enables a higher-order filter to be used during emittance downsampling.
        // Using this filter makes moving objects produce much smoother indirect illumination, at a significant performance cost.
        bool useHighQualityEmittanceDownsampling;

        // Controls whether a separate indirect irradiance 3D map is computed for use on the next frame.
        // In order to see multi-bounce lighting, call this function in the emittance voxelization pixel shader:
        //
        //    float3 VxgiGetIndirectIrradiance(float3 worldPos, float3 normal)
        //
        // Then shade the material with the returned indirect irradiance (don't forget to divide it by PI) and add the result to output color.
        bool enableMultiBounce;

        // Controls the size of the indirect irradiance map.
        // - indirectIrradianceMapLodBias == 0 means that the indirect irradiance map will have the same size and resolution 
        //    as the coarsest clipmap level.
        // - indirectIrradianceMapLodBias > 0 means that there will be fewer voxels, but not fewer than in the allocation map, 
        //    i.e. indirectIrradianceMapLodBias <= allocationMapLodBias.
        // - indirectIrradianceMapLodBias < 0 means that there will be more voxels, but not more than 256^3, and the resolution 
        //    will not be finer than the finest clipmap level, i.e. indirectIrradianceMapLodBias > -stackLevels.
        int32_t indirectIrradianceMapLodBias;

        // Default constructor sets default parameters
        VoxelizationParameters()
            : mapSize(64)
            , allocationMapLodBias(0)
            , stackLevels(5)
            , mipLevels(5)
            , persistentVoxelData(true)
            , simplifiedInvalidate(true)
            , opacityDirectionCount(OpacityDirections::SIX_DIMENSIONAL)
            , enableNvidiaExtensions(true)
            , enableGeometryShaderPassthrough(true)
            , emittanceFormat(EmittanceFormat::PERFORMANCE)
            , emittanceStorageScale(1.0f)
            , useEmittanceInterpolation(false)
            , useHighQualityEmittanceDownsampling(false)
            , enableMultiBounce(false)
            , indirectIrradianceMapLodBias(0)
        { }

        bool operator!=(const VoxelizationParameters& parameters) const
        {
            if (parameters.mapSize != mapSize ||
                parameters.allocationMapLodBias != allocationMapLodBias ||
                parameters.stackLevels != stackLevels ||
                parameters.mipLevels != mipLevels ||
                parameters.persistentVoxelData != persistentVoxelData ||
                parameters.simplifiedInvalidate != simplifiedInvalidate ||
                parameters.opacityDirectionCount != opacityDirectionCount ||
                parameters.enableNvidiaExtensions != enableNvidiaExtensions ||
                parameters.enableGeometryShaderPassthrough != enableGeometryShaderPassthrough ||
                parameters.emittanceFormat != emittanceFormat ||
                parameters.emittanceStorageScale != emittanceStorageScale ||
                parameters.useEmittanceInterpolation != useEmittanceInterpolation ||
                parameters.useHighQualityEmittanceDownsampling != useHighQualityEmittanceDownsampling ||
                parameters.enableMultiBounce != enableMultiBounce ||
                parameters.indirectIrradianceMapLodBias != indirectIrradianceMapLodBias
                ) return true;

            return false;
        }

        bool operator==(const VoxelizationParameters& parameters) const
        {
            return !(*this != parameters);
        }
    };

    struct TracedSamplesParameters
    {
        enum ColorMode
        {
            COLOR_MIP_LEVEL = 0,
            COLOR_EMITTANCE = 1,
            COLOR_OCCLUSION = 2,
            COLOR_TEXELS_LOWER_MIP = 3,
            COLOR_TEXELS_UPPER_MIP = 4,
        };

        ColorMode   colorMode;
        bool        onlyContributingSamples;
        int32_t     coneIndexFilter;
        int32_t     sampleIndexFilter;
        bool        showConeDirections;

        TracedSamplesParameters()
            : onlyContributingSamples(false)
            , coneIndexFilter(0)
            , sampleIndexFilter(0)
            , showConeDirections(false)
            , colorMode(COLOR_MIP_LEVEL)
        { }
    };

    class IUserDefinedShaderSet;

    struct MaterialSamplingRate
    {
        enum Enum
        {
            // Fixed rates, i.e. use the same rasterization resolution for all clipmap LODs.
            FIXED_DEFAULT,
            FIXED_2X,
            FIXED_3X,
            FIXED_4X,
            // Adaptive rates, i.e. coarser clipmap LODs use higher rasterization resolutions.
            // Default:            LOD 0 -> 1x, LOD 1 -> 2x, LOD 2 -> 4x, LOD 3 -> 8x, LOD 4 -> 16x
            ADAPTIVE_DEFAULT,
            // Greater/equal to 2: LOD 0 -> 2x, LOD 1 -> 2x, LOD 2 -> 4x, LOD 3 -> 8x, LOD 4 -> 16x
            ADAPTIVE_GE2,
            // Greater/equal to 4: LOD 0 -> 4x, LOD 1 -> 4x, LOD 2 -> 4x, LOD 3 -> 8x, LOD 4 -> 16x
            ADAPTIVE_GE4
        };
    };

    struct MaterialInfo
    {
        IUserDefinedShaderSet*   pixelShader;
        IUserDefinedShaderSet*   geometryShader;

        // Opacity voxelization thickness in voxels. [0..2]
        // Thickness affects the quality of occlusion produced by an object. Big thickness values should be used for walls, and small ones 
        // should be used for objects whose size is comparable to a voxel.
        // When a triangle is voxelized, it covers a set of small cubes. The sides of these cubes are 1/3 voxel, so one voxel contains 27 such cubes.
        // The voxelizationThickness parameter controls how many of these cubes does a triangle cover along its normal.
        // It is measured in voxels for convenience, e.g. when thickness <= 1/3, only one cube is covered; when it's 1.0, three cubes are covered.
        float                  voxelizationThickness;

        // Opacity voxelization anti-aliasing through jitter. Individual samples covered by objects are moved 
        // along the voxelization Z axis in order to smoothen the opacity transitions as objects move.
        float                  opacityNoiseScale; // in voxels, [-1..1]
        float                  opacityNoiseBias;  // in voxels, [-0.5..0.5]

        // Controls whether signed opacity representation of this object should block light in all directions, not just in front-to-back direction
        bool                   twoSided;

        // Set this to true if the geometry is represented in FrontCCW mode
        bool                   frontCounterClockwise;

        // Emittance voxelization anti-aliasing through a triangular filter applied in voxelization Z direction.
        // This filter is almost free in terms of performance, but it makes directly voxelized emittance differ from downsampled emittance.
        // For that reason, this filter should be used mostly on large dynamic objects.
        bool                   proportionalEmittance;

        // Controls whether light emitted by this material is omnidirectional. Set this to true for small objects.
        bool                   omnidirectionalLight;

        // A multiplier for material sampling rate during emittance voxelization, basically for rasterization resolution.
        // It's useful to use higher sampling rates in case shading results cannot be filtered well enough with once-per-voxel shading.
        // That happens for example with materials that have binary masks over textures or colors, because the mask cannot be filtered 
        // with standard texture sampling.
        MaterialSamplingRate::Enum materialSamplingRate;

        // Allows the geometry shader to cull triangles that fit into one page which is not marked for invalidation.
        // Improves performance when application mesh culling is too coarse in incremental voxelization mode.
        bool                   enableTriangleCulling;

        MaterialInfo()
            : pixelShader(NULL)
            , geometryShader(NULL)
            , voxelizationThickness(1.f)
            , opacityNoiseScale(0.f)
            , opacityNoiseBias(0.f)
            , twoSided(false)
            , frontCounterClockwise(false)
            , proportionalEmittance(false)
            , omnidirectionalLight(false)
            , materialSamplingRate(MaterialSamplingRate::FIXED_DEFAULT)
            , enableTriangleCulling(true)
        { }

        bool requiresNewState(const MaterialInfo& b) const
        {
            return
                pixelShader != b.pixelShader ||
                geometryShader != b.geometryShader ||
                materialSamplingRate != b.materialSamplingRate ||
                enableTriangleCulling != b.enableTriangleCulling;
        }

        bool requiresParameterUpdate(const MaterialInfo& b) const
        {
            return
                voxelizationThickness != b.voxelizationThickness ||
                opacityNoiseScale != b.opacityNoiseScale ||
                opacityNoiseBias != b.opacityNoiseBias ||
                twoSided != b.twoSided ||
                frontCounterClockwise != b.frontCounterClockwise ||
                proportionalEmittance != b.proportionalEmittance ||
                omnidirectionalLight != b.omnidirectionalLight;
        }

        bool operator == (const MaterialInfo& b) const
        {
            return !(*this != b);
        }

        bool operator != (const MaterialInfo& b) const
        {
            return requiresNewState(b) || requiresParameterUpdate(b);
        }
    };

    // Should be implemented by the application. 
    // Not essential to VXGI operation and is useful for performance measurements only.
    class IPerformanceMonitor
    {
        IPerformanceMonitor& operator=(const IPerformanceMonitor& other); //undefined
    protected:
        virtual ~IPerformanceMonitor() {};
    public:
        virtual void beginSection(const char* pSectionName) = 0;
        virtual void endSection() = 0;
    };

    // Should be implemented by the application.
    class IAllocator
    {
        IAllocator& operator=(const IAllocator& other); //undefined
    protected:
        virtual ~IAllocator() {};
    public:
        virtual void* allocateMemory(size_t size) = 0;
        virtual void freeMemory(void* ptr) = 0;
    };

    class IViewTracer
    {
        IViewTracer& operator=(const IViewTracer& other); //undefined
    protected:
        virtual ~IViewTracer() {};
    public:

        struct InputBuffers
        {
            // Handles for G-buffer textures
            NVRHI::TextureHandle gbufferDepth;     // Depth buffer, required
            NVRHI::TextureHandle gbufferNormal;    // Normals (.xyz) and roughness (.w), required
            NVRHI::TextureHandle gbufferGeoNormal; // Normals without normal maps (.xyz), optional - improves performance
            NVRHI::TextureHandle gbufferStencil;   // Stencil buffer for alternative tracing settings, optional

            // Parameters of the camera that was used to render the G-buffer, required
            Matrix4f viewMatrix;
            Matrix4f projMatrix;

            // Viewport within the G-buffer textures, required
            NVRHI::Viewport gbufferViewport;

            // Scale and bias for decoding the contents of gbufferNormal and gbufferGeoNormal textures.
            // The effective normal N is computed like this: N = normalize(gbufferNormal.xyz * gbufferNormalScale + gbufferNormalBias)
            float gbufferNormalScale;
            float gbufferNormalBias;

            // Parameters to determine whether to use alt-settings based on a stencil value.
            // Evaluated as (stencilValue & altSettingsStencilMask) == altSettingsStencilRefValue.
            // Alt-settings are disabled by default.
            int altSettingsStencilMask;
            int altSettingsStencilRefValue;

            InputBuffers()
                : gbufferNormalScale(1.0f)
                , gbufferNormalBias(0.0f)
                , gbufferViewport()
                , gbufferDepth(0)
                , gbufferStencil(0)
                , gbufferNormal(0)
                , gbufferGeoNormal(0)
                , altSettingsStencilMask(0)
                , altSettingsStencilRefValue(1)
            {}
        };

        // Computes the indirect diffuse illumination and returns the surface containing it.
        virtual Status::Enum computeDiffuseChannel(const DiffuseTracingParameters& params, NVRHI::TextureHandle& outDiffuse, const InputBuffers& inputBuffers, const InputBuffers* inputBuffersPreviousFrame = NULL) = 0;

        // Computes the indirect specular illumination and returns the surface containing it.
        virtual Status::Enum computeSpecularChannel(const SpecularTracingParameters& params, NVRHI::TextureHandle& outSpecular, const InputBuffers& inputBuffers, const InputBuffers* inputBuffersPreviousFrame = NULL) = 0;

        // Render the "debug samples" visualization for the samples that were previously saved
        // using a setPixelToSave call followed by computeDiffuseChannel or computeSpecularChannel
        // Uses the matrices and viewport from the inputBuffers structure, all other fields are ignored.
        virtual Status::Enum renderSamplesDebug(
            NVRHI::TextureHandle destinationTexture,
            NVRHI::TextureHandle destinationDepth,
            const TracedSamplesParameters &params,
            const InputBuffers& inputBuffers) = 0;

        // Render the "tracer vision" visualization, which performs cone tracing from the camera
        // rather than from the surfaces that are visible to the camera.
        // Uses the matrices and viewport from the inputBuffers structure, all other fields are ignored.
        virtual Status::Enum renderTracerVision(const TracerVisionParameters& params, NVRHI::TextureHandle destinationTexture, const InputBuffers& inputBuffers) = 0;

        // Sets the pixel for which to save texture samples performed during cone tracing.
        virtual void setPixelToSave(int x, int y) = 0;

        // Returns the diffuse cone angle that is used for 'numCones' cones when autoConeAngle is true.
        virtual float getDiffuseConeAngle(int numCones) = 0;
    };

    struct ShaderResources
    {
        enum { MAX_TEXTURE_BINDINGS = 128, MAX_SAMPLER_BINDINGS = 16, MAX_CB_BINDINGS = 15, MAX_UAV_BINDINGS = 64 };

        uint32_t textureSlots[MAX_TEXTURE_BINDINGS];
        uint32_t textureCount;

        uint32_t samplerSlots[MAX_SAMPLER_BINDINGS];
        uint32_t samplerCount;

        uint32_t constantBufferSlots[MAX_CB_BINDINGS];
        uint32_t constantBufferCount;

        uint32_t unorderedAccessViewSlots[MAX_UAV_BINDINGS];
        uint32_t unorderedAccessViewCount;

        ShaderResources()
            : textureCount(0)
            , samplerCount(0)
            , constantBufferCount(0)
            , unorderedAccessViewCount(0)
        {
            memset(textureSlots, 0, sizeof(textureSlots));
            memset(samplerSlots, 0, sizeof(samplerSlots));
            memset(constantBufferSlots, 0, sizeof(constantBufferSlots));
            memset(unorderedAccessViewSlots, 0, sizeof(unorderedAccessViewSlots));
        }
    };

    // This structure describes the attributes that have to be passed through the voxelization geometry shader.
    // The geometry shader code is generated by VXGI and is not available to the application.
    struct VoxelizationGeometryShaderDesc
    {
        enum { MAX_NAME_LENGTH = 512, MAX_ATTRIBUTE_COUNT = 32 };
        enum AttributeType
        {
            FLOAT_ATTR,
            INT_ATTR,
            UINT_ATTR,
        };

        struct Attribute
        {
            AttributeType type;
            uint32_t width, semanticIndex;
            char name[MAX_NAME_LENGTH];
            char semantic[MAX_NAME_LENGTH];

            Attribute() : type(FLOAT_ATTR), width(4), semanticIndex(0)
            {
                memset(name, 0, sizeof(name));
                memset(semantic, 0, sizeof(semantic));
            }
        };
        uint32_t pixelShaderInputCount;
        Attribute pixelShaderInputs[MAX_ATTRIBUTE_COUNT];

        VoxelizationGeometryShaderDesc() : pixelShaderInputCount(0)
        {
        }
    };

    struct VoxelizationPixelShaderDesc
    {
        const char* source;
        size_t sourceSize;
        const char* entryFunc;
        ShaderResources* userShaderCodeResources;
        bool useForOpacity;
        bool useForEmittance;
        bool canUseDefaultOpacityShader;
        bool useCoverageSupersampling;

        VoxelizationPixelShaderDesc()
            : source(NULL)
            , sourceSize(0)
            , entryFunc("main")
            , userShaderCodeResources(NULL)
            , useForOpacity(true)
            , useForEmittance(true)
            , canUseDefaultOpacityShader(false)
            , useCoverageSupersampling(false)
        {
        }
    };

    // This structure describes a user-defined triangle culling function, presented in HLSL source code.
    // Such function is inserted into the VXGI voxelization geometry shader and can discard triangles early in the pipeline,
    // based on vertex positions and/or triangle normal. For example, when voxelizing directly lit geometry,
    // the culling function can test whether the triangle intersects with the light frustum and is facing the light.
    // The function should be defined to match the following prototype: 
    // 
    //    bool CullTriangle(float3 v1, float3 v2, float3 v3, float3 normal) { ... }
    //
    // - v1, v2, v3 are vertex coordinates in the same order as they're received by the GS;
    // - normal is the triangle normal computed as normalize(cross(v1 - v0, v2 - v0));
    // - returns true if the triangle has to be discarded, and false if it has to be voxelized.
    // 
    struct VoxelizationGeometryShaderCullFunctionDesc
    {
        // Pointer to the culling function source code in HLSL, not necessarily ending with a zero
        const char* sourceCode;

        // Size of the source code, in bytes
        size_t sourceCodeSize;

        // Resources such as constant buffers or textures that are used by the culling function.
        // These resources have to be bound by the application after applying the voxelization state
        // that is produced by IGlobalIllumination::getVoxelizationState(...) function.
        ShaderResources resources;
    };

    // A container for binary data
    class IBlob
    {
        IBlob& operator=(const IBlob& other); //undefined
    protected:
        //The user cannot delete this
        virtual ~IBlob() {};
    public:
        virtual const void* getData() const = 0;
        virtual size_t getSize() const = 0;
        //the caller is finished with this object and it can be destroyed
        virtual void dispose() = 0;
    };

    class IUserDefinedShaderSet
    {
        IUserDefinedShaderSet& operator=(const IUserDefinedShaderSet& other); //undefined
    protected:
        virtual ~IUserDefinedShaderSet() {};
    public:
        enum ShaderType
        {
            VOXELIZATION_GEOMETRY_SHADER,
            VOXELIZATION_PIXEL_SHADER,
            VOXELIZATION_SS_PIXEL_SHADER,
            CONE_TRACING_PIXEL_SHADER,
            CONE_TRACING_COMPUTE_SHADER
        };

        virtual ShaderType getType() = 0;

        //There could be multiple versions of this shader inside
        virtual uint32_t getPermutationCount() = 0;
        virtual NVRHI::ShaderHandle getApplicationShaderHandle(uint32_t permutation) = 0;
    };

    struct UpdateVoxelizationParameters
    {
        // Anchor is the point around which the clipmap center is located - it is snapped to a grid.
        // For first-person cameras, the anchor should be placed slightly ahead of the camera, e.g. at (eyePosition + eyeDirection * giRange).
        Vector3f clipmapAnchor;

        // Scene bounding box in world space - used to stop cone tracing when cones exit the scene
        Box3f sceneExtents;

        // Size of the finest clipmap level, in world units
        float giRange;

        // A set of world-space boxes that contain some geometry that changed since the previous frame
        const Box3f* invalidatedRegions;

        // The number of boxes in 'invalidatedRegions'
        uint32_t invalidatedRegionCount;

		// A set of world-space frusta for lights which have been moved or otherwise changed.
		// All emittance data in pages intersecting the frusta will be cleared.
		const Frustum* invalidatedLightFrusta;

		// The number of frusta in 'invalidatedLightFrusta'
		uint32_t invalidatedFrustumCount;

        // Parameters that control the cone tracing process used for the indirect irradiance map.
        // The nearClipZ, farClipZ and debugParameters members of CommonTracingParameters are ignored here, all others are effective.
        IndirectIrradianceMapTracingParameters indirectIrradianceMapTracingParameters;

        UpdateVoxelizationParameters()
            : clipmapAnchor(0.f)
            , sceneExtents(Vector3f(FLT_MIN), Vector3f(FLT_MAX))
            , giRange(512.0f)
            , invalidatedRegions(NULL)
            , invalidatedRegionCount(0)
            , invalidatedLightFrusta(NULL)
            , invalidatedFrustumCount(0)
        { }
    };

    struct DebugRenderParameters
    {
        // Which texture?
        DebugRenderMode::Enum debugMode;

        // Camera paramaters
        Matrix4f viewMatrix;
        Matrix4f projMatrix;

        NVRHI::Viewport viewport;

        // Required
        NVRHI::TextureHandle destinationTexture;

        // Optional - use it to correctly overlay the voxels over the scene rendering
        NVRHI::TextureHandle destinationDepth;

        NVRHI::BlendState blendState;
        NVRHI::DepthStencilState depthStencilState;

        // Opacity that will be written into the .a channel of destinationTexture for covered pixels
        float targetOpacity;

        // Clipmap level to visualize (for opacity and emittance views)
        uint32_t level;

        // Allocation map bit index to visualize (for the allocation map view)
        uint32_t bitToDisplay;

        // Number of voxel faces to look through
        uint32_t voxelsToSkip;

        // Projection parameters
        float nearClipZ;
        float farClipZ;

        DebugRenderParameters()
            : debugMode(DebugRenderMode::DISABLED)
            , destinationTexture(NULL)
            , destinationDepth(NULL)
            , targetOpacity(1.0f)
            , level(0)
            , bitToDisplay(0)
            , voxelsToSkip(0)
            , nearClipZ(0.0f)
            , farClipZ(1.0f)
        { }
    };

    class IShaderCompiler
    {
    public:
        // Functions that create a voxelization geometry shader based on..
        // - A descriptor listing the attributes to pass through the GS
        virtual Status::Enum compileVoxelizationGeometryShader(IBlob** ppBlob, const VoxelizationGeometryShaderDesc& desc, const VoxelizationGeometryShaderCullFunctionDesc* cullFunction = NULL) = 0;
        // - A vertex shader; shader reflection is used to get the list of attributes
        virtual Status::Enum compileVoxelizationGeometryShaderFromVS(IBlob** ppBlob, const void* binary, size_t binarySize, const VoxelizationGeometryShaderCullFunctionDesc* cullFunction = NULL) = 0;
        // - A domain shader; shader reflection is used to get the list of attributes
        virtual Status::Enum compileVoxelizationGeometryShaderFromDS(IBlob** ppBlob, const void* binary, size_t binarySize, const VoxelizationGeometryShaderCullFunctionDesc* cullFunction = NULL) = 0;

        // Creates a default voxelization PS that is only useful for opacity voxelization of opaque geometry.
        // When used for emittance voxelization, this shader produces flat white emissive geometry.
        virtual Status::Enum compileVoxelizationDefaultPixelShader(IBlob** ppBlob) = 0;

        // Creates a custom voxelization PS that can be used for both opacity and emittance voxelization.
        // The source code provided by the user should call the following function:
        //
        //    void VxgiStoreVoxelizationData(VxgiVoxelizationPSInputData inputData, float3 emissiveColor)
        //
        // - inputData should be received as PS input attributes;
        // - emissiveColor can be computed as required
        //
        // Static const int VxgiIsEmissiveVoxelizationPass is defined to help the user code tell whether it's compiled 
        // for opacity voxelization (0) or emittance voxelization (1).
        //
        // VxgiGetIndirectIrradiance(...) function is also available to the user code to get the indirect irradiance,
        // see the comments for VoxelizationParameters::multiBounceMode for more details.
        // 
        // The useForOpacity and useForEmittance parameters control whether this shader will be used for opacity and 
        // emittance voxelization, respectively. When one of this flags is false and the shader is actually used for that kind 
        // of voxelization, calling getVoxelizationState will return a SHADER_MISSING error. 
        // 
        // If canUseDefaultOpacityShader parameter is set to true, VXGI will try to use the default opacity voxelization PS.
        // It will only succeed if the resource bindings that are generated automatically for the emittance voxelization PS 
        // and the default resource bindings match. This means that the user code should claim that it requires only b0 and no other
        // resources in slots lower than 9. If the resource bindings do not match, a new PS will be compiled.
        //
        virtual Status::Enum compileVoxelizationPixelShader(IBlob** ppBlob, const VoxelizationPixelShaderDesc& desc) = 0;

        // Functions that create user defined shaders that use VXGI cone tracing functions.
        // Cone tracing is defined this way:
        // 
        //    struct VxgiConeTracingArguments 
        //    {
        //      float3 firstSamplePosition;       // world space position of the first cone sample
        //      float3 direction;                 // direction of the cone
        //      float coneFactor;                 // sampleSize = t * coneFactor
        //      float tracingStep;                // see comment for CommonTracingParameters::tracingStep
        //      float firstSampleT;               // ray parameter (t) value for the first sample, in voxels
        //      float maxTracingDistance;         // maximum distance to trace from surface, in world units, default is 0.0 = no limit
        //      float opacityCorrectionFactor;    // see comment for CommonTracingParameters::opacityCorrectionFactor
        //      float emittanceScale;             // multiplier for the irradiance result, default is 1.0
        //      float initialOpacity;             // opacity accumulated elsewhere before starting the cone, default is 0.0
        //      float ambientAttenuationFactor;   // ambient contribution = exp(-t * ambientAttenuationFactor)
        //      float maxSamples;                 // limits the number of samples to take
        //      float randomSeed;                 // a pseudo random number, at least filling the range [0..1], used for tangent jitter
        //      float tangentJitterScale;         // see comment for SpecularTracingParameters::tangentJitterScale
        //      bool enableSceneBoundsCheck;      // should the cone be terminated when it leaves the scene bounding box
        //      bool flipOpacityDirections;       // see comment for TracingParameters::flipOpacityDirections
        //    };
        //  
        //    struct VxgiConeTracingResults
        //    {
        //      float3 irradiance;                // irradiance from the cone
        //      float ambient;                    // amount of ambient lighting from the cone, normalized to [0, 1]
        //      float finalOpacity;               // opacity accumulated by the cone
        //      float sampleCount;                // number of samples taken for the cone
        //    };
        //
        //    VxgiConeTracingResults VxgiTraceCone(VxgiConeTracingArguments args);
        //
        // Use this function to initialize all VxgiConeTracingArguments fields with default values:
        //
        //    VxgiConeTracingArguments VxgiDefaultConeTracingArguments();
        //
        virtual Status::Enum compileConeTracingPixelShader(IBlob** ppBlob, const char* source, size_t sourceSize, const char* entryFunc, const ShaderResources& userShaderCodeResources) = 0;
        virtual Status::Enum compileConeTracingComputeShader(IBlob** ppBlob, const char* source, size_t sourceSize, const char* entryFunc, const ShaderResources& userShaderCodeResources) = 0;

        // Tests whether the user defined shader binary is compatible with the current version of VXGI library.
        // If it isn't, you should recompile the shader and generate a new binary.
        virtual bool isValidUserDefinedShaderBinary(const void* binary, size_t binarySize) = 0;

        // Extracts the shader type descriptor from the binary.
        virtual IUserDefinedShaderSet::ShaderType getUserDefinedShaderBinaryType(const void* binary, size_t binarySize) = 0;

        // Gets the number of specific shader permutations stored in a binary.
        virtual uint32_t getUserDefinedShaderBinaryPermutationCount(const void* binary, size_t binarySize) = 0;

        // You must free these blobs
        virtual IBlob* getUserDefinedShaderBinaryReflectionData(const void* binary, size_t binarySize, uint32_t permutation) = 0;
        // This may do nothing if the API doesn't support this
        virtual IBlob* stripUserDefinedShaderBinary(const void* binary, size_t binarySize) = 0;
    };

    struct ShaderCompilerParameters
    {
        NVRHI::IErrorCallback* errorCallback;
        IAllocator* allocator;

        // you can use this to override the DLL VXGI will use to match your appliation
        const char* d3dCompilerDLLName;
        bool multicoreShaderCompilation;

        NVRHI::GraphicsAPI::Enum graphicsAPI;

		// Flags passed to the D3DCompile function
		uint32_t d3dCompileFlags;
		uint32_t d3dCompileFlags2;

        ShaderCompilerParameters()
            : errorCallback(nullptr)
            , allocator(nullptr)
            , d3dCompilerDLLName(nullptr)
            , multicoreShaderCompilation(true)
            , graphicsAPI(NVRHI::GraphicsAPI::D3D11)
			, d3dCompileFlags(0x9002) // D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION | D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY
			, d3dCompileFlags2(0)
        { }
    };

    // The primary interface for interaction with VXGI
    // An instance of object implementing this interface can be created with VFX_VXGI_CreateGIObject
    // and destroyed with VFX_VXGI_DestroyGIObject
    class IGlobalIllumination
    {
        IGlobalIllumination& operator=(const IGlobalIllumination& other); //undefined
    protected:
        virtual ~IGlobalIllumination() {};
    public:

        // Returns the hash of the VXGI shaders that compose the IUserDefinedShaderSets. If this does not match the application must compile new voxelization shaders
        // or else loadUserDefinedShaderSet will reject them
        virtual uint64_t getInternalShaderHash() = 0;

        // Creates a view tracer and allocated its resources
        // The device reference is assumed to stay valid until 
        // the next call to ReleaseResources().
        virtual Status::Enum createNewTracer(IViewTracer** ppTracer) = 0;

        // Releases all the previously created resources for a specific tracer
        // and forgets the previously supplied device reference.
        virtual void destroyTracer(IViewTracer* pTracer) = 0;

        // Get the current renderer interface
        virtual NVRHI::IRendererInterface* getRendererInterface() = 0;

        // Gets the performance monitor
        virtual IPerformanceMonitor* getPerformanceMonitor() = 0;

        // This can be used to find out what getWorldRegion will be after a future call to updateGlobalIllumination
        virtual Box3f calculateHypotheticalWorldRegion(
            const Vector3f& clipmapAnchor,
            float giRange) = 0;

        // Sets or updates the voxelization parameters, such as clipmap resolution or storage formats.
        // All data in the existing clipmap will be lost.
        // If this function fails, the GI object will be in an uninitialized state, and all subsequent rendering calls 
        // will fail and return an INVALID_STATE error.
        virtual Status::Enum setVoxelizationParameters(const VoxelizationParameters& parameters) = 0;

        // Validates the voxelization parameters without affecting the active settings or re-allocating anything.
        // Returns Status::OK for acceptable sets of parameters, signals and returns an error for unacceptable ones.
        virtual Status::Enum validateVoxelizationParameters(const VoxelizationParameters& parameters) = 0;

		// This function is to calculate voxelization view matrix in advance of calling prepareForOpacityVoxelization().
		// Some game engines use different threads for drawcalling and rendering, so repareing rendering parameters in advance of actual graphics API calls is needed.
		// This function will not change anything in this class members, and will return a viewMatrix which will be the same as the one we will get by calling getVoxelizationViewMatrix() after the prepareForOpacityVoxelization() call.
		virtual Status::Enum prepareVoxelizationViewMatrix(const Vector3f &clipmapAnchor, float giRange, Matrix4f& viewMatrix) const = 0;

        // This function performs all steps necessary to begin voxelization for a new frame. It should only be called once per frame.
        // The performOpacityVoxelization and performEmittanceVoxelization reference parameters are returned from this function,
        // indicating whether the application is allowed to perform opacity or emittance voxelization draw calls on this frame, respectively.
        // If the app tries to call getVoxelizationState when the respective perform... value is false, an INVALID_STATE error will be returned.
        virtual Status::Enum prepareForOpacityVoxelization(
            const UpdateVoxelizationParameters& params,
            bool& performOpacityVoxelization,
            bool& performEmittanceVoxelization) = 0;

        // This function performs some steps necessary to move from opacity voxelization to emittance voxelization.
        virtual Status::Enum prepareForEmittanceVoxelization() = 0;

        // Marks the beginning of a group of independent draw calls used for voxelization, useful for performance.
        // If this function is not called, a barrier will be inserted by the D3D runtime between these draw calls because they use a UAV.
        // Inside, this methods enqueues a call to NvAPI_D3D11_BeginUAVOverlap, which removes the barrier.
        virtual Status::Enum beginVoxelizationDrawCallGroup() = 0;

        // Marks the end of a group of independent draw calls used for voxelization.
        // Inside, this methods enqueues a call to NvAPI_D3D11_EndUAVOverlap.
        virtual Status::Enum endVoxelizationDrawCallGroup() = 0;

        // Returns the list of world-space regions that have to be revoxelized on this frame.
        // The list is based on the invalidatedRegions list passed to prepareForVoxelization(...), but is more complete and optimized.
        // Specifically, this list contains extra regions that are created from camera movements,
        // and all regions in the list are snapped to the allocation map page grid.
        // If the buffer passed as pRegions is too small, a BUFFER_TOO_SMALL error is returned, and numRegions contains a valid number. 
        // Call getInvalidatedRegions(0, 0, numRegions) to get the number of regions and allocate the buffer based on that.
        virtual Status::Enum getInvalidatedRegions(Box3f* pRegions, uint32_t maxRegions, uint32_t& numRegions) = 0;

        // Returns the minimum voxel size at a given world position using the last updated clipmap range and anchor. 
        // EXACT function returns the exact voxel size for each point, but it has discontinuities at level boundaries.
        // LINEAR_UNDERESTIMATE function returns a linear ramp that is less than or equal to the exact voxel size for each point.
        // LINEAR_OVERESTIMATE function returns a linear ramp that is greater than or equal to the exact voxel size for each point.
        // When zeroOutOfRange == false, values of this function outside of the clipmap equal to the values at the closest boundary.
        // When zeroOutOfRange == true, the function is zero outside of the clipmap.
        virtual float getMinVoxelSizeAtPoint(Vector3f position, VoxelSizeFunction::Enum function, bool zeroOutOfRange = false) = 0;

        // Returns the view matrix that has to be used for voxelization draw calls.
        // The projection matrix is always identity.
        virtual Status::Enum getVoxelizationViewMatrix(Matrix4f& viewMatrix) = 0;

        // Computes the state necessary to perform opacity voxelization or emittance voxelization for a given material.
        // If this function is called after prepareForOpacityVoxelization(...), a state for opacity voxelization is returned;
        // if it is called after prepareForEmittanceVoxelization(), a state for emittance voxelization is returned,
        // and otherwise an INVALID_STATE error is returned.
        // In order to voxelize geometry, set the returned state and don't forget to call preDraw/postDraw functions of the state.
        // The state object passed into this function will be completely overwritten.
        virtual Status::Enum getVoxelizationState(const MaterialInfo& materialInfo, NVRHI::DrawCallState& state) = 0;

        // This is a lightweight version of getVoxelizationState that only changes constant buffer contents.
        // It's safe to call this function if MaterialInfo::requiresParameterUpdate returns true, but MaterialInfo::requiresNewState returns false.
        virtual Status::Enum updateVoxelizationMaterialParameters(const MaterialInfo& materialInfo) = 0;

        // Finalizes all voxel representation updates and prepares for cone tracing.
        virtual Status::Enum finalizeVoxelization() = 0;

        // Renders a visualization of one of the voxel textures.
        virtual Status::Enum renderDebug(const DebugRenderParameters& params) = 0;

        // These functions bind the VXGI related resources to the user defined tracing shader.
        // You can add any other resources that your shader needs, but they have to be declared as ShaderResources 
        // in the corresponding createConeTracing{Pixel,Compute}Shader(...) call.
        // After the state is set, you can do any number of draw calls or dispatches with that state, 
        // which stays valid until the next call to updateGlobalIllumination.
        virtual Status::Enum setupUserDefinedConeTracingPixelShaderState(IUserDefinedShaderSet* shaderSet, NVRHI::DrawCallState& state) = 0;
        virtual Status::Enum setupUserDefinedConeTracingComputeShaderState(IUserDefinedShaderSet* shaderSet, NVRHI::DispatchState& state) = 0;

        // These functions return the clipmap parameters computed on the last successful call to prepareForOpacityVoxelization.
        virtual const Box3f& getLastUpdatedWorldRegion() = 0;
        virtual const Box3f& getLastUpdatedSceneExtents() = 0;
        virtual const Vector3f& getLastUpdatedClipmapAnchor() = 0;

        // Loads the previously compiled user defined shader from a binary representation.
        // The binary can be obtained from IShaderCompiler::compileXxxShader(...) methods and stored somewhere to speed up application startup.
        virtual Status::Enum loadUserDefinedShaderSet(IUserDefinedShaderSet** ppShaderSet, const void* binary, size_t binarySize, bool reportNoErrorsOnInvalidBinaryFormat = false) = 0;

        // Destroys a user defined shader set previously created with one of the above functions.
        virtual void destroyUserDefinedShaderSet(IUserDefinedShaderSet* shader) = 0;

        // Renders something simple (currently a cube) into the voxel textures.
        // Call this function after prepareForOpacityVoxelization and/or prepareForEmittanceVoxelization to test 
        // the rendering backend implementation separately from your scene voxelization code.
        virtual Status::Enum voxelizeTestScene(Vector3f testObjectPosition, float testObjectSize, IShaderCompiler* compiler) = 0;

        // Returns true if Maxwell [*] extended features are actually being used.
        virtual bool areNvidiaExtensionsUsed() = 0;

        // Sets up the extra state required by VXGI for fast voxelization on Maxwell GPUs.
        // This method should be called from the rendering backend when applying state if RenderState::setupExtraVoxelizationState == true.
        virtual Status::Enum setupExtraVoxelizationState() = 0;

        // Removes the state set by a prior call to setupExtraVoxelizationStateOpenGL().
        virtual Status::Enum removeExtraVoxelizationState() = 0;
    };

    struct GIParameters
    {
        NVRHI::IRendererInterface* rendererInterface;
        NVRHI::IErrorCallback* errorCallback;
        IPerformanceMonitor* perfMonitor;
        IAllocator* allocator;

        GIParameters()
            : rendererInterface(nullptr)
            , perfMonitor(nullptr)
            , allocator(nullptr)
            , errorCallback(nullptr)
        { }
    };

#if VXGI_DYNAMIC_LOAD_LIBRARY
#define VXGI_EXPORT_FUNCTION(rtype, name, ...) typedef rtype (*PFN_##name)(__VA_ARGS__); extern PFN_##name name
#define VXGI_VERSION_ARGUMENT Version version
#elif CONFIGURATION_TYPE_DLL
#define VXGI_EXPORT_FUNCTION(rtype, name, ...) __declspec( dllexport ) rtype name(__VA_ARGS__)
#define VXGI_VERSION_ARGUMENT Version version
#else
#define VXGI_EXPORT_FUNCTION(rtype, name, ...) __declspec( dllimport ) rtype name(__VA_ARGS__)
#define VXGI_VERSION_ARGUMENT Version version = Version()
#endif

#if !VXGI_DYNAMIC_LOAD_LIBRARY
    extern "C"
    {
#endif
        // Creates a root interface object for VXGI. 
        // The interfaceVersion parameter is there to make sure that the VXGI DLL is built with the same version of the headers as the user code.
        // If the interface versions do not match, this method will return WRONG_INTERFACE_VERSION.
        VXGI_EXPORT_FUNCTION(Status::Enum, VFX_VXGI_CreateGIObject, const GIParameters& params, IGlobalIllumination** ppGI, VXGI_VERSION_ARGUMENT);

        // Destroys a previously created instance of the VXGI interface object.
        VXGI_EXPORT_FUNCTION(void, VFX_VXGI_DestroyGIObject, IGlobalIllumination* gi);

        // Creates a shader compiler object for VXGI.
        VXGI_EXPORT_FUNCTION(Status::Enum, VFX_VXGI_CreateShaderCompiler, const ShaderCompilerParameters& params, IShaderCompiler** ppCompiler, VXGI_VERSION_ARGUMENT);

        // Destroys a previously created instance of the VXGI interface object.
        VXGI_EXPORT_FUNCTION(void, VFX_VXGI_DestroyShaderCompiler, IShaderCompiler* compiler);

        // Compares the version of the header that the user code was built with and the version that VXGI was built with.
        // Returns OK if the versions match, and WRONG_INTERFACE_VERSION otherwise.
        VXGI_EXPORT_FUNCTION(Status::Enum, VFX_VXGI_VerifyInterfaceVersion, VXGI_VERSION_ARGUMENT);

        // This method returns a hash of VXGI shader fragments that are linked together with user-defined voxelization or cone tracing shaders.
        // User code may store the compiled shaders in binary format and re-use them as long as this shader hash stays the same.
        // The shader hash is also verified in IGlobalIllumination::loadUserDefinedShaderSet, so it is not necessary to store it separately.
        VXGI_EXPORT_FUNCTION(uint64_t, VFX_VXGI_GetInternalShaderHash, VXGI_VERSION_ARGUMENT);

        // Converts a status code to a string containing the name of that status code, e.g. "NULL_ARGUMENT".
        VXGI_EXPORT_FUNCTION(const char*, VFX_VXGI_StatusToString, Status::Enum status);

#if VXGI_DYNAMIC_LOAD_LIBRARY
        Status::Enum GetProcAddresses(HMODULE hDLL);
#else
    }
#endif
    
} // VXGI
GI_END_PACKING

#endif // GFSDK_VXGI_H_
