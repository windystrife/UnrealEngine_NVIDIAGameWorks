// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyShaderResources.h: Empty shader resource RHI definitions.
=============================================================================*/

#pragma once

#include "CrossCompilerCommon.h"

/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
template<typename BaseResourceType /*, typename ShaderType */>
class TEmptyBaseShader : public BaseResourceType, public IRefCountedObject
{
public:

	/** Initialization constructor. */
	TEmptyBaseShader()
	{
	}
	TEmptyBaseShader(const TArray<uint8>& InCode);

	/** Destructor */
	virtual ~TEmptyBaseShader();


	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}
};

typedef TEmptyBaseShader<FRHIVertexShader> FEmptyVertexShader;
typedef TEmptyBaseShader<FRHIPixelShader> FEmptyPixelShader;
typedef TEmptyBaseShader<FRHIHullShader> FEmptyHullShader;
typedef TEmptyBaseShader<FRHIDomainShader> FEmptyDomainShader;
typedef TEmptyBaseShader<FRHIComputeShader> FEmptyComputeShader;
typedef TEmptyBaseShader<FRHIGeometryShader> FEmptyGeometryShader;
