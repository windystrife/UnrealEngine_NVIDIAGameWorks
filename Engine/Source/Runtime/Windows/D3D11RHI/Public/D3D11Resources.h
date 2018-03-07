// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11Resources.h: D3D resource RHI definitions.
=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"
#include "D3D11ShaderResources.h"

template <>
struct TTypeTraits<D3D11_INPUT_ELEMENT_DESC> : public TTypeTraitsBase<D3D11_INPUT_ELEMENT_DESC>
{
	enum { IsBytewiseComparable = true };
};

/** Convenience typedef: preallocated array of D3D11 input element descriptions. */
typedef TArray<D3D11_INPUT_ELEMENT_DESC,TFixedAllocator<MaxVertexElementCount> > FD3D11VertexElements;

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FD3D11VertexDeclaration : public FRHIVertexDeclaration
{
public:
	/** Elements of the vertex declaration. */
	FD3D11VertexElements VertexElements;

	uint16 StreamStrides[MaxVertexElementCount];

	/** Initialization constructor. */
	explicit FD3D11VertexDeclaration(const FD3D11VertexElements& InElements, const uint16* InStrides)
		: VertexElements(InElements)
	{
		FMemory::Memcpy(StreamStrides, InStrides, sizeof(StreamStrides));
	}
};

struct FD3D11ShaderData
{
	FD3D11ShaderResourceTable	ShaderResourceTable;
	TArray<FName>				UniformBuffers;
	bool						bShaderNeedsGlobalConstantBuffer;
};

/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
class FD3D11VertexShader : public FRHIVertexShader, public FD3D11ShaderData
{
public:
	enum { StaticFrequency = SF_Vertex };

	/** The vertex shader resource. */
	TRefCountPtr<ID3D11VertexShader> Resource;

	/** The vertex shader's bytecode, with custom data attached. */
	TArray<uint8> Code;

	// TEMP remove with removal of bound shader state
	int32 Offset;
};

class FD3D11GeometryShader : public FRHIGeometryShader, public FD3D11ShaderData
{
public:
	enum { StaticFrequency = SF_Geometry };

	/** The shader resource. */
	TRefCountPtr<ID3D11GeometryShader> Resource;
};

class FD3D11HullShader : public FRHIHullShader, public FD3D11ShaderData
{
public:
	enum { StaticFrequency = SF_Hull };

	/** The shader resource. */
	TRefCountPtr<ID3D11HullShader> Resource;
};

class FD3D11DomainShader : public FRHIDomainShader, public FD3D11ShaderData
{
public:
	enum { StaticFrequency = SF_Domain };

	/** The shader resource. */
	TRefCountPtr<ID3D11DomainShader> Resource;
};

class FD3D11PixelShader : public FRHIPixelShader, public FD3D11ShaderData
{
public:
	enum { StaticFrequency = SF_Pixel };

	/** The shader resource. */
	TRefCountPtr<ID3D11PixelShader> Resource;
};

class FD3D11ComputeShader : public FRHIComputeShader, public FD3D11ShaderData
{
public:
	enum { StaticFrequency = SF_Compute };

	/** The shader resource. */
	TRefCountPtr<ID3D11ComputeShader> Resource;
};

/**
 * Combined shader state and vertex definition for rendering geometry. 
 * Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
 */
class FD3D11BoundShaderState : public FRHIBoundShaderState
{
public:

	FCachedBoundShaderStateLink CacheLink;
	uint16 StreamStrides[MaxVertexElementCount];
	TRefCountPtr<ID3D11InputLayout> InputLayout;
	TRefCountPtr<ID3D11VertexShader> VertexShader;
	TRefCountPtr<ID3D11PixelShader> PixelShader;
	TRefCountPtr<ID3D11HullShader> HullShader;
	TRefCountPtr<ID3D11DomainShader> DomainShader;
	TRefCountPtr<ID3D11GeometryShader> GeometryShader;

	bool bShaderNeedsGlobalConstantBuffer[SF_NumFrequencies];


	/** Initialization constructor. */
	FD3D11BoundShaderState(
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI,
		ID3D11Device* Direct3DDevice
		);

	~FD3D11BoundShaderState();

