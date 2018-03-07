// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12State.cpp: D3D state implementation.
	=============================================================================*/

#include "D3D12RHIPrivate.h"

// MSFT: Need to make sure sampler state is thread safe
// Cache of Sampler States; we store pointers to both as we don't want the TMap to be artificially
// modifying ref counts if not needed; so we manage that ourselves
FCriticalSection GD3D12SamplerStateCacheLock;

static D3D12_TEXTURE_ADDRESS_MODE TranslateAddressMode(ESamplerAddressMode AddressMode)
{
	switch (AddressMode)
	{
	case AM_Clamp: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	case AM_Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	case AM_Border: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	};
}

static D3D12_CULL_MODE TranslateCullMode(ERasterizerCullMode CullMode)
{
	switch (CullMode)
	{
	case CM_CW: return D3D12_CULL_MODE_BACK;
	case CM_CCW: return D3D12_CULL_MODE_FRONT;
	default: return D3D12_CULL_MODE_NONE;
	};
}

static D3D12_FILL_MODE TranslateFillMode(ERasterizerFillMode FillMode)
{
	switch (FillMode)
	{
	case FM_Wireframe: return D3D12_FILL_MODE_WIREFRAME;
	default: return D3D12_FILL_MODE_SOLID;
	};
}

static D3D12_COMPARISON_FUNC TranslateCompareFunction(ECompareFunction CompareFunction)
{
	switch (CompareFunction)
	{
	case CF_Less: return D3D12_COMPARISON_FUNC_LESS;
	case CF_LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case CF_Greater: return D3D12_COMPARISON_FUNC_GREATER;
	case CF_GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	case CF_Equal: return D3D12_COMPARISON_FUNC_EQUAL;
	case CF_NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case CF_Never: return D3D12_COMPARISON_FUNC_NEVER;
	default: return D3D12_COMPARISON_FUNC_ALWAYS;
	};
}

static D3D12_COMPARISON_FUNC TranslateSamplerCompareFunction(ESamplerCompareFunction SamplerComparisonFunction)
{
	switch (SamplerComparisonFunction)
	{
	case SCF_Less: return D3D12_COMPARISON_FUNC_LESS;
	case SCF_Never:
	default: return D3D12_COMPARISON_FUNC_NEVER;
	};
}

static D3D12_STENCIL_OP TranslateStencilOp(EStencilOp StencilOp)
{
	switch (StencilOp)
	{
	case SO_Zero: return D3D12_STENCIL_OP_ZERO;
	case SO_Replace: return D3D12_STENCIL_OP_REPLACE;
	case SO_SaturatedIncrement: return D3D12_STENCIL_OP_INCR_SAT;
	case SO_SaturatedDecrement: return D3D12_STENCIL_OP_DECR_SAT;
	case SO_Invert: return D3D12_STENCIL_OP_INVERT;
	case SO_Increment: return D3D12_STENCIL_OP_INCR;
	case SO_Decrement: return D3D12_STENCIL_OP_DECR;
	default: return D3D12_STENCIL_OP_KEEP;
	};
}

static D3D12_BLEND_OP TranslateBlendOp(EBlendOperation BlendOp)
{
	switch (BlendOp)
	{
	case BO_Subtract: return D3D12_BLEND_OP_SUBTRACT;
	case BO_Min: return D3D12_BLEND_OP_MIN;
	case BO_Max: return D3D12_BLEND_OP_MAX;
	case BO_ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
	default: return D3D12_BLEND_OP_ADD;
	};
}

static D3D12_BLEND TranslateBlendFactor(EBlendFactor BlendFactor)
{
	switch (BlendFactor)
	{
	case BF_One: return D3D12_BLEND_ONE;
	case BF_SourceColor: return D3D12_BLEND_SRC_COLOR;
	case BF_InverseSourceColor: return D3D12_BLEND_INV_SRC_COLOR;
	case BF_SourceAlpha: return D3D12_BLEND_SRC_ALPHA;
	case BF_InverseSourceAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
	case BF_DestAlpha: return D3D12_BLEND_DEST_ALPHA;
	case BF_InverseDestAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
	case BF_DestColor: return D3D12_BLEND_DEST_COLOR;
	case BF_InverseDestColor: return D3D12_BLEND_INV_DEST_COLOR;
	case BF_ConstantBlendFactor: return D3D12_BLEND_BLEND_FACTOR;
	case BF_InverseConstantBlendFactor: return D3D12_BLEND_INV_BLEND_FACTOR;
	default: return D3D12_BLEND_ZERO;
	};
}

bool operator==(const D3D12_SAMPLER_DESC& lhs, const D3D12_SAMPLER_DESC& rhs)
{
	return 0 == memcmp(&lhs, &rhs, sizeof(lhs));
}

uint32 GetTypeHash(const D3D12_SAMPLER_DESC& Desc)
{
	return Desc.Filter;
}

