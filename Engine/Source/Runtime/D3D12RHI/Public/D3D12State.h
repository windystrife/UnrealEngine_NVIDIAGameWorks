// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12State.h: D3D state definitions.
	=============================================================================*/

#pragma once

class FD3D12SamplerState : public FRHISamplerState, public FD3D12DeviceChild, public FD3D12LinkedAdapterObject<FD3D12SamplerState>
{
public:
	D3D12_CPU_DESCRIPTOR_HANDLE Descriptor;
	uint32 DescriptorHeapIndex;
	const uint16 ID;

	FD3D12SamplerState(FD3D12Device* InParent, const D3D12_SAMPLER_DESC& Desc, uint16 SamplerID);
	~FD3D12SamplerState();
};

class FD3D12RasterizerState : public FRHIRasterizerState
{
public:
	D3D12_RASTERIZER_DESC Desc;
};

class FD3D12DepthStencilState : public FRHIDepthStencilState
{
public:

	D3D12_DEPTH_STENCIL_DESC Desc;

	/* Describes the read/write state of the separate depth and stencil components of the DSV. */
	FExclusiveDepthStencil AccessType;
};

class FD3D12BlendState : public FRHIBlendState
{
public:

	D3D12_BLEND_DESC Desc;
};

