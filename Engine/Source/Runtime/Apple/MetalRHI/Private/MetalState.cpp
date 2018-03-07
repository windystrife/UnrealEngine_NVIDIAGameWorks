// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalState.cpp: Metal state implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"
#include "ShaderCache.h"

int32 GMetalUseSamplerCompareFunc = 1;
FAutoConsoleVariableRef CVarMetalUseSamplerCompareFunc(
	TEXT("rhi.Metal.UseSamplerCompareFunc"),
	GMetalUseSamplerCompareFunc,
	TEXT("If true, tries to set the compareFunction on sampler descriptors if it is available. Defaults to 1, set to 0 to disable."),
	ECVF_ReadOnly|ECVF_RenderThreadSafe
	);

/**
 * Translate from UE4 enums to Metal enums (leaving these in Metal just for help in setting them up)
 */
static MTLSamplerMipFilter TranslateMipFilterMode(ESamplerFilter Filter)
{
	switch (Filter)
	{
		case SF_Point:	return MTLSamplerMipFilterNearest;
		default:		return MTLSamplerMipFilterLinear;
	}
}

static MTLSamplerMinMagFilter TranslateFilterMode(ESamplerFilter Filter)
{
	switch (Filter)
	{
		case SF_Point:				return MTLSamplerMinMagFilterNearest;
		case SF_AnisotropicPoint:	return MTLSamplerMinMagFilterLinear;
		case SF_AnisotropicLinear:	return MTLSamplerMinMagFilterLinear;
		default:					return MTLSamplerMinMagFilterLinear;
	}
}

static uint32 GetMetalMaxAnisotropy(ESamplerFilter Filter, uint32 MaxAniso)
{
	switch (Filter)
	{
		case SF_AnisotropicPoint:
		case SF_AnisotropicLinear:	return ComputeAnisotropyRT(MaxAniso);
		default:					return 1;
	}
}

static MTLSamplerMinMagFilter TranslateZFilterMode(ESamplerFilter Filter)
{	switch (Filter)
	{
		case SF_Point:				return MTLSamplerMinMagFilterNearest;
		case SF_AnisotropicPoint:	return MTLSamplerMinMagFilterNearest;
		case SF_AnisotropicLinear:	return MTLSamplerMinMagFilterLinear;
		default:					return MTLSamplerMinMagFilterLinear;
	}
}

static MTLSamplerAddressMode TranslateWrapMode(ESamplerAddressMode AddressMode)
{
	switch (AddressMode)
	{
		case AM_Clamp:	return MTLSamplerAddressModeClampToEdge;
		case AM_Mirror: return MTLSamplerAddressModeMirrorRepeat;
		case AM_Border: return MTLSamplerAddressModeClampToEdge;
		default:		return MTLSamplerAddressModeRepeat;
	}
}

static MTLCompareFunction TranslateCompareFunction(ECompareFunction CompareFunction)
{
	switch(CompareFunction)
	{
		case CF_Less:			return MTLCompareFunctionLess;
		case CF_LessEqual:		return MTLCompareFunctionLessEqual;
		case CF_Greater:		return MTLCompareFunctionGreater;
		case CF_GreaterEqual:	return MTLCompareFunctionGreaterEqual;
		case CF_Equal:			return MTLCompareFunctionEqual;
		case CF_NotEqual:		return MTLCompareFunctionNotEqual;
		case CF_Never:			return MTLCompareFunctionNever;
		default:				return MTLCompareFunctionAlways;
	};
}

static MTLCompareFunction TranslateSamplerCompareFunction(ESamplerCompareFunction SamplerComparisonFunction)
{
	switch(SamplerComparisonFunction)
	{
		case SCF_Less:		return MTLCompareFunctionLess;
		case SCF_Never:		return MTLCompareFunctionNever;
		default:			return MTLCompareFunctionNever;
	};
}

static MTLStencilOperation TranslateStencilOp(EStencilOp StencilOp)
{
	switch(StencilOp)
	{
		case SO_Zero:				return MTLStencilOperationZero;
		case SO_Replace:			return MTLStencilOperationReplace;
		case SO_SaturatedIncrement:	return MTLStencilOperationIncrementClamp;
		case SO_SaturatedDecrement:	return MTLStencilOperationDecrementClamp;
		case SO_Invert:				return MTLStencilOperationInvert;
		case SO_Increment:			return MTLStencilOperationIncrementWrap;
		case SO_Decrement:			return MTLStencilOperationDecrementWrap;
		default:					return MTLStencilOperationKeep;
	};
}