FSamplerStateRHIRef FD3D12DynamicRHI::RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	FD3D12Adapter* Adapter = &GetAdapter();

	return Adapter->CreateLinkedObject<FD3D12SamplerState>([&](FD3D12Device* Device)
	{
		return Device->CreateSampler(Initializer);
	});
}

FD3D12SamplerState* FD3D12Device::CreateSampler(const FSamplerStateInitializerRHI& Initializer)
{
	D3D12_SAMPLER_DESC SamplerDesc;
	FMemory::Memzero(&SamplerDesc, sizeof(D3D12_SAMPLER_DESC));

	SamplerDesc.AddressU = TranslateAddressMode(Initializer.AddressU);
	SamplerDesc.AddressV = TranslateAddressMode(Initializer.AddressV);
	SamplerDesc.AddressW = TranslateAddressMode(Initializer.AddressW);
	SamplerDesc.MipLODBias = Initializer.MipBias;
	SamplerDesc.MaxAnisotropy = ComputeAnisotropyRT(Initializer.MaxAnisotropy);
	SamplerDesc.MinLOD = Initializer.MinMipLevel;
	SamplerDesc.MaxLOD = Initializer.MaxMipLevel;

	// Determine whether we should use one of the comparison modes
	const bool bComparisonEnabled = Initializer.SamplerComparisonFunction != SCF_Never;
	switch (Initializer.Filter)
	{
	case SF_AnisotropicLinear:
	case SF_AnisotropicPoint:
		if (SamplerDesc.MaxAnisotropy == 1)
		{
			SamplerDesc.Filter = bComparisonEnabled ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		}
		else
		{
			// D3D12  doesn't allow using point filtering for mip filter when using anisotropic filtering
			SamplerDesc.Filter = bComparisonEnabled ? D3D12_FILTER_COMPARISON_ANISOTROPIC : D3D12_FILTER_ANISOTROPIC;
		}

		break;
	case SF_Trilinear:
		SamplerDesc.Filter = bComparisonEnabled ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		break;
	case SF_Bilinear:
		SamplerDesc.Filter = bComparisonEnabled ? D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		break;
	case SF_Point:
		SamplerDesc.Filter = bComparisonEnabled ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_POINT;
		break;
	}
	const FLinearColor LinearBorderColor = FColor(Initializer.BorderColor);
	SamplerDesc.BorderColor[0] = LinearBorderColor.R;
	SamplerDesc.BorderColor[1] = LinearBorderColor.G;
	SamplerDesc.BorderColor[2] = LinearBorderColor.B;
	SamplerDesc.BorderColor[3] = LinearBorderColor.A;
	SamplerDesc.ComparisonFunc = TranslateSamplerCompareFunction(Initializer.SamplerComparisonFunction);

	QUICK_SCOPE_CYCLE_COUNTER(FD3D12DynamicRHI_RHICreateSamplerState_LockAndCreate);
	FScopeLock Lock(&GD3D12SamplerStateCacheLock);

	// Check to see if the sampler has already been created
	// This is done to reduce cache misses accessing sampler objects
	TRefCountPtr<FD3D12SamplerState>* PreviouslyCreated = SamplerMap.Find(SamplerDesc);
	if (PreviouslyCreated)
	{
		return PreviouslyCreated->GetReference();
	}
	else
	{
		// 16-bit IDs are used for faster hashing
		check(SamplerID < 0xffff);

		FD3D12SamplerState* NewSampler = new FD3D12SamplerState(this, SamplerDesc, static_cast<uint16>(SamplerID));

		SamplerMap.Add(SamplerDesc, NewSampler);

		SamplerID++;

		return NewSampler;
	}
}

FRasterizerStateRHIRef FD3D12DynamicRHI::RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	FD3D12RasterizerState* RasterizerState = new FD3D12RasterizerState;

	D3D12_RASTERIZER_DESC& RasterizerDesc = RasterizerState->Desc;
	FMemory::Memzero(&RasterizerDesc, sizeof(D3D12_RASTERIZER_DESC));

	RasterizerDesc.CullMode = TranslateCullMode(Initializer.CullMode);
	RasterizerDesc.FillMode = TranslateFillMode(Initializer.FillMode);
	RasterizerDesc.SlopeScaledDepthBias = Initializer.SlopeScaleDepthBias;
	RasterizerDesc.FrontCounterClockwise = true;
	RasterizerDesc.DepthBias = FMath::FloorToInt(Initializer.DepthBias * (float)(1 << 24));
	RasterizerDesc.DepthClipEnable = true;
	RasterizerDesc.MultisampleEnable = Initializer.bAllowMSAA;

	return RasterizerState;
}

