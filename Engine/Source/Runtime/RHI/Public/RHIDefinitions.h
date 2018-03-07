// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHIDefinitions.h: Render Hardware Interface definitions
		(that don't require linking).
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "PixelFormat.h"
#include "HAL/IConsoleManager.h"

enum EShaderFrequency
{
	SF_Vertex			= 0,
	SF_Hull				= 1,
	SF_Domain			= 2,
	SF_Pixel			= 3,
	SF_Geometry			= 4,
	SF_Compute			= 5,

	SF_NumFrequencies	= 6,

	SF_NumBits			= 3,
};
static_assert(SF_NumFrequencies <= (1 << SF_NumBits), "SF_NumFrequencies will not fit on SF_NumBits");

/** @warning: update *LegacyShaderPlatform* when the below changes */
enum EShaderPlatform
{
	SP_PCD3D_SM5		= 0,
	SP_OPENGL_SM4		= 1,
	SP_PS4				= 2,
	/** Used when running in Feature Level ES2 in OpenGL. */
	SP_OPENGL_PCES2		= 3,
	SP_XBOXONE_D3D12    = 4,
	SP_PCD3D_SM4		= 5,
	SP_OPENGL_SM5		= 6,
	/** Used when running in Feature Level ES2 in D3D11. */
	SP_PCD3D_ES2		= 7,
	SP_OPENGL_ES2_ANDROID = 8,
	SP_OPENGL_ES2_WEBGL = 9, 
	SP_OPENGL_ES2_IOS	= 10,
	SP_METAL			= 11,
	SP_METAL_MRT		= 12,
	SP_OPENGL_ES31_EXT	= 13,
	/** Used when running in Feature Level ES3_1 in D3D11. */
	SP_PCD3D_ES3_1		= 14,
	/** Used when running in Feature Level ES3_1 in OpenGL. */
	SP_OPENGL_PCES3_1	= 15,
	SP_METAL_SM5		= 16,
	SP_VULKAN_PCES3_1	= 17,
	SP_METAL_SM4		= 18,
	SP_VULKAN_SM4		= 19,
	SP_VULKAN_SM5		= 20,
	SP_VULKAN_ES3_1_ANDROID = 21,
	SP_METAL_MACES3_1 	= 22,
	SP_METAL_MACES2		= 23,
	SP_OPENGL_ES3_1_ANDROID = 24,
	SP_SWITCH				= 25,
	SP_SWITCH_FORWARD		= 26,
	SP_METAL_MRT_MAC	= 27,

	SP_NumPlatforms		= 28,
	SP_NumBits			= 5,
};
static_assert(SP_NumPlatforms <= (1 << SP_NumBits), "SP_NumPlatforms will not fit on SP_NumBits");

enum ERenderQueryType
{
	// e.g. WaitForFrameEventCompletion()
	RQT_Undefined,
	// Result is the number of samples that are not culled (divide by MSAACount to get pixels)
	RQT_Occlusion,
	// Result is time in micro seconds = 1/1000 ms = 1/1000000 sec
	RQT_AbsoluteTime,
};

/** Maximum number of miplevels in a texture. */
enum { MAX_TEXTURE_MIP_COUNT = 14 };

/** The maximum number of vertex elements which can be used by a vertex declaration. */
enum
{
	MaxVertexElementCount = 16,
	MaxVertexElementCount_NumBits = 4,
};
static_assert(MaxVertexElementCount <= (1 << MaxVertexElementCount_NumBits), "MaxVertexElementCount will not fit on MaxVertexElementCount_NumBits");

/** The alignment in bytes between elements of array shader parameters. */
enum { ShaderArrayElementAlignBytes = 16 };

/** The number of render-targets that may be simultaneously written to. */
enum
{
	MaxSimultaneousRenderTargets = 8,
	MaxSimultaneousRenderTargets_NumBits = 3,
};
static_assert(MaxSimultaneousRenderTargets <= (1 << MaxSimultaneousRenderTargets_NumBits), "MaxSimultaneousRenderTargets will not fit on MaxSimultaneousRenderTargets_NumBits");

/** The number of UAVs that may be simultaneously bound to a shader. */
enum { MaxSimultaneousUAVs = 8 };

enum class ERHIZBuffer
{
	// Before changing this, make sure all math & shader assumptions are correct! Also wrap your C++ assumptions with
	//		static_assert(ERHIZBuffer::IsInvertedZBuffer(), ...);
	// Shader-wise, make sure to update Definitions.usf, HAS_INVERTED_Z_BUFFER
	FarPlane = 0,
	NearPlane = 1,

	// 'bool' for knowing if the API is using Inverted Z buffer
	IsInverted = (int32)((int32)ERHIZBuffer::FarPlane < (int32)ERHIZBuffer::NearPlane),
};

/**
 * The RHI's feature level indicates what level of support can be relied upon.
 * Note: these are named after graphics API's like ES2 but a feature level can be used with a different API (eg ERHIFeatureLevel::ES2 on D3D11)
 * As long as the graphics API supports all the features of the feature level (eg no ERHIFeatureLevel::SM5 on OpenGL ES2)
 */
namespace ERHIFeatureLevel
{
	enum Type
	{
		/** Feature level defined by the core capabilities of OpenGL ES2. */
		ES2,
		/** Feature level defined by the core capabilities of OpenGL ES3.1 & Metal/Vulkan. */
		ES3_1,
		/** Feature level defined by the capabilities of DX10 Shader Model 4. */
		SM4,
		/** Feature level defined by the capabilities of DX11 Shader Model 5. */
		SM5,
		Num
	};
};

enum ESamplerFilter
{
	SF_Point,
	SF_Bilinear,
	SF_Trilinear,
	SF_AnisotropicPoint,
	SF_AnisotropicLinear,

	ESamplerFilter_Num,
	ESamplerFilter_NumBits = 3,
};
static_assert(ESamplerFilter_Num <= (1 << ESamplerFilter_NumBits), "ESamplerFilter_Num will not fit on ESamplerFilter_NumBits");

enum ESamplerAddressMode
{
	AM_Wrap,
	AM_Clamp,
	AM_Mirror,
	/** Not supported on all platforms */
	AM_Border,

	ESamplerAddressMode_Num,
	ESamplerAddressMode_NumBits = 2,
};
static_assert(ESamplerAddressMode_Num <= (1 << ESamplerAddressMode_NumBits), "ESamplerAddressMode_Num will not fit on ESamplerAddressMode_NumBits");

enum ESamplerCompareFunction
{
	SCF_Never,
	SCF_Less
};

enum ERasterizerFillMode
{
	FM_Point,
	FM_Wireframe,
	FM_Solid,

	ERasterizerFillMode_Num,
	ERasterizerFillMode_NumBits = 2,
};
static_assert(ERasterizerFillMode_Num <= (1 << ERasterizerFillMode_NumBits), "ERasterizerFillMode_Num will not fit on ERasterizerFillMode_NumBits");

enum ERasterizerCullMode
{
	CM_None,
	CM_CW,
	CM_CCW,

	ERasterizerCullMode_Num,
	ERasterizerCullMode_NumBits = 2,
};
static_assert(ERasterizerCullMode_Num <= (1 << ERasterizerCullMode_NumBits), "ERasterizerCullMode_Num will not fit on ERasterizerCullMode_NumBits");

enum EColorWriteMask
{
	CW_RED   = 0x01,
	CW_GREEN = 0x02,
	CW_BLUE  = 0x04,
	CW_ALPHA = 0x08,

	CW_NONE  = 0,
	CW_RGB   = CW_RED | CW_GREEN | CW_BLUE,
	CW_RGBA  = CW_RED | CW_GREEN | CW_BLUE | CW_ALPHA,
	CW_RG    = CW_RED | CW_GREEN,
	CW_BA    = CW_BLUE | CW_ALPHA,

	EColorWriteMask_NumBits = 4,
};

enum ECompareFunction
{
	CF_Less,
	CF_LessEqual,
	CF_Greater,
	CF_GreaterEqual,
	CF_Equal,
	CF_NotEqual,
	CF_Never,
	CF_Always,

	ECompareFunction_Num,
	ECompareFunction_NumBits = 3,

	// Utility enumerations
	CF_DepthNearOrEqual		= (((int32)ERHIZBuffer::IsInverted != 0) ? CF_GreaterEqual : CF_LessEqual),
	CF_DepthNear			= (((int32)ERHIZBuffer::IsInverted != 0) ? CF_Greater : CF_Less),
	CF_DepthFartherOrEqual	= (((int32)ERHIZBuffer::IsInverted != 0) ? CF_LessEqual : CF_GreaterEqual),
	CF_DepthFarther			= (((int32)ERHIZBuffer::IsInverted != 0) ? CF_Less : CF_Greater),
};
static_assert(ECompareFunction_Num <= (1 << ECompareFunction_NumBits), "ECompareFunction_Num will not fit on ECompareFunction_NumBits");

enum EStencilMask
{
	SM_Default,
	SM_255,
	SM_1,
	SM_2,
	SM_4,
	SM_8,
	SM_16,
	SM_32,
	SM_64,
	SM_128,
	SM_Count
};

enum EStencilOp
{
	SO_Keep,
	SO_Zero,
	SO_Replace,
	SO_SaturatedIncrement,
	SO_SaturatedDecrement,
	SO_Invert,
	SO_Increment,
	SO_Decrement,

	EStencilOp_Num,
	EStencilOp_NumBits = 3,
};
static_assert(EStencilOp_Num <= (1 << EStencilOp_NumBits), "EStencilOp_Num will not fit on EStencilOp_NumBits");

enum EBlendOperation
{
	BO_Add,
	BO_Subtract,
	BO_Min,
	BO_Max,
	BO_ReverseSubtract,

	EBlendOperation_Num,
	EBlendOperation_NumBits = 3,
};
static_assert(EBlendOperation_Num <= (1 << EBlendOperation_NumBits), "EBlendOperation_Num will not fit on EBlendOperation_NumBits");

enum EBlendFactor
{
	BF_Zero,
	BF_One,
	BF_SourceColor,
	BF_InverseSourceColor,
	BF_SourceAlpha,
	BF_InverseSourceAlpha,
	BF_DestAlpha,
	BF_InverseDestAlpha,
	BF_DestColor,
	BF_InverseDestColor,
	BF_ConstantBlendFactor,
	BF_InverseConstantBlendFactor,

	EBlendFactor_Num,
	EBlendFactor_NumBits = 4,
};
static_assert(EBlendFactor_Num <= (1 << EBlendFactor_NumBits), "EBlendFactor_Num will not fit on EBlendFactor_NumBits");

enum EVertexElementType
{
	VET_None,
	VET_Float1,
	VET_Float2,
	VET_Float3,
	VET_Float4,
	VET_PackedNormal,	// FPackedNormal
	VET_UByte4,
	VET_UByte4N,
	VET_Color,
	VET_Short2,
	VET_Short4,
	VET_Short2N,		// 16 bit word normalized to (value/32767.0,value/32767.0,0,0,1)
	VET_Half2,			// 16 bit float using 1 bit sign, 5 bit exponent, 10 bit mantissa 
	VET_Half4,
	VET_Short4N,		// 4 X 16 bit word, normalized 
	VET_UShort2,
	VET_UShort4,
	VET_UShort2N,		// 16 bit word normalized to (value/65535.0,value/65535.0,0,0,1)
	VET_UShort4N,		// 4 X 16 bit word unsigned, normalized 
	VET_URGB10A2N,		// 10 bit r, g, b and 2 bit a normalized to (value/1023.0f, value/1023.0f, value/1023.0f, value/3.0f)
	VET_MAX,

	VET_NumBits = 5,
};
static_assert(VET_MAX <= (1 << VET_NumBits), "VET_MAX will not fit on VET_NumBits");

enum ECubeFace
{
	CubeFace_PosX = 0,
	CubeFace_NegX,
	CubeFace_PosY,
	CubeFace_NegY,
	CubeFace_PosZ,
	CubeFace_NegZ,
	CubeFace_MAX
};

enum EUniformBufferUsage
{
	// the uniform buffer is temporary, used for a single draw call then discarded
	UniformBuffer_SingleDraw = 0,
	// the uniform buffer is used for multiple draw calls but only for the current frame
	UniformBuffer_SingleFrame,
	// the uniform buffer is used for multiple draw calls, possibly across multiple frames
	UniformBuffer_MultiFrame,
};

/** The base type of a value in a uniform buffer. */
enum EUniformBufferBaseType
{
	UBMT_INVALID,
	UBMT_BOOL,
	UBMT_INT32,
	UBMT_UINT32,
	UBMT_FLOAT32,
	UBMT_STRUCT,
	UBMT_SRV,
	UBMT_UAV,
	UBMT_SAMPLER,
	UBMT_TEXTURE,

	EUniformBufferBaseType_Num,
	EUniformBufferBaseType_NumBits = 4,
};
static_assert(EUniformBufferBaseType_Num <= (1 << EUniformBufferBaseType_NumBits), "EUniformBufferBaseType_Num will not fit on EUniformBufferBaseType_NumBits");

struct FRHIResourceTableEntry
{
public:
	static CONSTEXPR uint32 GetEndOfStreamToken()
	{
		return 0xffffffff;
	}

	static uint32 Create(uint16 UniformBufferIndex, uint16 ResourceIndex, uint16 BindIndex)
	{
		return ((UniformBufferIndex & RTD_Mask_UniformBufferIndex) << RTD_Shift_UniformBufferIndex) |
			((ResourceIndex & RTD_Mask_ResourceIndex) << RTD_Shift_ResourceIndex) |
			((BindIndex & RTD_Mask_BindIndex) << RTD_Shift_BindIndex);
	}

	static inline uint16 GetUniformBufferIndex(uint32 Data)
	{
		return (Data >> RTD_Shift_UniformBufferIndex) & RTD_Mask_UniformBufferIndex;
	}

	static inline uint16 GetResourceIndex(uint32 Data)
	{
		return (Data >> RTD_Shift_ResourceIndex) & RTD_Mask_ResourceIndex;
	}

	static inline uint16 GetBindIndex(uint32 Data)
	{
		return (Data >> RTD_Shift_BindIndex) & RTD_Mask_BindIndex;
	}

private:
	enum EResourceTableDefinitions
	{
		RTD_NumBits_UniformBufferIndex	= 8,
		RTD_NumBits_ResourceIndex		= 16,
		RTD_NumBits_BindIndex			= 8,

		RTD_Mask_UniformBufferIndex		= (1 << RTD_NumBits_UniformBufferIndex) - 1,
		RTD_Mask_ResourceIndex			= (1 << RTD_NumBits_ResourceIndex) - 1,
		RTD_Mask_BindIndex				= (1 << RTD_NumBits_BindIndex) - 1,

		RTD_Shift_BindIndex				= 0,
		RTD_Shift_ResourceIndex			= RTD_Shift_BindIndex + RTD_NumBits_BindIndex,
		RTD_Shift_UniformBufferIndex	= RTD_Shift_ResourceIndex + RTD_NumBits_ResourceIndex,
	};
	static_assert(RTD_NumBits_UniformBufferIndex + RTD_NumBits_ResourceIndex + RTD_NumBits_BindIndex <= sizeof(uint32)* 8, "RTD_* values must fit in 32 bits");
};

enum EResourceLockMode
{
	RLM_ReadOnly,
	RLM_WriteOnly,
	RLM_Num
};

/** limited to 8 types in FReadSurfaceDataFlags */
enum ERangeCompressionMode
{
	// 0 .. 1
	RCM_UNorm,
	// -1 .. 1
	RCM_SNorm,
	// 0 .. 1 unless there are smaller values than 0 or bigger values than 1, then the range is extended to the minimum or the maximum of the values
	RCM_MinMaxNorm,
	// minimum .. maximum (each channel independent)
	RCM_MinMax,
};

enum class EPrimitiveTopologyType : uint8
{
	Triangle,
	Patch,
	Line,
	Point,
	//Quad,

	Num,
	NumBits = 2,
};
static_assert((uint32)EPrimitiveTopologyType::Num <= (1 << (uint32)EPrimitiveTopologyType::NumBits), "EPrimitiveTopologyType::Num will not fit on EPrimitiveTopologyType::NumBits");

enum EPrimitiveType
{
	PT_TriangleList,
	PT_TriangleStrip,
	PT_LineList,
	PT_QuadList,
	PT_PointList,
	PT_1_ControlPointPatchList,
	PT_2_ControlPointPatchList,
	PT_3_ControlPointPatchList,
	PT_4_ControlPointPatchList,
	PT_5_ControlPointPatchList,
	PT_6_ControlPointPatchList,
	PT_7_ControlPointPatchList,
	PT_8_ControlPointPatchList,
	PT_9_ControlPointPatchList,
	PT_10_ControlPointPatchList,
	PT_11_ControlPointPatchList,
	PT_12_ControlPointPatchList,
	PT_13_ControlPointPatchList,
	PT_14_ControlPointPatchList,
	PT_15_ControlPointPatchList,
	PT_16_ControlPointPatchList,
	PT_17_ControlPointPatchList,
	PT_18_ControlPointPatchList,
	PT_19_ControlPointPatchList,
	PT_20_ControlPointPatchList,
	PT_21_ControlPointPatchList,
	PT_22_ControlPointPatchList,
	PT_23_ControlPointPatchList,
	PT_24_ControlPointPatchList,
	PT_25_ControlPointPatchList,
	PT_26_ControlPointPatchList,
	PT_27_ControlPointPatchList,
	PT_28_ControlPointPatchList,
	PT_29_ControlPointPatchList,
	PT_30_ControlPointPatchList,
	PT_31_ControlPointPatchList,
	PT_32_ControlPointPatchList,
	PT_Num,
	PT_NumBits = 6
};
static_assert(PT_Num <= (1 << 8), "EPrimitiveType doesn't fit in a byte");
static_assert(PT_Num <= (1 << PT_NumBits), "PT_NumBits is too small");

/**
 *	Resource usage flags - for vertex and index buffers.
 */
enum EBufferUsageFlags
{
	BUF_None			  = 0x0000,
	
	// Mutually exclusive write-frequency flags

	/** The buffer will be written to once. */
	BUF_Static            = 0x0001, 

	/** 
	 * The buffer will be written to occasionally, GPU read only, CPU write only.  The data lifetime is until the next update, or the buffer is destroyed.
	 * Warning: On PS4, BUF_Dynamic do not support multiple updates per frame!  Later updates will overwrite earlier ones, causing a race condition with the GPU.
	 */
	BUF_Dynamic           = 0x0002, 

	/** The buffer's data will have a lifetime of one frame.  It MUST be written to each frame, or a new one created each frame. */
	BUF_Volatile          = 0x0004, 

	// Mutually exclusive bind flags.
	BUF_UnorderedAccess   = 0x0008, // Allows an unordered access view to be created for the buffer.

	/** Create a byte address buffer, which is basically a structured buffer with a uint32 type. */
	BUF_ByteAddressBuffer = 0x0020,
	/** Create a structured buffer with an atomic UAV counter. */
	BUF_UAVCounter        = 0x0040,
	/** Create a buffer that can be bound as a stream output target. */
	BUF_StreamOutput      = 0x0080,
	/** Create a buffer which contains the arguments used by DispatchIndirect or DrawIndirect. */
	BUF_DrawIndirect      = 0x0100,
	/** 
	 * Create a buffer that can be bound as a shader resource. 
	 * This is only needed for buffer types which wouldn't ordinarily be used as a shader resource, like a vertex buffer.
	 */
	BUF_ShaderResource    = 0x0200,

	/**
	 * Request that this buffer is directly CPU accessible
	 * (@todo josh: this is probably temporary and will go away in a few months)
	 */
	BUF_KeepCPUAccessible = 0x0400,

	/**
	 * Provide information that this buffer will contain only one vertex, which should be delivered to every primitive drawn.
	 * This is necessary for OpenGL implementations, which need to handle this case very differently (and can't handle GL_HALF_FLOAT in such vertices at all).
	 */
	BUF_ZeroStride        = 0x0800,

	/** Buffer should go in fast vram (hint only). Requires BUF_Transient */
	BUF_FastVRAM          = 0x1000,

	/** Buffer should be allocated from transient memory. */
	BUF_Transient		  = 0x2000,

	/** Buffer that should be accessed one byte at a time. */
	BUF_UINT8             = 0x4000,

	// Helper bit-masks
	BUF_AnyDynamic = (BUF_Dynamic | BUF_Volatile),
};

/** An enumeration of the different RHI reference types. */
enum ERHIResourceType
{
	RRT_None,

	RRT_SamplerState,
	RRT_RasterizerState,
	RRT_DepthStencilState,
	RRT_BlendState,
	RRT_VertexDeclaration,
	RRT_VertexShader,
	RRT_HullShader,
	RRT_DomainShader,
	RRT_PixelShader,
	RRT_GeometryShader,
	RRT_ComputeShader,
	RRT_BoundShaderState,
	RRT_UniformBuffer,
	RRT_IndexBuffer,
	RRT_VertexBuffer,
	RRT_StructuredBuffer,
	RRT_Texture,
	RRT_Texture2D,
	RRT_Texture2DArray,
	RRT_Texture3D,
	RRT_TextureCube,
	RRT_TextureReference,
	RRT_RenderQuery,
	RRT_Viewport,
	RRT_UnorderedAccessView,
	RRT_ShaderResourceView,

	RRT_Num
};

/** Flags used for texture creation */
enum ETextureCreateFlags
{
	TexCreate_None					= 0,

	// Texture can be used as a render target
	TexCreate_RenderTargetable		= 1<<0,
	// Texture can be used as a resolve target
	TexCreate_ResolveTargetable		= 1<<1,
	// Texture can be used as a depth-stencil target.
	TexCreate_DepthStencilTargetable= 1<<2,
	// Texture can be used as a shader resource.
	TexCreate_ShaderResource		= 1<<3,

	// Texture is encoded in sRGB gamma space
	TexCreate_SRGB					= 1<<4,
	// Texture will be created without a packed miptail
	TexCreate_NoMipTail				= 1<<5,
	// Texture will be created with an un-tiled format
	TexCreate_NoTiling				= 1<<6,
	// Texture that may be updated every frame
	TexCreate_Dynamic				= 1<<8,
	// Allow silent texture creation failure
	TexCreate_AllowFailure			= 1<<9,
	// Disable automatic defragmentation if the initial texture memory allocation fails.
	TexCreate_DisableAutoDefrag		= 1<<10,
	// Create the texture with automatic -1..1 biasing
	TexCreate_BiasNormalMap			= 1<<11,
	// Create the texture with the flag that allows mip generation later, only applicable to D3D11
	TexCreate_GenerateMipCapable	= 1<<12,
	// UnorderedAccessView (DX11 only)
	// Warning: Causes additional synchronization between draw calls when using a render target allocated with this flag, use sparingly
	// See: GCNPerformanceTweets.pdf Tip 37
	TexCreate_UAV					= 1<<16,
	// Render target texture that will be displayed on screen (back buffer)
	TexCreate_Presentable			= 1<<17,
	// Texture data is accessible by the CPU
	TexCreate_CPUReadback			= 1<<18,
	// Texture was processed offline (via a texture conversion process for the current platform)
	TexCreate_OfflineProcessed		= 1<<19,
	// Texture needs to go in fast VRAM if available (HINT only)
	TexCreate_FastVRAM				= 1<<20,
	// by default the texture is not showing up in the list - this is to reduce clutter, using the FULL option this can be ignored
	TexCreate_HideInVisualizeTexture= 1<<21,
	// Texture should be created in virtual memory, with no physical memory allocation made
	// You must make further calls to RHIVirtualTextureSetFirstMipInMemory to allocate physical memory
	// and RHIVirtualTextureSetFirstMipVisible to map the first mip visible to the GPU
	TexCreate_Virtual				= 1<<22,
	// Creates a RenderTargetView for each array slice of the texture
	// Warning: if this was specified when the resource was created, you can't use SV_RenderTargetArrayIndex to route to other slices!
	TexCreate_TargetArraySlicesIndependently	= 1<<23,
	// Texture that may be shared with DX9 or other devices
	TexCreate_Shared = 1 << 24,
	// RenderTarget will not use full-texture fast clear functionality.
	TexCreate_NoFastClear = 1 << 25,
	// Texture is a depth stencil resolve target
	TexCreate_DepthStencilResolveTarget = 1 << 26,
	// Flag used to indicted this texture is a streamable 2D texture, and should be counted towards the texture streaming pool budget.
	TexCreate_Streamable = 1 << 27,
	// Render target will not FinalizeFastClear; Caches and meta data will be flushed, but clearing will be skipped (avoids potentially trashing metadata)
	TexCreate_NoFastClearFinalize = 1 << 28,
	// Hint to the driver that this resource is managed properly by the engine for Alternate-Frame-Rendering in mGPU usage.
	TexCreate_AFRManual = 1 << 29,
	// Workaround for 128^3 volume textures getting bloated 4x due to tiling mode on PS4
	TexCreate_ReduceMemoryWithTilingMode = 1 << 30,
	/** Texture should be allocated from transient memory. */
	TexCreate_Transient = 1 << 31
};

enum EAsyncComputePriority
{
	AsyncComputePriority_Default = 0,
	AsyncComputePriority_High,
};
/**
 * Async texture reallocation status, returned by RHIGetReallocateTexture2DStatus().
 */
enum ETextureReallocationStatus
{
	TexRealloc_Succeeded = 0,
	TexRealloc_Failed,
	TexRealloc_InProgress,
};

/**
 * Action to take when a rendertarget is set.
 */
enum class ERenderTargetLoadAction : uint8
{
	ENoAction,
	ELoad,
	EClear,

	Num,
	NumBits = 2,
};
static_assert((uint32)ERenderTargetLoadAction::Num <= (1 << (uint32)ERenderTargetLoadAction::NumBits), "ERenderTargetLoadAction::Num will not fit on ERenderTargetLoadAction::NumBits");


/**
 * Action to take when a rendertarget is unset or at the end of a pass. 
 */
enum class ERenderTargetStoreAction : uint8
{
	ENoAction,
	EStore,
	EMultisampleResolve,

	Num,
	NumBits = 2,
};
static_assert((uint32)ERenderTargetStoreAction::Num <= (1 << (uint32)ERenderTargetStoreAction::NumBits), "ERenderTargetStoreAction::Num will not fit on ERenderTargetStoreAction::NumBits");

/**
 * Common render target use cases
 */
enum class ESimpleRenderTargetMode
{
	// These will all store out color and depth
	EExistingColorAndDepth,							// Color = Existing, Depth = Existing
	EUninitializedColorAndDepth,					// Color = ????, Depth = ????
	EUninitializedColorExistingDepth,				// Color = ????, Depth = Existing
	EUninitializedColorClearDepth,					// Color = ????, Depth = Default
	EClearColorExistingDepth,						// Clear Color = whatever was bound to the rendertarget at creation time. Depth = Existing
	EClearColorAndDepth,							// Clear color and depth to bound clear values.
	EExistingContents_NoDepthStore,					// Load existing contents, but don't store depth out.  depth can be written.
	EExistingColorAndClearDepth,					// Color = Existing, Depth = clear value
	EExistingColorAndDepthAndClearStencil,			// Color = Existing, Depth = Existing, Stencil = clear

	// If you add an item here, make sure to add it to DecodeRenderTargetMode() as well!
};

enum class EClearDepthStencil
{
	Depth,
	Stencil,
	DepthStencil,
};

/**
 * Hint to the driver on how to load balance async compute work.  On some platforms this may be a priority, on others actually masking out parts of the GPU for types of work.
 */
enum class EAsyncComputeBudget
{
	ELeast_0,			//Least amount of GPU allocated to AsyncCompute that still gets 'some' done.
	EGfxHeavy_1,		//Gfx gets most of the GPU.
	EBalanced_2,		//Async compute and Gfx share GPU equally.
	EComputeHeavy_3,	//Async compute can use most of the GPU
	EAll_4,				//Async compute can use the entire GPU.
};

inline bool IsPCPlatform(const EShaderPlatform Platform)
{
	return Platform == SP_PCD3D_SM5 || Platform == SP_PCD3D_SM4 || Platform == SP_PCD3D_ES2 || Platform == SP_PCD3D_ES3_1 ||
		Platform == SP_OPENGL_SM4 || Platform == SP_OPENGL_SM5 || Platform == SP_OPENGL_PCES2 || Platform == SP_OPENGL_PCES3_1 ||
		Platform == SP_METAL_SM4 || Platform == SP_METAL_SM5 ||
		Platform == SP_VULKAN_PCES3_1 || Platform == SP_VULKAN_SM4 || Platform == SP_VULKAN_SM5 || Platform == SP_METAL_MACES3_1 || Platform == SP_METAL_MACES2 || Platform == SP_METAL_MRT_MAC;
}

/** Whether the shader platform corresponds to the ES2 feature level. */
inline bool IsES2Platform(const EShaderPlatform Platform)
{
	return Platform == SP_PCD3D_ES2 || Platform == SP_OPENGL_PCES2 || Platform == SP_OPENGL_ES2_ANDROID || Platform == SP_OPENGL_ES2_WEBGL || Platform == SP_OPENGL_ES2_IOS || Platform == SP_METAL_MACES2;
}

/** Whether the shader platform corresponds to the ES2/ES3.1 feature level. */
inline bool IsMobilePlatform(const EShaderPlatform Platform)
{
	return IsES2Platform(Platform)
		|| Platform == SP_METAL || Platform == SP_PCD3D_ES3_1 || Platform == SP_OPENGL_PCES3_1 || Platform == SP_VULKAN_ES3_1_ANDROID
		|| Platform == SP_VULKAN_PCES3_1 || Platform == SP_METAL_MACES3_1 || Platform == SP_OPENGL_ES3_1_ANDROID || Platform == SP_SWITCH_FORWARD;
}

inline bool IsOpenGLPlatform(const EShaderPlatform Platform)
{
	return Platform == SP_OPENGL_SM4 || Platform == SP_OPENGL_SM5 || Platform == SP_OPENGL_PCES2 || Platform == SP_OPENGL_PCES3_1
		|| Platform == SP_OPENGL_ES2_ANDROID || Platform == SP_OPENGL_ES2_WEBGL || Platform == SP_OPENGL_ES2_IOS || Platform == SP_OPENGL_ES31_EXT
		|| Platform == SP_OPENGL_ES3_1_ANDROID || Platform == SP_SWITCH || Platform == SP_SWITCH_FORWARD;
}

inline bool IsMetalPlatform(const EShaderPlatform Platform)
{
	return Platform == SP_METAL || Platform == SP_METAL_MRT || Platform == SP_METAL_SM4 || Platform == SP_METAL_SM5 || Platform == SP_METAL_MACES3_1 || Platform == SP_METAL_MACES2 || Platform == SP_METAL_MRT_MAC;
}

inline bool IsConsolePlatform(const EShaderPlatform Platform)
{
	return Platform == SP_PS4 || Platform == SP_XBOXONE_D3D12;
}

inline bool IsSwitchPlatform(const EShaderPlatform Platform)
{
	return Platform == SP_SWITCH || Platform == SP_SWITCH_FORWARD;
}

inline bool IsPS4Platform(const EShaderPlatform Platform)
{
	return Platform == SP_PS4;
}

inline bool IsVulkanPlatform(const EShaderPlatform Platform)
{
	return Platform == SP_VULKAN_SM5 || Platform == SP_VULKAN_SM4 || Platform == SP_VULKAN_PCES3_1 || Platform == SP_VULKAN_ES3_1_ANDROID;
}

inline bool IsAndroidOpenGLESPlatform(const EShaderPlatform Platform)
{
	return Platform == SP_OPENGL_ES2_ANDROID || Platform == SP_OPENGL_ES3_1_ANDROID;
}

inline bool IsVulkanMobilePlatform(const EShaderPlatform Platform)
{
	return Platform == SP_VULKAN_PCES3_1 || Platform == SP_VULKAN_ES3_1_ANDROID;
}

inline bool IsD3DPlatform(const EShaderPlatform Platform, bool bIncludeXboxOne)
{
	switch (Platform)
	{
	case SP_PCD3D_SM5:
	case SP_PCD3D_SM4:
	case SP_PCD3D_ES3_1:
	case SP_PCD3D_ES2:
		return true;
	case SP_XBOXONE_D3D12:
		return bIncludeXboxOne;
	default:
		break;
	}

	return false;
}

inline bool IsHlslccShaderPlatform(const EShaderPlatform Platform)
{
	return IsMetalPlatform(Platform) || IsVulkanPlatform(Platform) || IsSwitchPlatform(Platform) || IsOpenGLPlatform(Platform);
}

inline ERHIFeatureLevel::Type GetMaxSupportedFeatureLevel(EShaderPlatform InShaderPlatform)
{
	switch (InShaderPlatform)
	{
	case SP_PCD3D_SM5:
	case SP_OPENGL_SM5:
	case SP_PS4:
	case SP_XBOXONE_D3D12:
	case SP_OPENGL_ES31_EXT:
	case SP_METAL_SM5:
	case SP_METAL_MRT:
	case SP_METAL_MRT_MAC:
	case SP_VULKAN_SM5:
	case SP_SWITCH:
		return ERHIFeatureLevel::SM5;
	case SP_VULKAN_SM4:
	case SP_PCD3D_SM4:
	case SP_OPENGL_SM4:
	case SP_METAL_SM4:
		return ERHIFeatureLevel::SM4;
	case SP_PCD3D_ES2:
	case SP_OPENGL_PCES2:
	case SP_OPENGL_ES2_ANDROID:
	case SP_OPENGL_ES2_WEBGL:
	case SP_OPENGL_ES2_IOS:
	case SP_METAL_MACES2:
		return ERHIFeatureLevel::ES2;
	case SP_METAL:
	case SP_METAL_MACES3_1:
	case SP_PCD3D_ES3_1:
	case SP_OPENGL_PCES3_1:
	case SP_VULKAN_PCES3_1:
	case SP_VULKAN_ES3_1_ANDROID:
	case SP_OPENGL_ES3_1_ANDROID:
	case SP_SWITCH_FORWARD:
		return ERHIFeatureLevel::ES3_1;
	default:
		checkf(0, TEXT("Unknown ShaderPlatform %d"), (int32)InShaderPlatform);
		return ERHIFeatureLevel::Num;
	}
}

/** Returns true if the feature level is supported by the shader platform. */
inline bool IsFeatureLevelSupported(EShaderPlatform InShaderPlatform, ERHIFeatureLevel::Type InFeatureLevel)
{
	return InFeatureLevel <= GetMaxSupportedFeatureLevel(InShaderPlatform);
}


inline bool RHINeedsToSwitchVerticalAxis(EShaderPlatform Platform)
{
#if WITH_EDITOR
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.ForceRHISwitchVerticalAxis"));
	if (CVar->GetValueOnAnyThread())
	{
		return true;
	}
#endif

	// ES2 & ES3.1 need to flip when rendering to an RT that will be post processed
	return IsOpenGLPlatform(Platform) && IsMobilePlatform(Platform) && !IsPCPlatform(Platform) && Platform != SP_METAL && !IsVulkanPlatform(Platform)
		   && Platform != SP_SWITCH && Platform != SP_SWITCH_FORWARD;
}

inline bool RHISupportsSeparateMSAAAndResolveTextures(const EShaderPlatform Platform)
{
	// Metal mobile devices, Vulkan and Android ES2/3.1 need to handle MSAA and resolve textures internally (unless RHICreateTexture2D was changed to take an optional resolve target)
	const bool bMobileMetalDevice = (Platform == SP_METAL || Platform == SP_METAL_MRT);
	return !bMobileMetalDevice && !IsVulkanPlatform(Platform) && !IsAndroidOpenGLESPlatform(Platform);
}

inline bool RHISupportsComputeShaders(const EShaderPlatform Platform)
{
	//@todo-rco: Add Metal & ES3.1 support
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
}

inline bool RHISupportsGeometryShaders(const EShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && !IsMetalPlatform(Platform) && Platform != SP_VULKAN_PCES3_1 && Platform != SP_VULKAN_ES3_1_ANDROID;
}

inline bool RHISupportsShaderCompression(const EShaderPlatform Platform)
{
	return true;
}

inline bool RHIHasTiledGPU(const EShaderPlatform Platform)
{
	// @todo MetalMRT Technically we should include (Platform == SP_METAL_MRT) but this would disable depth-pre-pass which is currently required.
	return Platform == SP_METAL || Platform == SP_OPENGL_ES2_IOS || Platform == SP_OPENGL_ES2_ANDROID || Platform == SP_OPENGL_ES3_1_ANDROID;
}

inline bool RHISupportsVertexShaderLayer(const EShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && IsPCPlatform(Platform) && IsMetalPlatform(Platform);
}

inline bool RHISupportsMobileMultiView(const EShaderPlatform Platform)
{
	return (Platform == EShaderPlatform::SP_OPENGL_ES3_1_ANDROID || Platform == EShaderPlatform::SP_OPENGL_ES2_ANDROID);
}

// Return what the expected number of samplers will be supported by a feature level
// Note that since the Feature Level is pretty orthogonal to the RHI/HW, this is not going to be perfect
// If should only be used for a guess at the limit, the real limit will not be known until runtime
inline uint32 GetExpectedFeatureLevelMaxTextureSamplers(ERHIFeatureLevel::Type FeatureLevel)
{
	if (FeatureLevel == ERHIFeatureLevel::ES2)
	{
		return 8;
	}
	else
	{
		return 16;
	}
}

inline int32 GetFeatureLevelMaxNumberOfBones(ERHIFeatureLevel::Type FeatureLevel)
{
	switch (FeatureLevel)
	{
	case ERHIFeatureLevel::ES2:
		return 75;
	case ERHIFeatureLevel::ES3_1:
	case ERHIFeatureLevel::SM4:
	case ERHIFeatureLevel::SM5:
		return 256;
	default:
		checkf(0, TEXT("Unknown FeatureLevel %d"), (int32)FeatureLevel);
	}

	return 0;
}

inline bool IsUniformBufferResourceType(EUniformBufferBaseType BaseType)
{
	return BaseType == UBMT_SRV || BaseType == UBMT_UAV || BaseType == UBMT_SAMPLER || BaseType == UBMT_TEXTURE;
}

inline const TCHAR* GetShaderFrequencyString(EShaderFrequency Frequency, bool bIncludePrefix = true)
{
	const TCHAR* String = TEXT("SF_NumFrequencies");
	switch (Frequency)
	{
	case SF_Vertex:			String = TEXT("SF_Vertex"); break;
	case SF_Hull:			String = TEXT("SF_Hull"); break;
	case SF_Domain:			String = TEXT("SF_Domain"); break;
	case SF_Geometry:		String = TEXT("SF_Geometry"); break;
	case SF_Pixel:			String = TEXT("SF_Pixel"); break;
	case SF_Compute:		String = TEXT("SF_Compute"); break;
	default:				
		checkf(0, TEXT("Unknown ShaderFrequency %d"), (int32)Frequency);
		break;
	}

	// Skip SF_
	int32 Index = bIncludePrefix ? 0 : 3;
	String += Index;
	return String;
};

// WaveWorks Start
struct WaveWorksShaderInput
{
	EShaderFrequency Frequency;
	ERHIResourceType Type;
	FName Name;
};
// WaveWorks End
