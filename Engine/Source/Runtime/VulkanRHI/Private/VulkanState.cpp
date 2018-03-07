// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanState.cpp: Vulkan state implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"

inline VkSamplerMipmapMode TranslateMipFilterMode(ESamplerFilter Filter)
{
	VkSamplerMipmapMode OutFilter = VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;

	switch (Filter)
	{
		case SF_Point:				OutFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST;	break;
		case SF_Bilinear:			OutFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST;	break;
		case SF_Trilinear:			OutFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR;	break;
		case SF_AnisotropicPoint:	OutFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR;	break;
		default:																break;
	}

	// Check for missing translation
	check(OutFilter != VK_SAMPLER_MIPMAP_MODE_MAX_ENUM);
	return OutFilter;
}

inline VkFilter TranslateMagFilterMode(ESamplerFilter InFilter)
{
	VkFilter OutFilter = VK_FILTER_MAX_ENUM;

	switch (InFilter)
	{
		case SF_Point:				OutFilter = VK_FILTER_NEAREST;	break;
		case SF_Bilinear:			OutFilter = VK_FILTER_LINEAR;	break;
		case SF_Trilinear:			OutFilter = VK_FILTER_LINEAR;	break;
		case SF_AnisotropicPoint:	OutFilter = VK_FILTER_LINEAR;	break;
		default:													break;
	}

	// Check for missing translation
	check(OutFilter != VK_FILTER_MAX_ENUM);
	return OutFilter;
}

inline VkFilter TranslateMinFilterMode(ESamplerFilter InFilter)
{
	VkFilter OutFilter = VK_FILTER_MAX_ENUM;

	switch (InFilter)
	{
		case SF_Point:				OutFilter = VK_FILTER_NEAREST;	break;
		case SF_Bilinear:			OutFilter = VK_FILTER_LINEAR;	break;
		case SF_Trilinear:			OutFilter = VK_FILTER_LINEAR;	break;
		case SF_AnisotropicPoint:	OutFilter = VK_FILTER_LINEAR;	break;
		default:													break;
	}

	// Check for missing translation
	check(OutFilter != VK_FILTER_MAX_ENUM);
	return OutFilter;
}

inline VkSamplerAddressMode TranslateWrapMode(ESamplerAddressMode InAddressMode, const bool bSupportsMirrorClampToEdge)
{
	VkSamplerAddressMode OutAddressMode = VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;

	switch (InAddressMode)
	{
		case AM_Wrap:		OutAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;			break;
		case AM_Clamp:		OutAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;		break;
		case AM_Mirror:		OutAddressMode = bSupportsMirrorClampToEdge
												? VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE
												: VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;		break;
		case AM_Border:		OutAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;	break;
		default:
			break;
	}

	// Check for missing translation
	check(OutAddressMode != VK_SAMPLER_ADDRESS_MODE_MAX_ENUM);
	return OutAddressMode;
}

inline VkCompareOp TranslateSamplerCompareFunction(ESamplerCompareFunction InSamplerComparisonFunction)
{
	VkCompareOp OutSamplerComparisonFunction = VK_COMPARE_OP_MAX_ENUM;

	switch (InSamplerComparisonFunction)
	{
		case SCF_Less:	OutSamplerComparisonFunction = VK_COMPARE_OP_LESS;	break;
		case SCF_Never:	OutSamplerComparisonFunction = VK_COMPARE_OP_NEVER;	break;
		default:															break;
	};

	// Check for missing translation
	check(OutSamplerComparisonFunction != VK_COMPARE_OP_MAX_ENUM);
	return OutSamplerComparisonFunction;
}

static inline VkBlendOp BlendOpToVulkan(EBlendOperation InOp)
{
	VkBlendOp OutOp = VK_BLEND_OP_MAX_ENUM;

	switch (InOp)
	{
		case BO_Add:				OutOp = VK_BLEND_OP_ADD;				break;
		case BO_Subtract:			OutOp = VK_BLEND_OP_SUBTRACT;			break;
		case BO_Min:				OutOp = VK_BLEND_OP_MIN;				break;
		case BO_Max:				OutOp = VK_BLEND_OP_MAX;				break;
		case BO_ReverseSubtract:	OutOp = VK_BLEND_OP_REVERSE_SUBTRACT;	break;
		default:															break;
	}

	// Check for missing translation
	check(OutOp != VK_BLEND_OP_MAX_ENUM);
	return OutOp;
}