FDepthStencilStateRHIRef FD3D12DynamicRHI::RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
	FD3D12DepthStencilState* DepthStencilState = new FD3D12DepthStencilState;

	D3D12_DEPTH_STENCIL_DESC &DepthStencilDesc = DepthStencilState->Desc;
	FMemory::Memzero(&DepthStencilDesc, sizeof(D3D12_DEPTH_STENCIL_DESC));

	// depth part
	DepthStencilDesc.DepthEnable = Initializer.DepthTest != CF_Always || Initializer.bEnableDepthWrite;
	DepthStencilDesc.DepthWriteMask = Initializer.bEnableDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	DepthStencilDesc.DepthFunc = TranslateCompareFunction(Initializer.DepthTest);

	// stencil part
	DepthStencilDesc.StencilEnable = Initializer.bEnableFrontFaceStencil || Initializer.bEnableBackFaceStencil;
	DepthStencilDesc.StencilReadMask = Initializer.StencilReadMask;
	DepthStencilDesc.StencilWriteMask = Initializer.StencilWriteMask;
	DepthStencilDesc.FrontFace.StencilFunc = TranslateCompareFunction(Initializer.FrontFaceStencilTest);
	DepthStencilDesc.FrontFace.StencilFailOp = TranslateStencilOp(Initializer.FrontFaceStencilFailStencilOp);
	DepthStencilDesc.FrontFace.StencilDepthFailOp = TranslateStencilOp(Initializer.FrontFaceDepthFailStencilOp);
	DepthStencilDesc.FrontFace.StencilPassOp = TranslateStencilOp(Initializer.FrontFacePassStencilOp);
	if (Initializer.bEnableBackFaceStencil)
	{
		DepthStencilDesc.BackFace.StencilFunc = TranslateCompareFunction(Initializer.BackFaceStencilTest);
		DepthStencilDesc.BackFace.StencilFailOp = TranslateStencilOp(Initializer.BackFaceStencilFailStencilOp);
		DepthStencilDesc.BackFace.StencilDepthFailOp = TranslateStencilOp(Initializer.BackFaceDepthFailStencilOp);
		DepthStencilDesc.BackFace.StencilPassOp = TranslateStencilOp(Initializer.BackFacePassStencilOp);
	}
	else
	{
		DepthStencilDesc.BackFace = DepthStencilDesc.FrontFace;
	}

	const bool bStencilOpIsKeep =
		Initializer.FrontFaceStencilFailStencilOp == SO_Keep
		&& Initializer.FrontFaceDepthFailStencilOp == SO_Keep
		&& Initializer.FrontFacePassStencilOp == SO_Keep
		&& Initializer.BackFaceStencilFailStencilOp == SO_Keep
		&& Initializer.BackFaceDepthFailStencilOp == SO_Keep
		&& Initializer.BackFacePassStencilOp == SO_Keep;

	const bool bMayWriteStencil = Initializer.StencilWriteMask != 0 && !bStencilOpIsKeep;
	DepthStencilState->AccessType.SetDepthStencilWrite(Initializer.bEnableDepthWrite, bMayWriteStencil);

	return DepthStencilState;
}

FBlendStateRHIRef FD3D12DynamicRHI::RHICreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	FD3D12BlendState* BlendState = new FD3D12BlendState;
	D3D12_BLEND_DESC &BlendDesc = BlendState->Desc;
	FMemory::Memzero(&BlendDesc, sizeof(D3D12_BLEND_DESC));

	BlendDesc.AlphaToCoverageEnable = false;
	BlendDesc.IndependentBlendEnable = Initializer.bUseIndependentRenderTargetBlendStates;

	static_assert(MaxSimultaneousRenderTargets <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT, "Too many MRTs.");
	for (uint32 RenderTargetIndex = 0; RenderTargetIndex < MaxSimultaneousRenderTargets; ++RenderTargetIndex)
	{
		const FBlendStateInitializerRHI::FRenderTarget& RenderTargetInitializer = Initializer.RenderTargets[RenderTargetIndex];
		D3D12_RENDER_TARGET_BLEND_DESC& RenderTarget = BlendDesc.RenderTarget[RenderTargetIndex];
		RenderTarget.BlendEnable =
			RenderTargetInitializer.ColorBlendOp != BO_Add || RenderTargetInitializer.ColorDestBlend != BF_Zero || RenderTargetInitializer.ColorSrcBlend != BF_One ||
			RenderTargetInitializer.AlphaBlendOp != BO_Add || RenderTargetInitializer.AlphaDestBlend != BF_Zero || RenderTargetInitializer.AlphaSrcBlend != BF_One;
		RenderTarget.BlendOp = TranslateBlendOp(RenderTargetInitializer.ColorBlendOp);
		RenderTarget.SrcBlend = TranslateBlendFactor(RenderTargetInitializer.ColorSrcBlend);
		RenderTarget.DestBlend = TranslateBlendFactor(RenderTargetInitializer.ColorDestBlend);
		RenderTarget.BlendOpAlpha = TranslateBlendOp(RenderTargetInitializer.AlphaBlendOp);
		RenderTarget.SrcBlendAlpha = TranslateBlendFactor(RenderTargetInitializer.AlphaSrcBlend);
		RenderTarget.DestBlendAlpha = TranslateBlendFactor(RenderTargetInitializer.AlphaDestBlend);
		RenderTarget.RenderTargetWriteMask =
			((RenderTargetInitializer.ColorWriteMask & CW_RED) ? D3D12_COLOR_WRITE_ENABLE_RED : 0)
			| ((RenderTargetInitializer.ColorWriteMask & CW_GREEN) ? D3D12_COLOR_WRITE_ENABLE_GREEN : 0)
			| ((RenderTargetInitializer.ColorWriteMask & CW_BLUE) ? D3D12_COLOR_WRITE_ENABLE_BLUE : 0)
			| ((RenderTargetInitializer.ColorWriteMask & CW_ALPHA) ? D3D12_COLOR_WRITE_ENABLE_ALPHA : 0);
	}

	return BlendState;
}

