// This code contains NVIDIA Confidential Information and is disclosed 
// under the Mutual Non-Disclosure Agreement. 
// 
// Notice 
// ALL NVIDIA DESIGN SPECIFICATIONS AND CODE ("MATERIALS") ARE PROVIDED "AS IS" NVIDIA MAKES 
// NO REPRESENTATIONS, WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO 
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ANY IMPLIED WARRANTIES OF NONINFRINGEMENT, 
// MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 
// 
// NVIDIA Corporation assumes no responsibility for the consequences of use of such 
// information or for any infringement of patents or other rights of third parties that may 
// result from its use. No license is granted by implication or otherwise under any patent 
// or patent rights of NVIDIA Corporation. No third party distribution is allowed unless 
// expressly authorized by NVIDIA.  Details are subject to change without notice. 
// This code supersedes and replaces all information previously supplied. 
// NVIDIA Corporation products are not authorized for use as critical 
// components in life support devices or systems without express written approval of 
// NVIDIA Corporation. 
// 
// Copyright (c) 2003 - 2016 NVIDIA Corporation. All rights reserved.
//
// NVIDIA Corporation and its licensors retain all intellectual property and proprietary
// rights in and to this software and related documentation and any modifications thereto.
// Any use, reproduction, disclosure or distribution of this software and related
// documentation without an express license agreement from NVIDIA Corporation is
// strictly prohibited.
//

 
/*==============================================================================
	NvVolumetricLighting.h
================================================================================

NVIDIA Volumetric Lighting
-----------------------------------------
Gameworks Volumetric Lighting provides dynamic, physically-based light
scattering based on application-provided media properties and existing
shadowing information.

ENGINEERING CONTACT
Nathan Hoobler (NVIDIA Devtech)
nhoobler@nvidia.com

==============================================================================*/
#ifndef NVVOLUMETRICLIGHTING_H
#define NVVOLUMETRICLIGHTING_H
////////////////////////////////////////////////////////////////////////////////

/*==============================================================================
NvFoundation Includes
==============================================================================*/

#include <Nv.h>
#include <NvPreprocessor.h>
#include <NvCTypes.h>

/*==============================================================================
Forward Declarations
==============================================================================*/

#if defined(NV_PLATFORM_D3D11)
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11ShaderResourceView;
#endif

// [Add other platforms here]