	/**
	 * Get the shader for the given frequency.
	 */
	FORCEINLINE FD3D11VertexShader*   GetVertexShader() const   { return (FD3D11VertexShader*)CacheLink.GetVertexShader(); }
	FORCEINLINE FD3D11PixelShader*    GetPixelShader() const    { return (FD3D11PixelShader*)CacheLink.GetPixelShader(); }
	FORCEINLINE FD3D11HullShader*     GetHullShader() const     { return (FD3D11HullShader*)CacheLink.GetHullShader(); }
	FORCEINLINE FD3D11DomainShader*   GetDomainShader() const   { return (FD3D11DomainShader*)CacheLink.GetDomainShader(); }
	FORCEINLINE FD3D11GeometryShader* GetGeometryShader() const { return (FD3D11GeometryShader*)CacheLink.GetGeometryShader(); }
};

/** The base class of resources that may be bound as shader resources. */
class FD3D11BaseShaderResource : public IRefCountedObject
{
public:
	FD3D11BaseShaderResource()
		: CurrentGPUAccess(EResourceTransitionAccess::EReadable)
		, LastFrameWritten(-1)
		, bDirty(false)
	{}

	void SetCurrentGPUAccess(EResourceTransitionAccess Access)
	{
		if (Access == EResourceTransitionAccess::EReadable)
		{
			bDirty = false;
		}
		CurrentGPUAccess = Access;
	}

	EResourceTransitionAccess GetCurrentGPUAccess() const
	{
		return CurrentGPUAccess;
	}

	uint32 GetLastFrameWritten() const
	{
		return LastFrameWritten;
	}	

	void SetDirty(bool bInDirty, uint32 CurrentFrame);

	bool IsDirty() const
	{
		return bDirty;
	}

private:
	
	/** Whether the current resource is logically GPU readable or writable.  Mostly for validation for newer RHI's*/
	EResourceTransitionAccess CurrentGPUAccess;

	/** Most recent frame this resource was written to. */
	uint32 LastFrameWritten;

	/** Resource has been written to without a subsequent read barrier.  Mostly for UAVs */
	bool bDirty;
};

/** Texture base class. */
class D3D11RHI_API FD3D11TextureBase : public FD3D11BaseShaderResource
{
public:

	FD3D11TextureBase(
		class FD3D11DynamicRHI* InD3DRHI,
		ID3D11Resource* InResource,
		ID3D11ShaderResourceView* InShaderResourceView,
		int32 InRTVArraySize,
		bool bInCreatedRTVsPerSlice,
		const TArray<TRefCountPtr<ID3D11RenderTargetView> >& InRenderTargetViews,
		TRefCountPtr<ID3D11DepthStencilView>* InDepthStencilViews
		) 
	: D3DRHI(InD3DRHI)
	, IHVResourceHandle(nullptr)
	, MemorySize(0)
	, BaseShaderResource(this)
	, Resource(InResource)
	, ShaderResourceView(InShaderResourceView)
	, RenderTargetViews(InRenderTargetViews)
	, bCreatedRTVsPerSlice(bInCreatedRTVsPerSlice)
	, RTVArraySize(InRTVArraySize)
	, NumDepthStencilViews(0)	
	{
		// Set the DSVs for all the access type combinations
		if ( InDepthStencilViews != nullptr )
		{
			for (uint32 Index = 0; Index < FExclusiveDepthStencil::MaxIndex; Index++)
			{
				DepthStencilViews[Index] = InDepthStencilViews[Index];
				// New Monolithic Graphics drivers have optional "fast calls" replacing various D3d functions
				// You can't use fast version of XXSetShaderResources (called XXSetFastShaderResource) on dynamic or d/s targets
				if ( DepthStencilViews[Index] != NULL )
					NumDepthStencilViews++;
			}
		}
	}

	virtual ~FD3D11TextureBase() {}

	int32 GetMemorySize() const
	{
		return MemorySize;
	}

	void SetMemorySize( int32 InMemorySize )
	{
		MemorySize = InMemorySize;
	}

	// Accessors.
	ID3D11Resource* GetResource() const { return Resource; }
	ID3D11ShaderResourceView* GetShaderResourceView() const { return ShaderResourceView; }
	FD3D11BaseShaderResource* GetBaseShaderResource() const { return BaseShaderResource; }

	void SetIHVResourceHandle(void* InHandle)
	{
		IHVResourceHandle = InHandle;
	}

	void* GetIHVResourceHandle() const
	{
		return IHVResourceHandle;
	}

