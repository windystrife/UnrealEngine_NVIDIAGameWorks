#pragma once

// NvFlow begin

#include "RHI.h"
#include "RHIResources.h"

class FRHICommandListImmediate;

#include "GridInteractionNvFlow.h"

enum { MAX_NVFLOW_GRIDS = 4 };

struct GridExportParamsNvFlow
{
	FIntVector BlockDim;
	FIntVector BlockDimBits;
	FVector    BlockDimInv;
	FIntVector LinearBlockDim;
	FIntVector LinearBlockOffset;
	FVector    DimInv;
	FVector    VDim;
	FVector    VDimInv;
	FIntVector PoolGridDim;
	FIntVector GridDim;
	bool       IsVTR;
	FMatrix    WorldToVolume;
	float      VelocityScale;

	float GridToParticleAccelTimeConstant;
	float GridToParticleDecelTimeConstant;
	float GridToParticleThresholdMultiplier;

	FShaderResourceViewRHIRef DataSRV;
	FShaderResourceViewRHIRef BlockTableSRV;
};

struct ParticleSimulationParamsNvFlow
{
	TEnumAsByte<enum EInteractionChannelNvFlow> InteractionChannel;
	struct FInteractionResponseContainerNvFlow ResponseToInteractionChannels;

	FBox Bounds;

	int32 TextureSizeX;
	int32 TextureSizeY;
	FTexture2DRHIRef PositionTextureRHI;
	FTexture2DRHIRef VelocityTextureRHI;

	int32 ParticleCount;
	FShaderResourceViewRHIRef VertexBufferSRV;
};

struct GridAccessHooksNvFlow
{
	virtual uint32 NvFlowQueryGridExportParams(FRHICommandListImmediate& RHICmdList, const ParticleSimulationParamsNvFlow& ParticleSimulationParams, uint32 MaxCount, GridExportParamsNvFlow* ResultParamsList) = 0;
};

extern ENGINE_API struct GridAccessHooksNvFlow* GGridAccessNvFlowHooks;

// NvFlow end