////////////////////////////////////////////////////////////////////////////////
#if (!NV_DOXYGEN)
namespace Nv { 
using namespace nvidia;
namespace VolumetricLighting { 
#endif
////////////////////////////////////////////////////////////////////////////////

/*==============================================================================
	Library Version
==============================================================================*/

//! Describes the library version
struct VersionDesc
{ 
	uint32_t Major;		//!< Major version of the product, changed manually with every product release with a large new feature set. API refactoring. Breaks backwards compatibility 
	uint32_t Minor;		//!< Minor version of the product, changed manually with every minor product release containing some features. Minor API changes
	uint32_t Build;		//!< Very minor version of the product, mostly for bug fixing. No API changes, serialization compatible. 
	uint32_t Revision;	//!< Latest Perforce revision of the codebase used for this build. 
}; 

//! Current library version number
const VersionDesc LIBRARY_VERSION={1,0,0,0/*VERSIONCHANGELISTREPLACETOKEN*/};

/*==============================================================================
	Platform-dependent wrappers
==============================================================================*/

//! Rendering context information for platform
union PlatformRenderCtx
{
#if defined(NV_PLATFORM_D3D11)
	ID3D11DeviceContext* d3d11;
	PlatformRenderCtx(ID3D11DeviceContext* x) : d3d11(x){};
    operator ID3D11DeviceContext*() { return d3d11;  }
#endif
	// [Add other platforms here]
};

//! Render target information for platform
union PlatformRenderTarget
{
#if defined(NV_PLATFORM_D3D11)
	ID3D11RenderTargetView* d3d11;
    PlatformRenderTarget(ID3D11RenderTargetView* x) : d3d11(x){};
    operator ID3D11RenderTargetView*() { return d3d11; }
#endif
	// [Add other platforms here]
};

//! Depth-Stencil target information for platform
union PlatformDepthStencilTarget
{
#if defined(NV_PLATFORM_D3D11)
	ID3D11DepthStencilView* d3d11;
    PlatformDepthStencilTarget(ID3D11DepthStencilView* x) : d3d11(x){};
    operator ID3D11DepthStencilView*() { return d3d11; }
#endif
	// [Add other platforms here]
};

//! Surface shader resource information for platform
union PlatformShaderResource
{
#if defined(NV_PLATFORM_D3D11)
	ID3D11ShaderResourceView* d3d11;
    PlatformShaderResource(ID3D11ShaderResourceView* x) : d3d11(x){};
    operator ID3D11ShaderResourceView*() { return d3d11; }
#endif
	// [Add other platforms here]
};

/*==============================================================================
	API Types and Enums
==============================================================================*/

//! Context used for an instance of the library used for rendering
typedef void * Context;

//! Return codes returned by library API functions
enum class Status
{
    OK					=  0,	//!< Success
    FAIL				= -1,   //!< Unspecified Failure
	INVALID_VERSION		= -2,	//!< Mismatch between header and dll
    UNINITIALIZED       = -3,   //!< API call made before the library has been properly initialized
	UNIMPLEMENTED		= -4,	//!< Call not implemented for platform
	INVALID_PARAMETER	= -5,	//!< One or more invalid parameters
    UNSUPPORTED_DEVICE 	= -6,	//!< Device doesn't support necessary features
    RESOURCE_FAILURE	= -7,	//!< Failed to allocate a resource
    API_ERROR	        = -8,	//!< The platform API returned an error to the library
};

//! Platform/API IDs
enum class PlatformName
{
	UNKNOWN = -1,
	#if defined(NV_PLATFORM_D3D11)
		D3D11,	//!< Direct3D 11
	#endif
	// [Add other platforms here]
    COUNT
};

//! Debug mode constants (bit flags)
enum class DebugFlags
{
	NONE 		= 0x00000000,	//!< No debug visualizations
	WIREFRAME 	= 0x00000001,	//!< Render volume as wireframe
	NO_BLENDING	= 0x00000002	//!< Don't blend scene into output
};

//! Specifies the godrays buffer resolution relative to framebuffer
enum class DownsampleMode
{
	UNKNOWN = -1,
	FULL,		//!< Same resolution as framebuffer
	HALF,		//!< Half dimensions of framebuffer (1x downsample)
	QUARTER,    //!< Quarter dimensions of framebuffer (2x downsample)
	COUNT
};

//! Specifies the godrays buffer sample rate
enum class MultisampleMode
{
	UNKNOWN = -1,
	SINGLE, //!< Single-sample
	MSAA2,	//!< 2x MSAA
	MSAA4,	//!< 4x MSAA
	COUNT
};

enum class FilterMode
{
	UNKNOWN = -1,
	NONE,		//!< No post-processing filter
	TEMPORAL,	//!< Temporal AA on post-process output
};

//! Phase function to use for this media term
enum class PhaseFunctionType
{
	UNKNOWN = -1,
	ISOTROPIC,	        //!< Isotropic scattering (equivalent to HG with 0 eccentricity, but more efficient)
	RAYLEIGH,	        //!< Rayleigh scattering term (air/small molecules)
	HENYEYGREENSTEIN,	//!< Scattering term with variable anisotropy
    MIE_HAZY,	        //!< Slightly forward-scattering
    MIE_MURKY,	        //!< Densely forward-scattering
	COUNT
};

//! Specifies the geometric mapping of the shadow map
enum class ShadowMapLayout
{
	UNKNOWN = -1,
	SIMPLE,			//!< Simple frustum depth texture
	CASCADE_ATLAS,	//!< Multiple depth views combined into one texture
	CASCADE_ARRAY,	//!< Multiple depth views as texture array slices
	CASCADE_MULTI,	//!< Multiple depth views in the multiple textures
    PARABOLOID,     //!< Depth mapped using paraboloid warping
	CUBE,			//!< Depth mapped using cube
	COUNT
};

//! Specifies the encoding of shadow map samples
enum class ShadowMapFormat
{
    UNKNOWN = -1,
    DEPTH,      //!< Simple depth-buffer output
    COUNT
};

//! Specifies the class of light source
enum class LightType
{
	UNKNOWN = -1,
	DIRECTIONAL,    //!< Simple analytic directional light (like the sun)
	SPOTLIGHT,      //!< Spotlight with frustum shadow map and angular falloff
	OMNI,			//!< Omni-directional local light source
	COUNT
};

//! Specifies the type of distance attenuation applied to the light
enum class AttenuationMode
{
	UNKNOWN = -1,
    NONE,           //!< No attenuation
	POLYNOMIAL,	    //!< f(x) = 1-(A+Bx+Cx^2)
    INV_POLYNOMIAL, //!< f(x) = 1/(A+Bx+Cx^2)+D
	COUNT
};

//! Specifies the type of angular falloff to apply to the spotlight
enum class SpotlightFalloffMode
{
    UNKNOWN = -1,
    NONE,   //!< No falloff (constant brightness across cone cross-section)
    FIXED,	//!< A_fixed(vL, vP) = (dot(vL, vP) - theta_max)/(1 - theta_max)
    CUSTOM, //!< A_custom(vL, vP) = (A_fixed(vL, vP))^n
    COUNT
};

//! Amount of tessellation to use
enum class TessellationQuality
{
    UNKNOWN = -1,
    LOW,        //!< Low amount of tessellation (16x)
    MEDIUM,     //!< Medium amount of tessellation (32x)
    HIGH,       //!< High amount of tessellation (64x)
    COUNT
};

//! Quality of upsampling
enum class UpsampleQuality
{
	UNKNOWN = -1,
	POINT,		//!< Point sampling (no filter)
	BILINEAR,	//!< Bilinear Filtering
	BILATERAL,	//!< Bilateral Filtering (using depth)
	COUNT
};

enum class HMDDeviceType
{
	UNKNOWN = -1,
	OCULUSRIFT,		//!< Oculus Rift
	STEAMVR,		//!< HTC Vive
	COUNT
};

enum class VRProjectConfiguration
{
	UNKNOWN = -1,
	NONE,
	CONSERVATIVE,
	BALANCED,
	AGGRESSIVE,
    COUNT
};

enum class StereoscopicPass
{
	UNKNOWN = -1,
	FULL,			//!< Apply the full screen in mono or both Left and Right eyes screen in stereo
	LEFTEYE,		//!< The screen from Left-eye was applied in stereo
	RIGHTEYE,		//!< The screen from Right-eye was applied in stereo
};

//! Platform-specific parameters
struct PlatformDesc
{
	PlatformName platform;	//!< Platform identifier
	union
	{
	#if defined(NV_PLATFORM_D3D11)
		struct
		{
			ID3D11Device *pDevice;	//!< D3D11 Device to use for context
		} d3d11;
	#endif
	// [Add other platforms here]
	};
};

//! Context description
struct ContextDesc
{
	struct
	{
		uint32_t uWidth;					//!< Width of output/depth surface
		uint32_t uHeight;					//!< Height of output/depth surface
		uint32_t uSamples;					//!< Sample rate of output/depth surface
	} framebuffer;
	DownsampleMode eDownsampleMode;			//!< Target resolution of internal buffer
	MultisampleMode eInternalSampleMode;	//!< Target sample rate of internal buffer
	FilterMode eFilterMode;					//!< Type of filtering to do on the output
	bool bStereoEnabled;					//!< Stereo rendering
	bool bSinglePassStereo;					//!< Enable Single Pass Stereo
	bool bReversedZ;						//!< Reversed z projection transform for view frustum (0 far, 1 near)
	HMDDeviceType	eHMDDevice;						//!< HMD device type
	VRProjectConfiguration eLensMatchedConfig;		//!< LMS configuration
	VRProjectConfiguration eMultiResConfig;			//!< MRS configuration
};

//! Viewer Camera/Framebuffer Description
struct ViewerDesc 
{
	NvcMat44 mProj;		        //!< Camera projection transform
	NvcMat44 mViewProj;	        //!< Camera view-proj transform
	float fZNear;				//!< World-space distance to camera near view plane
	float fZFar;				//!< World-space distance to camera far view plane
	NvcVec3 vEyePosition;	    //!< Camera position in world-space
	uint32_t uViewportTopLeftX;	//!< Viewport Top left X position
	uint32_t uViewportTopLeftY;	//!< Viewport Top left Y position
	uint32_t uViewportWidth;	//!< Viewport Width (may differ from framebuffer)
	uint32_t uViewportHeight;	//!< Viewport Height (may differ from framebuffer)
	uint32_t uNonVRProjectViewportWidth;		//!< Viewport Width (may differ from framebuffer) without vr projection scaling
	uint32_t uNonVRProjectViewportHeight;		//!< Viewport Height (may differ from framebuffer) without vr projection scaling
};

//! Describes one component of the phase function
struct PhaseTerm
{
	PhaseFunctionType ePhaseFunc;	//!< Phase function this term uses
	NvcVec3 vDensity;			    //!< Optical density in [R,G,B]
	float fEccentricity;		    //!< Degree/direction of anisotropy (-1, 1) (HG only)
};

//! Maximum number of phase terms in a medium
const uint32_t MAX_PHASE_TERMS = 4;

//! Volume Medium Description
struct MediumDesc
{
    NvcVec3 vAbsorption;		//!< Absorpsive component of the medium
	uint32_t uNumPhaseTerms;    //!< Number of valid phase terms