	/** 
	 * Get the render target view for the specified mip and array slice.
	 * An array slice of -1 is used to indicate that no array slice should be required. 
	 */
	ID3D11RenderTargetView* GetRenderTargetView(int32 MipIndex, int32 ArraySliceIndex) const
	{
		int32 ArrayIndex = MipIndex;

		if (bCreatedRTVsPerSlice)
		{
			check(ArraySliceIndex >= 0);
			ArrayIndex = MipIndex * RTVArraySize + ArraySliceIndex;
		}
		else 
		{
			// Catch attempts to use a specific slice without having created the texture to support it
			check(ArraySliceIndex == -1 || ArraySliceIndex == 0);
		}

		if ((uint32)ArrayIndex < (uint32)RenderTargetViews.Num())
		{
			return RenderTargetViews[ArrayIndex];
		}
		return 0;
	}
	ID3D11DepthStencilView* GetDepthStencilView(FExclusiveDepthStencil AccessType) const
	{ 
		return DepthStencilViews[AccessType.GetIndex()]; 
	}

	// New Monolithic Graphics drivers have optional "fast calls" replacing various D3d functions
	// You can't use fast version of XXSetShaderResources (called XXSetFastShaderResource) on dynamic or d/s targets
	bool HasDepthStencilView()
	{
		return ( NumDepthStencilViews > 0 );
	}	

	void AliasResources(FD3D11TextureBase* Texture)
	{
		check(MemorySize == Texture->MemorySize);
		check(bCreatedRTVsPerSlice == Texture->bCreatedRTVsPerSlice);
		check(RTVArraySize == Texture->RTVArraySize);
		check(NumDepthStencilViews == Texture->NumDepthStencilViews);

		Resource = Texture->Resource;
		BaseShaderResource = Texture->BaseShaderResource;
		ShaderResourceView = Texture->ShaderResourceView;
		RenderTargetViews = Texture->RenderTargetViews;

		for (uint32 Index = 0; Index < NumDepthStencilViews; Index++)
		{
			DepthStencilViews[Index] = Texture->DepthStencilViews[Index];
		}
	}

protected:

	/** The D3D11 RHI that created this texture. */
	FD3D11DynamicRHI* D3DRHI;

	//Resource handle for use by IHVs for SLI and other purposes.
	void* IHVResourceHandle;

	/** Amount of memory allocated by this texture, in bytes. */
	int32 MemorySize;

	/** Pointer to the base shader resource. Usually the object itself, but not for texture references. */
	FD3D11BaseShaderResource* BaseShaderResource;

	/** The texture resource. */
	TRefCountPtr<ID3D11Resource> Resource;

	/** A shader resource view of the texture. */
	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;
	
	/** A render targetable view of the texture. */
	TArray<TRefCountPtr<ID3D11RenderTargetView> > RenderTargetViews;

	bool bCreatedRTVsPerSlice;

	int32 RTVArraySize;

	/** A depth-stencil targetable view of the texture. */
	TRefCountPtr<ID3D11DepthStencilView> DepthStencilViews[FExclusiveDepthStencil::MaxIndex];

	/** Number of Depth Stencil Views - used for fast call tracking. */
	uint32	NumDepthStencilViews;	
};

/** 2D texture (vanilla, cubemap or 2D array) */
template<typename BaseResourceType>
class D3D11RHI_API TD3D11Texture2D : public BaseResourceType, public FD3D11TextureBase
{
public:

	/** Flags used when the texture was created */
	uint32 Flags;

	/** Initialization constructor. */
	TD3D11Texture2D(
		class FD3D11DynamicRHI* InD3DRHI,
		ID3D11Texture2D* InResource,
		ID3D11ShaderResourceView* InShaderResourceView,
		bool bInCreatedRTVsPerSlice,
		int32 InRTVArraySize,
		const TArray<TRefCountPtr<ID3D11RenderTargetView> >& InRenderTargetViews,
		TRefCountPtr<ID3D11DepthStencilView>* InDepthStencilViews,
		uint32 InSizeX,
		uint32 InSizeY,
		uint32 InSizeZ,
		uint32 InNumMips,
		uint32 InNumSamples,
		EPixelFormat InFormat,
		bool bInCubemap,
		uint32 InFlags,
		bool bInPooled,
		const FClearValueBinding& InClearValue
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
		, void* InRawTextureMemory = nullptr
#endif
		)
	: BaseResourceType(
		InSizeX,
		InSizeY,
		InSizeZ,
		InNumMips,
		InNumSamples,
		InFormat,
		InFlags,
		InClearValue
		)
	, FD3D11TextureBase(
		InD3DRHI,
		InResource,
		InShaderResourceView, 
		InRTVArraySize,
		bInCreatedRTVsPerSlice,
		InRenderTargetViews,
		InDepthStencilViews
		)
	, Flags(InFlags)
	, bCubemap(bInCubemap)
	, bPooled(bInPooled)
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	, RawTextureMemory(InRawTextureMemory)
#endif
	{
	}