FGraphicsPipelineStateRHIRef FD3D12DynamicRHI::RHICreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
{
	FD3D12PipelineStateCache& PSOCache = GetAdapter().GetPSOCache();

	FBoundShaderStateRHIRef BoundShaderState = RHICreateBoundShaderState(
		Initializer.BoundShaderState.VertexDeclarationRHI,
		Initializer.BoundShaderState.VertexShaderRHI,
		Initializer.BoundShaderState.HullShaderRHI,
		Initializer.BoundShaderState.DomainShaderRHI,
		Initializer.BoundShaderState.PixelShaderRHI,
		Initializer.BoundShaderState.GeometryShaderRHI);

	FD3D12HighLevelGraphicsPipelineStateDesc GraphicsDesc = {};

	// Zero the RTV array - this is necessary to prevent uninitialized memory affecting the PSO cache hash generation
	// Note that the above GraphicsDesc = {} does not clear down the array, probably because it's a TStaticArray rather 
	// than a standard C array. 
	FMemory::Memzero(&GraphicsDesc.RTVFormats[0], sizeof(GraphicsDesc.RTVFormats[0]) * GraphicsDesc.RTVFormats.Num());

	GraphicsDesc.BoundShaderState = FD3D12DynamicRHI::ResourceCast(BoundShaderState.GetReference());
	GraphicsDesc.BlendState = &FD3D12DynamicRHI::ResourceCast(Initializer.BlendState)->Desc;
	GraphicsDesc.RasterizerState = &FD3D12DynamicRHI::ResourceCast(Initializer.RasterizerState)->Desc;
	GraphicsDesc.DepthStencilState = &FD3D12DynamicRHI::ResourceCast(Initializer.DepthStencilState)->Desc;
	GraphicsDesc.SampleMask = 0xFFFFFFFF;
	GraphicsDesc.PrimitiveTopologyType = D3D12PrimitiveTypeToTopologyType(TranslatePrimitiveType(Initializer.PrimitiveType));

	TranslateRenderTargetFormats(Initializer, GraphicsDesc.RTVFormats, GraphicsDesc.DSVFormat);
	GraphicsDesc.NumRenderTargets = Initializer.ComputeNumValidRenderTargets();
	GraphicsDesc.SampleDesc.Count = Initializer.NumSamples;
	GraphicsDesc.SampleDesc.Quality = GetMaxMSAAQuality(Initializer.NumSamples);

	// TODO: [PSO API] do we really have to make a new alloc or can we update the RHI to hold/convert to a FxxxRHIRef?
	return new FD3D12GraphicsPipelineState(Initializer, PSOCache.FindGraphics(&GraphicsDesc));
}

FD3D12SamplerState::FD3D12SamplerState(FD3D12Device* InParent, const D3D12_SAMPLER_DESC& Desc, uint16 SamplerID)
	: ID(SamplerID),
	FD3D12DeviceChild(InParent)
{
	Descriptor.ptr = 0;
	FD3D12OfflineDescriptorManager& DescriptorAllocator = GetParentDevice()->GetSamplerDescriptorAllocator();
	Descriptor = DescriptorAllocator.AllocateHeapSlot(DescriptorHeapIndex);

	GetParentDevice()->CreateSamplerInternal(Desc, Descriptor);
}

FD3D12SamplerState::~FD3D12SamplerState()
{
	if (Descriptor.ptr)
	{
		FD3D12OfflineDescriptorManager& DescriptorAllocator = GetParentDevice()->GetSamplerDescriptorAllocator();
		DescriptorAllocator.FreeHeapSlot(Descriptor, DescriptorHeapIndex);
		Descriptor.ptr = 0;
	}
}