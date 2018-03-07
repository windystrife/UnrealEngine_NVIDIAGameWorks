// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticBoundShaderState.cpp: Static bound shader state implementation.
=============================================================================*/

#include "StaticBoundShaderState.h"
#include "RenderingThread.h"
#include "Shader.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

TLinkedList<FGlobalBoundShaderStateResource*>*& FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList()
{
	static TLinkedList<FGlobalBoundShaderStateResource*>* List = NULL;
	return List;
}

FGlobalBoundShaderStateResource::FGlobalBoundShaderStateResource()
	: GlobalListLink(this)
#if DO_CHECK
	, BoundVertexDeclaration(nullptr)
	, BoundVertexShader(nullptr)
	, BoundPixelShader(nullptr)
	, BoundGeometryShader(nullptr)
#endif 
{
	// Add this resource to the global list in the rendering thread.
	if(IsInRenderingThread())
	{
		GlobalListLink.LinkHead(GetGlobalBoundShaderStateList());
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			LinkGlobalBoundShaderStateResource,FGlobalBoundShaderStateResource*,Resource,this,
			{
				Resource->GlobalListLink.LinkHead(GetGlobalBoundShaderStateList());
			});
	}
}

FGlobalBoundShaderStateResource::~FGlobalBoundShaderStateResource()
{
	// Remove this resource from the global list.
	GlobalListLink.Unlink();
}

/**
 * Initializes a global bound shader state with a vanilla bound shader state and required information.
 */
FBoundShaderStateRHIParamRef FGlobalBoundShaderStateResource::GetInitializedRHI(
	FVertexDeclarationRHIParamRef VertexDeclaration,
	FVertexShaderRHIParamRef VertexShader, 
	FPixelShaderRHIParamRef PixelShader,
	FGeometryShaderRHIParamRef GeometryShader
	)
{
	check(IsInitialized());

	// This should only be called in the rendering thread after the RHI has been initialized.
	check(GIsRHIInitialized);
	check(IsInRenderingThread());

	// Create the bound shader state if it hasn't been cached yet.
	if(!IsValidRef(BoundShaderState))
	{
#if DO_CHECK
		BoundVertexDeclaration = VertexDeclaration;
		BoundVertexShader = VertexShader;
		BoundPixelShader = PixelShader;
		BoundGeometryShader = GeometryShader;
#endif 
		BoundShaderState = 
			RHICreateBoundShaderState(
			VertexDeclaration,
			VertexShader,
			FHullShaderRHIRef(), 
			FDomainShaderRHIRef(),
			PixelShader,
			GeometryShader);
	}
#if DO_CHECK
	// Verify that the passed in shaders will actually be used
	// This will catch cases where one bound shader state is being used with more than one combination of shaders
	// Otherwise setting the shader will just silently fail once the bound shader state has been initialized with a different shader 
	check(BoundVertexDeclaration == VertexDeclaration &&
		BoundVertexShader == VertexShader &&
		BoundPixelShader == PixelShader &&
		BoundGeometryShader == GeometryShader);
#endif 

	return BoundShaderState;
}

FBoundShaderStateRHIParamRef FGlobalBoundShaderStateResource::GetPreinitializedRHI()
{
	check(IsInitialized());
	check(GIsRHIInitialized);
	check(IsInParallelRenderingThread());
	return BoundShaderState;
}

void FGlobalBoundShaderStateResource::ReleaseRHI()
{
	// Release the cached bound shader state.
	BoundShaderState.SafeRelease();
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