	virtual ~TD3D11Texture2D();

	/**
	 * Locks one of the texture's mip-maps.
	 * @return A pointer to the specified texture data.
	 */
	void* Lock(uint32 MipIndex,uint32 ArrayIndex,EResourceLockMode LockMode,uint32& DestStride);

	/** Unlocks a previously locked mip-map. */
	void Unlock(uint32 MipIndex,uint32 ArrayIndex);

	// Accessors.
	ID3D11Texture2D* GetResource() const { return (ID3D11Texture2D*)FD3D11TextureBase::GetResource(); }
	bool IsCubemap() const { return bCubemap; }

	/** FRHITexture override.  See FRHITexture::GetNativeResource() */
	virtual void* GetNativeResource() const override final
	{ 
		return GetResource();
	}
	virtual void* GetNativeShaderResourceView() const override final
	{
		return GetShaderResourceView();
	}
	virtual void* GetTextureBaseRHI() override final
	{
		return static_cast<FD3D11TextureBase*>(this);
	}


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
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	void* GetRawTextureMemory() const
	{
		return RawTextureMemory;
	}
#endif

private:

	/** Whether the texture is a cube-map. */
	const uint32 bCubemap : 1;
	/** Whether the texture can be pooled. */
	const uint32 bPooled : 1;
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	void* RawTextureMemory;
#endif
};

/** 3D Texture */
class FD3D11Texture3D : public FRHITexture3D, public FD3D11TextureBase
{
public:

	/** Initialization constructor. */
	FD3D11Texture3D(
		class FD3D11DynamicRHI* InD3DRHI,
		ID3D11Texture3D* InResource,
		ID3D11ShaderResourceView* InShaderResourceView,
		const TArray<TRefCountPtr<ID3D11RenderTargetView> >& InRenderTargetViews,
		uint32 InSizeX,
		uint32 InSizeY,
		uint32 InSizeZ,
		uint32 InNumMips,
		EPixelFormat InFormat,
		uint32 InFlags,
		const FClearValueBinding& InClearValue
		)
	: FRHITexture3D(InSizeX,InSizeY,InSizeZ,InNumMips,InFormat,InFlags,InClearValue)
	, FD3D11TextureBase(
		InD3DRHI,
		InResource,
		InShaderResourceView,
		1,
		false,
		InRenderTargetViews,
		NULL
		)
	{
	}

	virtual ~FD3D11Texture3D();
	
	// Accessors.
	ID3D11Texture3D* GetResource() const { return (ID3D11Texture3D*)FD3D11TextureBase::GetResource(); }
	
	virtual void* GetTextureBaseRHI() override final
	{
		return static_cast<FD3D11TextureBase*>(this);
	}

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

class FD3D11BaseTexture2D : public FRHITexture2D
{
public:
	FD3D11BaseTexture2D(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples, EPixelFormat InFormat, uint32 InFlags, const FClearValueBinding& InClearValue)
	: FRHITexture2D(InSizeX,InSizeY,InNumMips,InNumSamples,InFormat,InFlags, InClearValue)
	{}
	uint32 GetSizeZ() const { return 0; }
};

class FD3D11BaseTexture2DArray : public FRHITexture2DArray
{
public:
	FD3D11BaseTexture2DArray(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples, EPixelFormat InFormat, uint32 InFlags, const FClearValueBinding& InClearValue)
	: FRHITexture2DArray(InSizeX,InSizeY,InSizeZ,InNumMips,InFormat,InFlags,InClearValue)
	{ check(InNumSamples == 1); }
};

class FD3D11BaseTextureCube : public FRHITextureCube
{
public:
	FD3D11BaseTextureCube(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples, EPixelFormat InFormat, uint32 InFlags, const FClearValueBinding& InClearValue)
	: FRHITextureCube(InSizeX,InNumMips,InFormat,InFlags,InClearValue)
	{ check(InNumSamples == 1); }
	uint32 GetSizeX() const { return GetSize(); }
	uint32 GetSizeY() const { return GetSize(); } //-V524
	uint32 GetSizeZ() const { return 0; }
};

typedef TD3D11Texture2D<FRHITexture>              FD3D11Texture;
typedef TD3D11Texture2D<FD3D11BaseTexture2D>      FD3D11Texture2D;
typedef TD3D11Texture2D<FD3D11BaseTexture2DArray> FD3D11Texture2DArray;
typedef TD3D11Texture2D<FD3D11BaseTextureCube>    FD3D11TextureCube;

/** Texture reference class. */
class FD3D11TextureReference : public FRHITextureReference, public FD3D11TextureBase
{
public:
	FD3D11TextureReference(class FD3D11DynamicRHI* InD3DRHI, FLastRenderTimeContainer* LastRenderTime)
		: FRHITextureReference(LastRenderTime)
		, FD3D11TextureBase(InD3DRHI,NULL,NULL, 0, false,TArray<TRefCountPtr<ID3D11RenderTargetView> >(),NULL)
	{
		BaseShaderResource = NULL;
	}