static MTLBlendOperation TranslateBlendOp(EBlendOperation BlendOp)
{
	switch(BlendOp)
	{
		case BO_Subtract:	return MTLBlendOperationSubtract;
		case BO_Min:		return MTLBlendOperationMin;
		case BO_Max:		return MTLBlendOperationMax;
		default:			return MTLBlendOperationAdd;
	};
}


static MTLBlendFactor TranslateBlendFactor(EBlendFactor BlendFactor)
{
	switch(BlendFactor)
	{
		case BF_One:					return MTLBlendFactorOne;
		case BF_SourceColor:			return MTLBlendFactorSourceColor;
		case BF_InverseSourceColor:		return MTLBlendFactorOneMinusSourceColor;
		case BF_SourceAlpha:			return MTLBlendFactorSourceAlpha;
		case BF_InverseSourceAlpha:		return MTLBlendFactorOneMinusSourceAlpha;
		case BF_DestAlpha:				return MTLBlendFactorDestinationAlpha;
		case BF_InverseDestAlpha:		return MTLBlendFactorOneMinusDestinationAlpha;
		case BF_DestColor:				return MTLBlendFactorDestinationColor;
		case BF_InverseDestColor:		return MTLBlendFactorOneMinusDestinationColor;
		default:						return MTLBlendFactorZero;
	};
}

static MTLColorWriteMask TranslateWriteMask(EColorWriteMask WriteMask)
{
	uint32 Result = 0;
	Result |= (WriteMask & CW_RED) ? (MTLColorWriteMaskRed) : 0;
	Result |= (WriteMask & CW_GREEN) ? (MTLColorWriteMaskGreen) : 0;
	Result |= (WriteMask & CW_BLUE) ? (MTLColorWriteMaskBlue) : 0;
	Result |= (WriteMask & CW_ALPHA) ? (MTLColorWriteMaskAlpha) : 0;
	
	return (MTLColorWriteMask)Result;
}


@protocol MTLSamplerCompareDescriptor <NSObject>

- (void) setCompareFunction : (MTLCompareFunction) Func;

@end


FMetalSamplerState::FMetalSamplerState(id<MTLDevice> Device, const FSamplerStateInitializerRHI& Initializer)
{
	MTLSamplerDescriptor* Desc = [[MTLSamplerDescriptor alloc] init];
	
	Desc.minFilter = Desc.magFilter = TranslateFilterMode(Initializer.Filter);
	Desc.mipFilter = TranslateMipFilterMode(Initializer.Filter);
	Desc.maxAnisotropy = GetMetalMaxAnisotropy(Initializer.Filter, Initializer.MaxAnisotropy);
	Desc.sAddressMode = TranslateWrapMode(Initializer.AddressU);
	Desc.tAddressMode = TranslateWrapMode(Initializer.AddressV);
	Desc.rAddressMode = TranslateWrapMode(Initializer.AddressW);
	Desc.lodMinClamp = Initializer.MinMipLevel;
	Desc.lodMaxClamp = Initializer.MaxMipLevel;
	
	static bool bMetalUseSamplerCompareFunc = GMetalUseSamplerCompareFunc && [Desc respondsToSelector:@selector(setCompareFunction:)];
	if(bMetalUseSamplerCompareFunc)
	{
		[(id<MTLSamplerCompareDescriptor>)Desc setCompareFunction: TranslateSamplerCompareFunction(Initializer.SamplerComparisonFunction)];
	}
	
	State = [Device newSamplerStateWithDescriptor:Desc];
	[Desc release];
	TRACK_OBJECT(STAT_MetalSamplerStateCount, State);
}

FMetalSamplerState::~FMetalSamplerState()
{
	UNTRACK_OBJECT(STAT_MetalSamplerStateCount, State);
	[State release];
}

FMetalRasterizerState::FMetalRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	State = Initializer;
}

FMetalRasterizerState::~FMetalRasterizerState()
{
	
}