    //! Phase term definitions
 	PhaseTerm PhaseTerms[MAX_PHASE_TERMS];
	
};

//! Describes an individual slice in a shadow map cascade
struct ShadowMapElementDesc
{
    NvcMat44 mViewProj;		//!< View-Proj transform for cascade
	uint32_t uOffsetX;		//!< X-offset within texture
	uint32_t uOffsetY;		//!< Y-offset within texture
	uint32_t uWidth;		//!< Footprint width within texture
	uint32_t uHeight;		//!< Footprint height within texture
    uint32_t mArrayIndex;   //!< Texture array index for this element (if used)
	float fInvMaxSubjectDepth; //!< The inverse of the max depth (only with Linearized Depth)
	NvcVec4 vShadowmapMinMaxValue; //!< Minimum(xy) and maximum(zw) uv of the shadow map (only with Shadow Space)
};

//! Maximum number of sub-elements in a shadow map set
const uint32_t MAX_SHADOWMAP_ELEMENTS = 4;

//! Shadow Map Structural Description
struct ShadowMapDesc
{
	ShadowMapLayout eType;	//!< Shadow map structure type
	uint32_t uWidth; 		//!< Shadow map texture width
	uint32_t uHeight;		//!< Shadow map texture height
	uint32_t uElementCount; //!< Number of sub-elements in the shadow map
	bool bLinearizedDepth;	//!< Linearized Depth for shadow map
	bool bShadowSpace;		//!< Transform a world space position into the shadow space or clip space 
	NvcMat44 mCubeViewProj[6]; //!< View-Proj transform for 6 faces of cube

