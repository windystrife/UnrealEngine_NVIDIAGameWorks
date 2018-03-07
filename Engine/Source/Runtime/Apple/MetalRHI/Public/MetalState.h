// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalState.h: Metal state definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

class FMetalSamplerState : public FRHISamplerState
{
public:
	
	/** 
	 * Constructor/destructor
	 */
	FMetalSamplerState(id<MTLDevice> Device, const FSamplerStateInitializerRHI& Initializer);
	~FMetalSamplerState();

	id <MTLSamplerState> State;
};

class FMetalRasterizerState : public FRHIRasterizerState
{
public:

	/**
	 * Constructor/destructor
	 */
	FMetalRasterizerState(const FRasterizerStateInitializerRHI& Initializer);
	~FMetalRasterizerState();
	
	FRasterizerStateInitializerRHI State;
};

class FMetalDepthStencilState : public FRHIDepthStencilState
{
public:

	/**
	 * Constructor/destructor
	 */
	FMetalDepthStencilState(id<MTLDevice> Device, const FDepthStencilStateInitializerRHI& Initializer);
	~FMetalDepthStencilState();
	
	id<MTLDepthStencilState> State;
	bool bIsDepthWriteEnabled;
	bool bIsStencilWriteEnabled;
	
};

class FMetalBlendState : public FRHIBlendState
{
public:

	/**
	 * Constructor/destructor
	 */
	FMetalBlendState(const FBlendStateInitializerRHI& Initializer);
	~FMetalBlendState();
	

	struct FBlendPerMRT
	{
		MTLRenderPipelineColorAttachmentDescriptor* BlendState;
		uint8 BlendStateKey;
	};
	FBlendPerMRT RenderTargetStates[MaxSimultaneousRenderTargets];

private:
	// this tracks blend settings (in a bit flag) into a unique key that uses few bits, for PipelineState MRT setup
	static TMap<uint32, uint8> BlendSettingsToUniqueKeyMap;
	static uint8 NextKey;
	static FCriticalSection Mutex;
};