	void SetReferencedTexture(FRHITexture* InTexture, FD3D11BaseShaderResource* InBaseShaderResource, ID3D11ShaderResourceView* InSRV)
	{
		ShaderResourceView = InSRV;
		BaseShaderResource = InBaseShaderResource;
		FRHITextureReference::SetReferencedTexture(InTexture);
	}

	virtual void* GetTextureBaseRHI() override final
	{
		return static_cast<FD3D11TextureBase*>(this);
	}
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

/** Given a pointer to a RHI texture that was created by the D3D11 RHI, returns a pointer to the FD3D11TextureBase it encapsulates. */
FORCEINLINE FD3D11TextureBase* GetD3D11TextureFromRHITexture(FRHITexture* Texture)
{
	if (!Texture)
	{
		return NULL;
	}
	FD3D11TextureBase* Result((FD3D11TextureBase*)Texture->GetTextureBaseRHI());
	check(Result);
	return Result;
}

/** D3D11 render query */
class FD3D11RenderQuery : public FRHIRenderQuery
{
public:

	/** The query resource. */
	TRefCountPtr<ID3D11Query> Resource;

	/** The cached query result. */
	uint64 Result;

	/** true if the query's result is cached. */
	bool bResultIsCached : 1;

	// todo: memory optimize
	ERenderQueryType QueryType;

	/** Initialization constructor. */
	FD3D11RenderQuery(ID3D11Query* InResource, ERenderQueryType InQueryType):
		Resource(InResource),
		Result(0),
		bResultIsCached(false),
		QueryType(InQueryType)
	{}

};

/** Updates tracked stats for a buffer. */
extern void UpdateBufferStats(TRefCountPtr<ID3D11Buffer> Buffer, bool bAllocating);

/** Forward declare the constants ring buffer. */
class FD3D11ConstantsRingBuffer;

/** A ring allocation from the constants ring buffer. */
struct FRingAllocation
{
	ID3D11Buffer* Buffer;
	void* DataPtr;
	uint32 Offset;
	uint32 Size;

	FRingAllocation() : Buffer(NULL) {}
	inline bool IsValid() const { return Buffer != NULL; }
};

/** Uniform buffer resource class. */
class FD3D11UniformBuffer : public FRHIUniformBuffer
{
public:

	/** The D3D11 constant buffer resource */
	TRefCountPtr<ID3D11Buffer> Resource;

	/** Allocation in the constants ring buffer if applicable. */
	FRingAllocation RingAllocation;

	/** Resource table containing RHI references. */
	TArray<TRefCountPtr<FRHIResource> > ResourceTable;

	/** Initialization constructor. */
	FD3D11UniformBuffer(class FD3D11DynamicRHI* InD3D11RHI, const FRHIUniformBufferLayout& InLayout, ID3D11Buffer* InResource,const FRingAllocation& InRingAllocation)
	: FRHIUniformBuffer(InLayout)
	, Resource(InResource)
	, RingAllocation(InRingAllocation)
	, D3D11RHI(InD3D11RHI)
	{}

	virtual ~FD3D11UniformBuffer();

private:
	class FD3D11DynamicRHI* D3D11RHI;
};

/** Index buffer resource class that stores stride information. */
class FD3D11IndexBuffer : public FRHIIndexBuffer, public FD3D11BaseShaderResource
{
public:

	/** The index buffer resource */
	TRefCountPtr<ID3D11Buffer> Resource;

	FD3D11IndexBuffer(ID3D11Buffer* InResource, uint32 InStride, uint32 InSize, uint32 InUsage)
	: FRHIIndexBuffer(InStride,InSize,InUsage)
	, Resource(InResource)
	{}

