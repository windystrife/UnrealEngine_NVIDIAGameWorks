// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyState.h: Empty state definitions.
=============================================================================*/

#pragma once

class FEmptySamplerState : public FRHISamplerState
{
public:
	
	/** 
	 * Constructor
	 */
	FEmptySamplerState(const FSamplerStateInitializerRHI& Initializer);
};

class FEmptyRasterizerState : public FRHIRasterizerState
{
public:

	/**
	 * Constructor
	 */
	FEmptyRasterizerState(const FRasterizerStateInitializerRHI& Initializer);
};

class FEmptyDepthStencilState : public FRHIDepthStencilState
{
public:

	/** 
	 * Constructor
	 */
	FEmptyDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer);
};

class FEmptyBlendState : public FRHIBlendState
{
public:

	/** 
	 * Constructor
	 */
	FEmptyBlendState(const FBlendStateInitializerRHI& Initializer);
};