static inline VkBlendFactor BlendFactorToVulkan(EBlendFactor InFactor)
{
	VkBlendFactor BlendMode = VK_BLEND_FACTOR_MAX_ENUM;

	switch (InFactor)
	{
		case BF_Zero:						BlendMode = VK_BLEND_FACTOR_ZERO;						break;
		case BF_One:						BlendMode = VK_BLEND_FACTOR_ONE;						break;
		case BF_SourceColor:				BlendMode = VK_BLEND_FACTOR_SRC_COLOR;					break;
		case BF_InverseSourceColor:			BlendMode = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;		break;
		case BF_SourceAlpha:				BlendMode = VK_BLEND_FACTOR_SRC_ALPHA;					break;
		case BF_InverseSourceAlpha:			BlendMode = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;		break;
		case BF_DestAlpha:					BlendMode = VK_BLEND_FACTOR_DST_ALPHA;					break;
		case BF_InverseDestAlpha:			BlendMode = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;		break;
		case BF_DestColor:					BlendMode = VK_BLEND_FACTOR_DST_COLOR;					break;
		case BF_InverseDestColor:			BlendMode = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;		break;
		case BF_ConstantBlendFactor:		BlendMode = VK_BLEND_FACTOR_CONSTANT_COLOR;				break;
		case BF_InverseConstantBlendFactor:	BlendMode = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;	break;
		default:																					break;
	}

	// Check for missing translation
	check(BlendMode != VK_BLEND_FACTOR_MAX_ENUM);
	return BlendMode;
}

static inline VkCompareOp CompareOpToVulkan(ECompareFunction InOp)
{
	VkCompareOp OutOp = VK_COMPARE_OP_MAX_ENUM;

	switch (InOp)
	{
		case CF_Less:			OutOp = VK_COMPARE_OP_LESS;				break;
		case CF_LessEqual:		OutOp = VK_COMPARE_OP_LESS_OR_EQUAL;	break;
		case CF_Greater:		OutOp = VK_COMPARE_OP_GREATER;			break;
		case CF_GreaterEqual:	OutOp = VK_COMPARE_OP_GREATER_OR_EQUAL;	break;
		case CF_Equal:			OutOp = VK_COMPARE_OP_EQUAL;			break;
		case CF_NotEqual:		OutOp = VK_COMPARE_OP_NOT_EQUAL;		break;
		case CF_Never:			OutOp = VK_COMPARE_OP_NEVER;			break;
		case CF_Always:			OutOp = VK_COMPARE_OP_ALWAYS;			break;
		default:														break;
	}

	// Check for missing translation
	check(OutOp != VK_COMPARE_OP_MAX_ENUM);
	return OutOp;
}

static inline VkStencilOp StencilOpToVulkan(EStencilOp InOp)
{
	VkStencilOp OutOp = VK_STENCIL_OP_MAX_ENUM;

	switch (InOp)
	{
		case SO_Keep:					OutOp = VK_STENCIL_OP_KEEP;					break;
		case SO_Zero:					OutOp = VK_STENCIL_OP_ZERO;					break;
		case SO_Replace:				OutOp = VK_STENCIL_OP_REPLACE;				break;
		case SO_SaturatedIncrement:		OutOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;	break;
		case SO_SaturatedDecrement:		OutOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP;	break;
		case SO_Invert:					OutOp = VK_STENCIL_OP_INVERT;				break;
		case SO_Increment:				OutOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;	break;
		case SO_Decrement:				OutOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;	break;
		default:																	break;
	}

	check(OutOp != VK_STENCIL_OP_MAX_ENUM);
	return OutOp;
}

static inline VkPolygonMode RasterizerFillModeToVulkan(ERasterizerFillMode InFillMode)
{
	VkPolygonMode OutFillMode = VK_POLYGON_MODE_MAX_ENUM;

	switch (InFillMode)
	{
		case FM_Point:			OutFillMode = VK_POLYGON_MODE_POINT;	break;
		case FM_Wireframe:		OutFillMode = VK_POLYGON_MODE_LINE;		break;
		case FM_Solid:			OutFillMode = VK_POLYGON_MODE_FILL;		break;
		default:														break;
	}

	// Check for missing translation
	check(OutFillMode != VK_POLYGON_MODE_MAX_ENUM);
	return OutFillMode;
}

static inline VkCullModeFlags RasterizerCullModeToVulkan(ERasterizerCullMode InCullMode)
{
	VkCullModeFlags outCullMode = VK_CULL_MODE_NONE;

	switch (InCullMode)
	{
		case CM_None:	outCullMode = VK_CULL_MODE_NONE;		break;
		case CM_CW:		outCullMode = VK_CULL_MODE_FRONT_BIT;	break;
		case CM_CCW:	outCullMode = VK_CULL_MODE_BACK_BIT;	break;
			// Check for missing translation
		default:		check(false);							break;
	}

	return outCullMode;
}