	virtual ~FD3D11IndexBuffer()
	{
		UpdateBufferStats(Resource, false);
	}

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

/** Structured buffer resource class. */
class FD3D11StructuredBuffer : public FRHIStructuredBuffer, public FD3D11BaseShaderResource
{
public:

	TRefCountPtr<ID3D11Buffer> Resource;

	FD3D11StructuredBuffer(ID3D11Buffer* InResource, uint32 InStride, uint32 InSize, uint32 InUsage)
	: FRHIStructuredBuffer(InStride,InSize,InUsage)
	, Resource(InResource)
	{
		SetCurrentGPUAccess(EResourceTransitionAccess::ERWBarrier);
	}

	virtual ~FD3D11StructuredBuffer()
	{
		UpdateBufferStats(Resource, false);
	}
	
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

/** Vertex buffer resource class. */
class FD3D11VertexBuffer : public FRHIVertexBuffer, public FD3D11BaseShaderResource
{
public:

	TRefCountPtr<ID3D11Buffer> Resource;

	FD3D11VertexBuffer(ID3D11Buffer* InResource, uint32 InSize, uint32 InUsage)
	: FRHIVertexBuffer(InSize,InUsage)
	, Resource(InResource)
	{}

	virtual ~FD3D11VertexBuffer()
	{
		UpdateBufferStats(Resource, false);
	}
	
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

/** Shader resource view class. */
class FD3D11ShaderResourceView : public FRHIShaderResourceView
{
public:
	
	TRefCountPtr<ID3D11ShaderResourceView> View;
	TRefCountPtr<FD3D11BaseShaderResource> Resource;

	FD3D11ShaderResourceView(ID3D11ShaderResourceView* InView,FD3D11BaseShaderResource* InResource)
	: View(InView)
	, Resource(InResource)
	{}
};

/** Unordered access view class. */
class FD3D11UnorderedAccessView : public FRHIUnorderedAccessView
{
public:
	
	TRefCountPtr<ID3D11UnorderedAccessView> View;
	TRefCountPtr<FD3D11BaseShaderResource> Resource;	
	void* IHVResourceHandle;
	
	FD3D11UnorderedAccessView(ID3D11UnorderedAccessView* InView,FD3D11BaseShaderResource* InResource)
	: View(InView)
	, Resource(InResource)
	, IHVResourceHandle(nullptr)
	{}
};

void ReturnPooledTexture2D(int32 MipCount, EPixelFormat PixelFormat, ID3D11Texture2D* InResource);
void ReleasePooledTextures();

template<class T>
struct TD3D11ResourceTraits
{
};
template<>
struct TD3D11ResourceTraits<FRHIVertexDeclaration>
{
	typedef FD3D11VertexDeclaration TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIVertexShader>
{
	typedef FD3D11VertexShader TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIGeometryShader>
{
	typedef FD3D11GeometryShader TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIHullShader>
{
	typedef FD3D11HullShader TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIDomainShader>
{
	typedef FD3D11DomainShader TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIPixelShader>
{
	typedef FD3D11PixelShader TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIComputeShader>
{
	typedef FD3D11ComputeShader TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIBoundShaderState>
{
	typedef FD3D11BoundShaderState TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHITexture3D>
{
	typedef FD3D11Texture3D TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHITexture>
{
	typedef FD3D11Texture TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHITexture2D>
{
	typedef FD3D11Texture2D TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHITexture2DArray>
{
	typedef FD3D11Texture2DArray TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHITextureCube>
{
	typedef FD3D11TextureCube TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIRenderQuery>
{
	typedef FD3D11RenderQuery TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIUniformBuffer>
{
	typedef FD3D11UniformBuffer TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIIndexBuffer>
{
	typedef FD3D11IndexBuffer TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIStructuredBuffer>
{
	typedef FD3D11StructuredBuffer TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIVertexBuffer>
{
	typedef FD3D11VertexBuffer TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIShaderResourceView>
{
	typedef FD3D11ShaderResourceView TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIUnorderedAccessView>
{
	typedef FD3D11UnorderedAccessView TConcreteType;
};

template<>
struct TD3D11ResourceTraits<FRHISamplerState>
{
	typedef FD3D11SamplerState TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIRasterizerState>
{
	typedef FD3D11RasterizerState TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIDepthStencilState>
{
	typedef FD3D11DepthStencilState TConcreteType;
};
template<>
struct TD3D11ResourceTraits<FRHIBlendState>
{
	typedef FD3D11BlendState TConcreteType;
};

