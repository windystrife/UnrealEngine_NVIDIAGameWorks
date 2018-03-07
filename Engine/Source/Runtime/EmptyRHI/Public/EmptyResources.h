// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyResources.h: Empty resource RHI definitions.
=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FEmptyVertexDeclaration : public FRHIVertexDeclaration
{
public:

	/** Cached element info array (offset, stream index, etc) */
	FVertexDeclarationElementList Elements;

	/** Initialization constructor. */
	FEmptyVertexDeclaration(const FVertexDeclarationElementList& InElements);
};


/**
 * Combined shader state and vertex definition for rendering geometry. 
 * Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
 */
class FEmptyBoundShaderState : public FRHIBoundShaderState
{
public:

	FCachedBoundShaderStateLink CacheLink;

	/** Cached vertex structure */
	TRefCountPtr<FEmptyVertexDeclaration> VertexDeclaration;

	/** Cached shaders */
	TRefCountPtr<FEmptyVertexShader> VertexShader;
	TRefCountPtr<FEmptyPixelShader> PixelShader;
	TRefCountPtr<FEmptyHullShader> HullShader;
	TRefCountPtr<FEmptyDomainShader> DomainShader;
	TRefCountPtr<FEmptyGeometryShader> GeometryShader;

	/** Initialization constructor. */
	FEmptyBoundShaderState(
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI);

	/**
	 *Destructor
	 */
	~FEmptyBoundShaderState();
};


/** Texture/RT wrapper. */
class FEmptySurface
{
public:

	/** 
	 * Constructor that will create Texture and Color/DepthBuffers as needed
	 */
	FEmptySurface(ERHIResourceType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData);

	/**
	 * Destructor
	 */
	~FEmptySurface();

	/**
	 * Locks one of the texture's mip-maps.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 * @return A pointer to the specified texture data.
	 */
	void* Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride);

	/** Unlocks a previously locked mip-map.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 */
	void Unlock(uint32 MipIndex, uint32 ArrayIndex);

	/**
	 * Returns how much memory is used by the surface
	 */
	uint32 GetMemorySize();

protected:

};

class FEmptyTexture2D : public FRHITexture2D
{
public:
	/** The surface info */
	FEmptySurface Surface;

	// Constructor, just calls base and Surface constructor
	FEmptyTexture2D(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 Flags, FResourceBulkDataInterface* BulkData)
		: FRHITexture2D(SizeX, SizeY, NumMips, NumSamples, Format, Flags)
		, Surface(RRT_Texture2D, Format, SizeX, SizeY, 1, /*bArray=*/ false, 1, NumMips, Flags, BulkData)
	{
	}
};

class FEmptyTexture2DArray : public FRHITexture2DArray
{
public:
	/** The surface info */
	FEmptySurface Surface;

	// Constructor, just calls base and Surface constructor
	FEmptyTexture2DArray(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData)
		: FRHITexture2DArray(SizeX, SizeY, ArraySize, NumMips, Format, Flags)
		, Surface(RRT_Texture2DArray, Format, SizeX, SizeY, 1, /*bArray=*/ true, ArraySize, NumMips, Flags, BulkData)
	{
	}
};

class FEmptyTexture3D : public FRHITexture3D
{
public:
	/** The surface info */
	FEmptySurface Surface;

	// Constructor, just calls base and Surface constructor
	FEmptyTexture3D(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData)
		: FRHITexture3D(SizeX, SizeY, SizeZ, NumMips, Format, Flags)
		, Surface(RRT_Texture3D, Format, SizeX, SizeY, SizeZ, /*bArray=*/ false, 1, NumMips, Flags, BulkData)
	{
	}
};

class FEmptyTextureCube : public FRHITextureCube
{
public:
	/** The surface info */
	FEmptySurface Surface;

	// Constructor, just calls base and Surface constructor
	FEmptyTextureCube(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData)
		: FRHITextureCube(Size, NumMips, Format, Flags)
		, Surface(RRT_TextureCube, Format, Size, Size, 6, bArray, ArraySize, NumMips, Flags, BulkData)
	{
	}
};

/** Given a pointer to a RHI texture that was created by the Empty RHI, returns a pointer to the FEmptyTextureBase it encapsulates. */
FEmptySurface& GetSurfaceFromRHITexture(FRHITexture* Texture);