FVulkanSamplerState::FVulkanSamplerState(const FSamplerStateInitializerRHI& Initializer, FVulkanDevice& InDevice) :
	Sampler(VK_NULL_HANDLE),
	Device(InDevice)
{
#if !VULKAN_KEEP_CREATE_INFO
	VkSamplerCreateInfo SamplerInfo;
#endif
	FMemory::Memzero(SamplerInfo);
	SamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	SamplerInfo.magFilter = TranslateMagFilterMode(Initializer.Filter);
	SamplerInfo.minFilter = TranslateMinFilterMode(Initializer.Filter);
	SamplerInfo.mipmapMode = TranslateMipFilterMode(Initializer.Filter);
#if PLATFORM_ANDROID
	// Some Android devices might not support this extension
	const bool bSupportsMirrorClampToEdge = InDevice.GetOptionalExtensions().HasMirrorClampToEdge;
#else
	// All major desktop devices supports this extension
	const bool bSupportsMirrorClampToEdge = true;
#endif
	SamplerInfo.addressModeU = TranslateWrapMode(Initializer.AddressU, bSupportsMirrorClampToEdge);
	SamplerInfo.addressModeV = TranslateWrapMode(Initializer.AddressV, bSupportsMirrorClampToEdge);
	SamplerInfo.addressModeW = TranslateWrapMode(Initializer.AddressW, bSupportsMirrorClampToEdge);
	

	SamplerInfo.mipLodBias = Initializer.MipBias;
	SamplerInfo.maxAnisotropy = FMath::Clamp((float)ComputeAnisotropyRT(Initializer.MaxAnisotropy), 1.0f, InDevice.GetLimits().maxSamplerAnisotropy);
	SamplerInfo.anisotropyEnable = SamplerInfo.maxAnisotropy > 1;

	// FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Using LOD Group %d / %d, MaxAniso = %f \n"), (int32)Initializer.Filter, Initializer.MaxAnisotropy, SamplerInfo.maxAnisotropy);

	SamplerInfo.compareEnable = Initializer.SamplerComparisonFunction != SCF_Never ? VK_TRUE : VK_FALSE;
	SamplerInfo.compareOp = TranslateSamplerCompareFunction(Initializer.SamplerComparisonFunction);
	SamplerInfo.minLod = Initializer.MinMipLevel;
	SamplerInfo.maxLod = Initializer.MaxMipLevel;
	SamplerInfo.borderColor = Initializer.BorderColor == 0 ? VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK : VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	VERIFYVULKANRESULT(VulkanRHI::vkCreateSampler(Device.GetInstanceHandle(), &SamplerInfo, nullptr, &Sampler));
}

FVulkanSamplerState::~FVulkanSamplerState()
{
	Device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::Sampler, Sampler);
	Sampler = VK_NULL_HANDLE;
}

FVulkanRasterizerState::FVulkanRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	FVulkanRasterizerState::ResetCreateInfo(RasterizerState);

	// @todo vulkan: I'm assuming that Solid and Wireframe wouldn't ever be mixed within the same BoundShaderState, so we are ignoring the fill mode as a unique identifier
	//checkf(Initializer.FillMode == FM_Solid, TEXT("PIPELINE KEY: Only FM_Solid is supported fill mode [got %d]"), (int32)Initializer.FillMode);

	RasterizerState.polygonMode = RasterizerFillModeToVulkan(Initializer.FillMode);
	RasterizerState.cullMode = RasterizerCullModeToVulkan(Initializer.CullMode);

	//RasterizerState.depthClampEnable = VK_FALSE;
	RasterizerState.depthBiasEnable = Initializer.DepthBias != 0.0f ? VK_TRUE : VK_FALSE;
	//RasterizerState.rasterizerDiscardEnable = VK_FALSE;

	RasterizerState.depthBiasSlopeFactor = Initializer.SlopeScaleDepthBias;
	RasterizerState.depthBiasConstantFactor = Initializer.DepthBias;

	//RasterizerState.lineWidth = 1.0f;
}