FMetalDepthStencilState::FMetalDepthStencilState(id<MTLDevice> Device, const FDepthStencilStateInitializerRHI& Initializer)
{
	MTLDepthStencilDescriptor* Desc = [[MTLDepthStencilDescriptor alloc] init];
	Desc.depthCompareFunction = TranslateCompareFunction(Initializer.DepthTest);
	Desc.depthWriteEnabled = Initializer.bEnableDepthWrite;
	
	if (Initializer.bEnableFrontFaceStencil)
	{
		// set up front face stencil operations
		MTLStencilDescriptor* Stencil = [[MTLStencilDescriptor alloc] init];
		Stencil.stencilCompareFunction = TranslateCompareFunction(Initializer.FrontFaceStencilTest);
		Stencil.stencilFailureOperation = TranslateStencilOp(Initializer.FrontFaceStencilFailStencilOp);
		Stencil.depthFailureOperation = TranslateStencilOp(Initializer.FrontFaceDepthFailStencilOp);
		Stencil.depthStencilPassOperation = TranslateStencilOp(Initializer.FrontFacePassStencilOp);
		Stencil.readMask = Initializer.StencilReadMask;
		Stencil.writeMask = Initializer.StencilWriteMask;

		Desc.frontFaceStencil = Stencil;
		[Stencil release];
	}
	
	if (Initializer.bEnableBackFaceStencil)
	{
		// set up back face stencil operations
		MTLStencilDescriptor* Stencil = [[MTLStencilDescriptor alloc] init];
		Stencil.stencilCompareFunction = TranslateCompareFunction(Initializer.BackFaceStencilTest);
		Stencil.stencilFailureOperation = TranslateStencilOp(Initializer.BackFaceStencilFailStencilOp);
		Stencil.depthFailureOperation = TranslateStencilOp(Initializer.BackFaceDepthFailStencilOp);
		Stencil.depthStencilPassOperation = TranslateStencilOp(Initializer.BackFacePassStencilOp);
		Stencil.readMask = Initializer.StencilReadMask;
		Stencil.writeMask = Initializer.StencilWriteMask;

		Desc.backFaceStencil = Stencil;
		
		[Stencil release];
	}
	else if(Initializer.bEnableFrontFaceStencil)
	{
		// set up back face stencil operations to front face in single-face mode
		MTLStencilDescriptor* Stencil = [[MTLStencilDescriptor alloc] init];
		Stencil.stencilCompareFunction = TranslateCompareFunction(Initializer.FrontFaceStencilTest);
		Stencil.stencilFailureOperation = TranslateStencilOp(Initializer.FrontFaceStencilFailStencilOp);
		Stencil.depthFailureOperation = TranslateStencilOp(Initializer.FrontFaceDepthFailStencilOp);
		Stencil.depthStencilPassOperation = TranslateStencilOp(Initializer.FrontFacePassStencilOp);
		Stencil.readMask = Initializer.StencilReadMask;
		Stencil.writeMask = Initializer.StencilWriteMask;
		Desc.backFaceStencil = Stencil;
		[Stencil release];
	}
		
	// bake out the descriptor
	State = [Device newDepthStencilStateWithDescriptor:Desc];
	TRACK_OBJECT(STAT_MetalDepthStencilStateCount, State);
	[Desc release];
	
	// cache some pipeline state info
	bIsDepthWriteEnabled = Initializer.bEnableDepthWrite;
	bIsStencilWriteEnabled = Initializer.bEnableFrontFaceStencil || Initializer.bEnableBackFaceStencil;
}

FMetalDepthStencilState::~FMetalDepthStencilState()
{
	UNTRACK_OBJECT(STAT_MetalDepthStencilStateCount, State);
	[State release];
}



// statics
TMap<uint32, uint8> FMetalBlendState::BlendSettingsToUniqueKeyMap;
uint8 FMetalBlendState::NextKey = 0;
FCriticalSection FMetalBlendState::Mutex;

