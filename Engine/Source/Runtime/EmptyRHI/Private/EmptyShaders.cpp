// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyShaders.cpp: Empty shader RHI implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"


/** Initialization constructor. */
template<typename BaseResourceType>
TEmptyBaseShader<BaseResourceType>::TEmptyBaseShader(const TArray<uint8>& InCode)
{

}


/** Destructor */
template<typename BaseResourceType>
TEmptyBaseShader<BaseResourceType>::~TEmptyBaseShader()
{

}


FVertexShaderRHIRef FEmptyDynamicRHI::RHICreateVertexShader(const TArray<uint8>& Code)
{
	FEmptyVertexShader* Shader = new FEmptyVertexShader(Code);
	return Shader;
}

FPixelShaderRHIRef FEmptyDynamicRHI::RHICreatePixelShader(const TArray<uint8>& Code)
{
	FEmptyPixelShader* Shader = new FEmptyPixelShader(Code);
	return Shader;
}

FHullShaderRHIRef FEmptyDynamicRHI::RHICreateHullShader(const TArray<uint8>& Code) 
{ 
	FEmptyHullShader* Shader = new FEmptyHullShader(Code);
	return Shader;
}

FDomainShaderRHIRef FEmptyDynamicRHI::RHICreateDomainShader(const TArray<uint8>& Code) 
{ 
	FEmptyDomainShader* Shader = new FEmptyDomainShader(Code);
	return Shader;
}

FGeometryShaderRHIRef FEmptyDynamicRHI::RHICreateGeometryShader(const TArray<uint8>& Code) 
{ 
	FEmptyGeometryShader* Shader = new FEmptyGeometryShader(Code);
	return Shader;
}

FGeometryShaderRHIRef FEmptyDynamicRHI::RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList,
	uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	checkf(0, TEXT("Not supported yet"));
	return NULL;
}

FComputeShaderRHIRef FEmptyDynamicRHI::RHICreateComputeShader(const TArray<uint8>& Code) 
{ 
	FEmptyComputeShader* Shader = new FEmptyComputeShader(Code);
	return Shader;
}


FEmptyBoundShaderState::FEmptyBoundShaderState(
			FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
			FVertexShaderRHIParamRef InVertexShaderRHI,
			FPixelShaderRHIParamRef InPixelShaderRHI,
			FHullShaderRHIParamRef InHullShaderRHI,
			FDomainShaderRHIParamRef InDomainShaderRHI,
			FGeometryShaderRHIParamRef InGeometryShaderRHI)
	:	CacheLink(InVertexDeclarationRHI,InVertexShaderRHI,InPixelShaderRHI,InHullShaderRHI,InDomainShaderRHI,InGeometryShaderRHI,this)
{
	FEmptyVertexDeclaration* InVertexDeclaration = FEmptyDynamicRHI::ResourceCast(InVertexDeclarationRHI);
	FEmptyVertexShader* InVertexShader = FEmptyDynamicRHI::ResourceCast(InVertexShaderRHI);
	FEmptyPixelShader* InPixelShader = FEmptyDynamicRHI::ResourceCast(InPixelShaderRHI);
	FEmptyHullShader* InHullShader = FEmptyDynamicRHI::ResourceCast(InHullShaderRHI);
	FEmptyDomainShader* InDomainShader = FEmptyDynamicRHI::ResourceCast(InDomainShaderRHI);
	FEmptyGeometryShader* InGeometryShader = FEmptyDynamicRHI::ResourceCast(InGeometryShaderRHI);

	// cache everything
	VertexDeclaration = InVertexDeclaration;
	VertexShader = InVertexShader;
	PixelShader = InPixelShader;
	HullShader = InHullShader;
	DomainShader = InDomainShader;
	GeometryShader = InGeometryShader;
}

FEmptyBoundShaderState::~FEmptyBoundShaderState()
{

}

FBoundShaderStateRHIRef FEmptyDynamicRHI::RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FHullShaderRHIParamRef HullShaderRHI, 
	FDomainShaderRHIParamRef DomainShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI,
	FGeometryShaderRHIParamRef GeometryShaderRHI
	)
{
	check(IsInRenderingThread());
	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		VertexDeclarationRHI,
		VertexShaderRHI,
		PixelShaderRHI,
		HullShaderRHI,
		DomainShaderRHI,
		GeometryShaderRHI
		);

	if(CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
	else
	{
		return new FEmptyBoundShaderState(VertexDeclarationRHI,VertexShaderRHI,PixelShaderRHI,HullShaderRHI,DomainShaderRHI,GeometryShaderRHI);
	}
}