FVulkanDepthStencilState::FVulkanDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
	FMemory::Memzero(DepthStencilState);
	DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	DepthStencilState.depthTestEnable = (Initializer.DepthTest != CF_Always || Initializer.bEnableDepthWrite) ? VK_TRUE : VK_FALSE;
	DepthStencilState.depthCompareOp = CompareOpToVulkan(Initializer.DepthTest);
	DepthStencilState.depthWriteEnable = Initializer.bEnableDepthWrite ? VK_TRUE : VK_FALSE;

	DepthStencilState.depthBoundsTestEnable = VK_FALSE;	// What is this?
	DepthStencilState.minDepthBounds = 0;
	DepthStencilState.maxDepthBounds = 0;

	DepthStencilState.stencilTestEnable = (Initializer.bEnableFrontFaceStencil || Initializer.bEnableBackFaceStencil) ? VK_TRUE : VK_FALSE;

	// Front
	DepthStencilState.back.failOp = StencilOpToVulkan(Initializer.FrontFaceStencilFailStencilOp);
	DepthStencilState.back.passOp = StencilOpToVulkan(Initializer.FrontFacePassStencilOp);
	DepthStencilState.back.depthFailOp = StencilOpToVulkan(Initializer.FrontFaceDepthFailStencilOp);
	DepthStencilState.back.compareOp = CompareOpToVulkan(Initializer.FrontFaceStencilTest);
	DepthStencilState.back.compareMask = Initializer.StencilReadMask;
	DepthStencilState.back.writeMask = Initializer.StencilWriteMask;
	DepthStencilState.back.reference = 0;

	if (Initializer.bEnableBackFaceStencil)
	{
		// Back
		DepthStencilState.front.failOp = StencilOpToVulkan(Initializer.BackFaceStencilFailStencilOp);
		DepthStencilState.front.passOp = StencilOpToVulkan(Initializer.BackFacePassStencilOp);
		DepthStencilState.front.depthFailOp = StencilOpToVulkan(Initializer.BackFaceDepthFailStencilOp);
		DepthStencilState.front.compareOp = CompareOpToVulkan(Initializer.BackFaceStencilTest);
		DepthStencilState.front.compareMask = Initializer.StencilReadMask;
		DepthStencilState.front.writeMask = Initializer.StencilWriteMask;
		DepthStencilState.front.reference = 0;
	}
	else
	{
		DepthStencilState.front = DepthStencilState.back;
	}
}

FVulkanBlendState::FVulkanBlendState(const FBlendStateInitializerRHI& Initializer)
{
	for (uint32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		const FBlendStateInitializerRHI::FRenderTarget& ColorTarget = Initializer.RenderTargets[Index];
		VkPipelineColorBlendAttachmentState& BlendState = BlendStates[Index];
		FMemory::Memzero(BlendState);

		BlendState.colorBlendOp = BlendOpToVulkan(ColorTarget.ColorBlendOp);
		BlendState.alphaBlendOp = BlendOpToVulkan(ColorTarget.AlphaBlendOp);

		BlendState.dstColorBlendFactor = BlendFactorToVulkan(ColorTarget.ColorDestBlend);
		BlendState.dstAlphaBlendFactor = BlendFactorToVulkan(ColorTarget.AlphaDestBlend);

		BlendState.srcColorBlendFactor = BlendFactorToVulkan(ColorTarget.ColorSrcBlend);
		BlendState.srcAlphaBlendFactor = BlendFactorToVulkan(ColorTarget.AlphaSrcBlend);

		BlendState.blendEnable =
			(ColorTarget.ColorBlendOp != BO_Add || ColorTarget.ColorDestBlend != BF_Zero || ColorTarget.ColorSrcBlend != BF_One ||
			ColorTarget.AlphaBlendOp != BO_Add || ColorTarget.AlphaDestBlend != BF_Zero || ColorTarget.AlphaSrcBlend != BF_One) ? VK_TRUE : VK_FALSE;

		BlendState.colorWriteMask = (ColorTarget.ColorWriteMask & CW_RED) ? VK_COLOR_COMPONENT_R_BIT : 0;
		BlendState.colorWriteMask |= (ColorTarget.ColorWriteMask & CW_GREEN) ? VK_COLOR_COMPONENT_G_BIT : 0;
		BlendState.colorWriteMask |= (ColorTarget.ColorWriteMask & CW_BLUE) ? VK_COLOR_COMPONENT_B_BIT : 0;
		BlendState.colorWriteMask |= (ColorTarget.ColorWriteMask & CW_ALPHA) ? VK_COLOR_COMPONENT_A_BIT : 0;
	}
}

FSamplerStateRHIRef FVulkanDynamicRHI::RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	return new FVulkanSamplerState(Initializer, *Device);
}


FRasterizerStateRHIRef FVulkanDynamicRHI::RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	return new FVulkanRasterizerState(Initializer);
}


FDepthStencilStateRHIRef FVulkanDynamicRHI::RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
	return new FVulkanDepthStencilState(Initializer);
}


FBlendStateRHIRef FVulkanDynamicRHI::RHICreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	return new FVulkanBlendState(Initializer);
}