FMetalBlendState::FMetalBlendState(const FBlendStateInitializerRHI& Initializer)
{
	for(uint32 RenderTargetIndex = 0;RenderTargetIndex < MaxSimultaneousRenderTargets; ++RenderTargetIndex)
	{
		// which initializer to use
		const FBlendStateInitializerRHI::FRenderTarget& Init =
			Initializer.bUseIndependentRenderTargetBlendStates
				? Initializer.RenderTargets[RenderTargetIndex]
				: Initializer.RenderTargets[0];

		// make a new blend state
		MTLRenderPipelineColorAttachmentDescriptor* BlendState = [[MTLRenderPipelineColorAttachmentDescriptor alloc] init];
		TRACK_OBJECT(STAT_MetalRenderPipelineColorAttachmentDescriptor, BlendState);

		// set values
		BlendState.blendingEnabled =
			Init.ColorBlendOp != BO_Add || Init.ColorDestBlend != BF_Zero || Init.ColorSrcBlend != BF_One ||
			Init.AlphaBlendOp != BO_Add || Init.AlphaDestBlend != BF_Zero || Init.AlphaSrcBlend != BF_One;
		BlendState.sourceRGBBlendFactor = TranslateBlendFactor(Init.ColorSrcBlend);
		BlendState.destinationRGBBlendFactor = TranslateBlendFactor(Init.ColorDestBlend);
		BlendState.rgbBlendOperation = TranslateBlendOp(Init.ColorBlendOp);
		BlendState.sourceAlphaBlendFactor = TranslateBlendFactor(Init.AlphaSrcBlend);
		BlendState.destinationAlphaBlendFactor = TranslateBlendFactor(Init.AlphaDestBlend);
		BlendState.alphaBlendOperation = TranslateBlendOp(Init.AlphaBlendOp);
		BlendState.writeMask = TranslateWriteMask(Init.ColorWriteMask);
		
		RenderTargetStates[RenderTargetIndex].BlendState = BlendState;

		// get the unique key
		uint32 BlendBitMask =
			(BlendState.sourceRGBBlendFactor << 0) | (BlendState.destinationRGBBlendFactor << 4) | (BlendState.rgbBlendOperation << 8) |
			(BlendState.sourceAlphaBlendFactor << 11) | (BlendState.destinationAlphaBlendFactor << 15) | (BlendState.alphaBlendOperation << 19) |
			(BlendState.writeMask << 22);
		
		
		if(IsRunningRHIInSeparateThread())
		{
			Mutex.Lock();
		}
		uint8* Key = BlendSettingsToUniqueKeyMap.Find(BlendBitMask);
		if (Key == NULL)
		{
			Key = &BlendSettingsToUniqueKeyMap.Add(BlendBitMask, NextKey++);

			// only giving 5 bits to the key, since we need to pack a bunch of them into 64 bits
			checkf(NextKey < 32, TEXT("Too many unique blend states to fit into the PipelineStateHash"));
		}
		// set the key
		RenderTargetStates[RenderTargetIndex].BlendStateKey = *Key;
		if(IsRunningRHIInSeparateThread())
		{
			Mutex.Unlock();
		}
	}
}

FMetalBlendState::~FMetalBlendState()
{
	for(uint32 RenderTargetIndex = 0;RenderTargetIndex < MaxSimultaneousRenderTargets; ++RenderTargetIndex)
	{
		UNTRACK_OBJECT(STAT_MetalRenderPipelineColorAttachmentDescriptor, RenderTargetStates[RenderTargetIndex].BlendState);
		[RenderTargetStates[RenderTargetIndex].BlendState release];
	}
}





FSamplerStateRHIRef FMetalDynamicRHI::RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
    @autoreleasepool {
	FSamplerStateRHIRef State = new FMetalSamplerState(ImmediateContext.Context->GetDevice(), Initializer);
	return State;
	}
}

FRasterizerStateRHIRef FMetalDynamicRHI::RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	@autoreleasepool {
    FRasterizerStateRHIRef State = new FMetalRasterizerState(Initializer);
	FShaderCache::LogRasterizerState(ImmediateContext.Context->GetCurrentState().GetShaderCacheStateObject(), Initializer, State);
	return State;
	}
}

FDepthStencilStateRHIRef FMetalDynamicRHI::RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
	@autoreleasepool {
	FDepthStencilStateRHIRef State = new FMetalDepthStencilState(ImmediateContext.Context->GetDevice(), Initializer);
	FShaderCache::LogDepthStencilState(ImmediateContext.Context->GetCurrentState().GetShaderCacheStateObject(),Initializer, State);
	return State;
	}
}


FBlendStateRHIRef FMetalDynamicRHI::RHICreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	@autoreleasepool {
	FBlendStateRHIRef State = new FMetalBlendState(Initializer);
	FShaderCache::LogBlendState(ImmediateContext.Context->GetCurrentState().GetShaderCacheStateObject(), Initializer, State);
	return State;
	}
}