    //! Individual cascade descriptions
	ShadowMapElementDesc Elements[MAX_SHADOWMAP_ELEMENTS];
};

//! Light Source Description
struct LightDesc 
{
	LightType eType;		//!< Type of light source
	NvcMat44 mLightToWorld;	//!< Light clip-space to world-space transform
	NvcVec3 vIntensity;		//!< Color of light
	union
	{
		//! LightType = Directional
		struct {
			NvcVec3 vDirection;			        //!< Normalized light direction
		} Directional;

		//! LightType = Spotlight
		struct {
			NvcVec3 vDirection;			        //!< Normalized light direction
			NvcVec3 vPosition;				    //!< Light position in world-space
            float fZNear;                       //!< World-space distance to near view plane
            float fZFar;                        //!< World-space distance to far view plane
            SpotlightFalloffMode eFalloffMode;  //!< Equation to use for angular falloff
			float fFalloff_CosTheta;		    //!< Spotlight falloff angle
			float fFalloff_Power;			    //!< Spotlight power
			AttenuationMode eAttenuationMode;	//!< Light falloff equation
			float fAttenuationFactors[4];	    //!< Factors in the attenuation equation
		} Spotlight;

		//! LightType = Omni
		struct {
			NvcVec3 vPosition;				    //!< Light position in world-space
            float fZNear;                       //!< World-space distance to near view plane
            float fZFar;                        //!< World-space distance to far view plane
			AttenuationMode eAttenuationMode;	//!< Light falloff equation
			float fAttenuationFactors[4];	    //!< Factors in the attenuation equation
		} Omni;
	};
};

//! Parameters for Volume Generation
struct VolumeDesc
{
	float fTargetRayResolution;         //!< Target minimum ray width in pixels
	uint32_t uMaxMeshResolution;        //!< Maximum geometric resolution of the mesh. Accounts for requested tessellation quality.
	float fDepthBias;			        //!< Amount to bias ray geometry depth
    TessellationQuality eTessQuality;   //!< Quality level of tessellation to use
};

//! Post-Processing Behavior Description
struct PostprocessDesc
{
	NvcMat44 mUnjitteredViewProj;		//!< Camera view projection without jitter
    float fTemporalFactor;				//!< Weight of pixel history smoothing (0.0 for off)
    float fFilterThreshold;				//!< Threshold of frame movement to use temporal history
	UpsampleQuality eUpsampleQuality;	//!< Quality of upsampling to use
    NvcVec3 vFogLight;                  //!< Light to use as "faked" multiscattering
    float fMultiscatter;                //<! strength of faked multiscatter effect
    bool bDoFog;						//!< Apply fogging based on scattering
    bool bIgnoreSkyFog;				    //!< Ignore depth values of (1.0f) for fogging
    float  fBlendfactor;				//!< Blend factor to use for compositing
	StereoscopicPass	eStereoPass;	//!< Apply postprocess on the full/left/right screen.
};

/*==============================================================================
	API Definition
==============================================================================*/

#define NV_VOLUMETRICLIGHTING_API(r) NV_DLL_EXPORT r NV_CALL_CONV

//! Load the library and initialize global state
NV_VOLUMETRICLIGHTING_API(Status) OpenLibrary(
    NvAllocatorCallback * allocator = nullptr,          //!< (opt) Memory management handler for library
    NvAssertHandler * assert_handler = nullptr,			//!< (opt) Assertion handler for library
    const VersionDesc & link_version = LIBRARY_VERSION	//!< Requested library version (do not set)
    );

//! Release the library and resources, and uninitialize all global state
NV_VOLUMETRICLIGHTING_API(Status) CloseLibrary();

//! Create a new rendering interface
NV_VOLUMETRICLIGHTING_API(Status) CreateContext(
	Context & out_ctx,	                    //!< Pointer to contain newly created context
	const PlatformDesc * pPlatformDesc,     //!< Platform-specific data
	const ContextDesc * pContextDesc        //!< Context description
    );

//! Release the context and any associated resources
NV_VOLUMETRICLIGHTING_API(Status) ReleaseContext(
    Context & ctx                           //!< Library context to release
    );

//! Begin accumulation of lighting volumes for a view
NV_VOLUMETRICLIGHTING_API(Status) BeginAccumulation(
    Context ctx,                                //!< Library context to operate on
    PlatformRenderCtx renderCtx,		        //!< Context to use for rendering
    PlatformShaderResource sceneDepth,	        //!< Scene Depth-Buffer
    ViewerDesc const* pViewerDesc[],		    //!< Description of camera space
    MediumDesc const* pMediumDesc,		        //!< Description of medium
    DebugFlags debugFlags = DebugFlags::NONE    //!< Debug flags to apply for this pass
    );

//! Add a lighting volume to the accumulated results
NV_VOLUMETRICLIGHTING_API(Status) RenderVolume(
    Context ctx,                            //!< Library context to operate on
    PlatformRenderCtx renderCtx,            //!< Context to use for rendering
    PlatformShaderResource shadowMap[],       //!< Shadow map resource for each cascade, size is uElementCount
    ShadowMapDesc const* pShadowMapDesc,    //!< Shadow map layout description
    LightDesc const* pLightDesc,            //!< Light source description
    VolumeDesc const* pVolumeDesc           //!< Parameters for volume generation
    );

//! Finish accumulation of lighting volumes
NV_VOLUMETRICLIGHTING_API(Status) EndAccumulation(
    Context ctx,                            //!< Library context to operate on
    PlatformRenderCtx renderCtx             //!< Context to use for rendering
    );

//! Resolve the results and composite into the provided scene
NV_VOLUMETRICLIGHTING_API(Status) ApplyLighting(
    Context ctx,                            //!< Library context to operate on
    PlatformRenderCtx renderCtx,	        //!< Context to use for rendering
    PlatformRenderTarget sceneTarget,	    //!< Render target to composite into
    PlatformShaderResource sceneDepth,	    //!< Depth buffer for scene
    PostprocessDesc const* pPostprocessDesc //!< Options for how to perform the resolve and composite
    );

////////////////////////////////////////////////////////////////////////////////
#if (!NV_DOXYGEN)
}; /*namespace VolumetricLighting*/ 
namespace Vl = VolumetricLighting;
}; /*namespace Nv*/
namespace NvVl = Nv::VolumetricLighting;
#endif
////////////////////////////////////////////////////////////////////////////////
#endif // NVVOLUMETRICLIGHTING_H