/** Empty occlusion query */
class FEmptyRenderQuery : public FRHIRenderQuery
{
public:

	/** Initialization constructor. */
	FEmptyRenderQuery(ERenderQueryType InQueryType);

	~FEmptyRenderQuery();

	/**
	 * Kick off an occlusion test 
	 */
	void Begin();

	/**
	 * Finish up an occlusion test 
	 */
	void End();

};

/** Index buffer resource class that stores stride information. */
class FEmptyIndexBuffer : public FRHIIndexBuffer
{
public:

	/** Constructor */
	FEmptyIndexBuffer(uint32 InStride, uint32 InSize, uint32 InUsage);

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void* Lock(EResourceLockMode LockMode, uint32 Size=0);

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void Unlock();
};


/** Vertex buffer resource class that stores usage type. */
class FEmptyVertexBuffer : public FRHIVertexBuffer
{
public:

	/** Constructor */
	FEmptyVertexBuffer(uint32 InSize, uint32 InUsage);

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void* Lock(EResourceLockMode LockMode, uint32 Size=0);

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void Unlock();
};

class FEmptyUniformBuffer : public FRHIUniformBuffer
{
public:

	// Constructor
	FEmptyUniformBuffer(const FRHIUniformBufferLayout& InLayout, const void* Contents, uint32 NumBytes, EUniformBufferUsage Usage);

	// Destructor 
	~FEmptyUniformBuffer();
};


class FEmptyStructuredBuffer : public FRHIStructuredBuffer
{
public:
	// Constructor
	FEmptyStructuredBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 InUsage);

	// Destructor
	~FEmptyStructuredBuffer();

};



class FEmptyUnorderedAccessView : public FRHIUnorderedAccessView
{
public:

	// the potential resources to refer to with the UAV object
	TRefCountPtr<FEmptyStructuredBuffer> SourceStructuredBuffer;
	TRefCountPtr<FEmptyVertexBuffer> SourceVertexBuffer;
	TRefCountPtr<FRHITexture> SourceTexture;
};


class FEmptyShaderResourceView : public FRHIShaderResourceView
{
public:

	// The vertex buffer this SRV comes from (can be null)
	TRefCountPtr<FEmptyVertexBuffer> SourceVertexBuffer;

	// The texture that this SRV come from
	TRefCountPtr<FRHITexture> SourceTexture;

	~FEmptyShaderResourceView();
};

template<class T>
struct TEmptyResourceTraits
{
};
template<>
struct TEmptyResourceTraits<FRHIVertexDeclaration>
{
	typedef FEmptyVertexDeclaration TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIVertexShader>
{
	typedef FEmptyVertexShader TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIGeometryShader>
{
	typedef FEmptyGeometryShader TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIHullShader>
{
	typedef FEmptyHullShader TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIDomainShader>
{
	typedef FEmptyDomainShader TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIPixelShader>
{
	typedef FEmptyPixelShader TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIComputeShader>
{
	typedef FEmptyComputeShader TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIBoundShaderState>
{
	typedef FEmptyBoundShaderState TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHITexture3D>
{
	typedef FEmptyTexture3D TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHITexture>
{
	typedef FEmptyTexture TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHITexture2D>
{
	typedef FEmptyTexture2D TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHITexture2DArray>
{
	typedef FEmptyTexture2DArray TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHITextureCube>
{
	typedef FEmptyTextureCube TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIRenderQuery>
{
	typedef FEmptyRenderQuery TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIUniformBuffer>
{
	typedef FEmptyUniformBuffer TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIIndexBuffer>
{
	typedef FEmptyIndexBuffer TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIStructuredBuffer>
{
	typedef FEmptyStructuredBuffer TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIVertexBuffer>
{
	typedef FEmptyVertexBuffer TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIShaderResourceView>
{
	typedef FEmptyShaderResourceView TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIUnorderedAccessView>
{
	typedef FEmptyUnorderedAccessView TConcreteType;
};

template<>
struct TEmptyResourceTraits<FRHISamplerState>
{
	typedef FEmptySamplerState TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIRasterizerState>
{
	typedef FEmptyRasterizerState TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIDepthStencilState>
{
	typedef FEmptyDepthStencilState TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIBlendState>
{
	typedef FEmptyBlendState TConcreteType;
};
template<>
struct TEmptyResourceTraits<FRHIViewport>
{
	typedef FEmptyViewport TConcreteType;
};
