// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalResources.h: Metal resource RHI definitions.
=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"
#include "MetalShaderResources.h"

/** Parallel execution is available on Mac but not iOS for the moment - it needs to be tested because it isn't cost-free */
#define METAL_SUPPORTS_PARALLEL_RHI_EXECUTE 1

class FMetalContext;
@class FMetalShaderPipeline;

/** The MTLVertexDescriptor and a pre-calculated hash value used to simplify comparisons (as vendor MTLVertexDescriptor implementations aren't all comparable) */
struct FMetalHashedVertexDescriptor
{
	NSUInteger VertexDescHash;
	MTLVertexDescriptor* VertexDesc;
	
	FMetalHashedVertexDescriptor();
	FMetalHashedVertexDescriptor(MTLVertexDescriptor* Desc, uint32 Hash);
	FMetalHashedVertexDescriptor(FMetalHashedVertexDescriptor const& Other);
	~FMetalHashedVertexDescriptor();
	
	FMetalHashedVertexDescriptor& operator=(FMetalHashedVertexDescriptor const& Other);
	bool operator==(FMetalHashedVertexDescriptor const& Other) const;
	
	friend uint32 GetTypeHash(FMetalHashedVertexDescriptor const& Hash)
	{
		return Hash.VertexDescHash;
	}
};

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FMetalVertexDeclaration : public FRHIVertexDeclaration
{
public:

	/** Initialization constructor. */
	FMetalVertexDeclaration(const FVertexDeclarationElementList& InElements);
	~FMetalVertexDeclaration();
	
	/** Cached element info array (offset, stream index, etc) */
	FVertexDeclarationElementList Elements;

	/** This is the layout for the vertex elements */
	FMetalHashedVertexDescriptor Layout;
	
	/** Hash without considering strides which may be overriden */
	uint32 BaseHash;

protected:
	void GenerateLayout(const FVertexDeclarationElementList& Elements);

};

extern NSString* DecodeMetalSourceCode(uint32 CodeSize, TArray<uint8> const& CompressedSource);

/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
template<typename BaseResourceType, int32 ShaderType>
class TMetalBaseShader : public BaseResourceType, public IRefCountedObject
{
public:
	enum { StaticFrequency = ShaderType };

	/** Initialization constructor. */
	TMetalBaseShader()
	: Function(nil)
	, Library(nil)
	, SideTableBinding(-1)
	, SourceLen(0)
	, SourceCRC(0)
	, GlslCodeNSString(nil)
	, CodeSize(0)
	{
	}
	
	void Init(const TArray<uint8>& InCode, FMetalCodeHeader& Header, id<MTLLibrary> InLibrary = nil);

	/** Destructor */
	virtual ~TMetalBaseShader();

	/** @returns The Metal source code as an NSString if available or nil if not. Will dynamically decompress from compressed data on first invocation. */
	inline NSString* GetSourceCode()
	{
		if (!GlslCodeNSString && CodeSize && CompressedSource.Num())
		{
			GlslCodeNSString = DecodeMetalSourceCode(CodeSize, CompressedSource);
		}
		return GlslCodeNSString;
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
	
	// this is the compiler shader
	id<MTLFunction> Function;
	id<MTLLibrary> Library; // For function-constant specialisation.

	/** External bindings for this shader. */
	FMetalShaderBindings Bindings;

	// List of memory copies from RHIUniformBuffer to packed uniforms
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;
	
	/** The binding for the buffer side-table if present */
	int32 SideTableBinding;

	/** CRC & Len for name disambiguation */
	uint32 SourceLen;
	uint32 SourceCRC;
	
private:
	/** The debuggable text source */
	NSString* GlslCodeNSString;
	
	/** The compressed text source */
	TArray<uint8> CompressedSource;
	
	/** The uncompressed text source size */
	uint32 CodeSize;
};

class FMetalVertexShader : public TMetalBaseShader<FRHIVertexShader, SF_Vertex>
{
public:
	FMetalVertexShader(const TArray<uint8>& InCode);
	FMetalVertexShader(const TArray<uint8>& InCode, id<MTLLibrary> InLibrary);
	
	// for VSHS
	FMetalTessellationOutputs TessellationOutputAttribs;
	float  TessellationMaxTessFactor;
	uint32 TessellationOutputControlPoints;
	uint32 TessellationDomain;
	uint32 TessellationInputControlPoints;
	uint32 TessellationPatchesPerThreadGroup;
	uint32 TessellationPatchCountBuffer;
	uint32 TessellationIndexBuffer;
	uint32 TessellationHSOutBuffer;
	uint32 TessellationHSTFOutBuffer;
	uint32 TessellationControlPointOutBuffer;
	uint32 TessellationControlPointIndexBuffer;
};

class FMetalPixelShader : public TMetalBaseShader<FRHIPixelShader, SF_Pixel>
{
public:
	FMetalPixelShader(const TArray<uint8>& InCode);
	FMetalPixelShader(const TArray<uint8>& InCode, id<MTLLibrary> InLibrary);
};

class FMetalHullShader : public TMetalBaseShader<FRHIHullShader, SF_Hull>
{
public:
	FMetalHullShader(const TArray<uint8>& InCode);
	FMetalHullShader(const TArray<uint8>& InCode, id<MTLLibrary> InLibrary);
};

class FMetalDomainShader : public TMetalBaseShader<FRHIDomainShader, SF_Domain>
{
public:
	FMetalDomainShader(const TArray<uint8>& InCode);
	FMetalDomainShader(const TArray<uint8>& InCode, id<MTLLibrary> InLibrary);
	
	MTLWinding TessellationOutputWinding;
	MTLTessellationPartitionMode TessellationPartitioning;
	uint32 TessellationHSOutBuffer;
	uint32 TessellationControlPointOutBuffer;
};

typedef TMetalBaseShader<FRHIGeometryShader, SF_Geometry> FMetalGeometryShader;

class FMetalComputeShader : public TMetalBaseShader<FRHIComputeShader, SF_Compute>
{
public:
	FMetalComputeShader(const TArray<uint8>& InCode);
	FMetalComputeShader(const TArray<uint8>& InCode, id<MTLLibrary> InLibrary);
	virtual ~FMetalComputeShader();
	
	// the state object for a compute shader
	FMetalShaderPipeline* Pipeline;
	
	// thread group counts
	int32 NumThreadsX;
	int32 NumThreadsY;
	int32 NumThreadsZ;
};

struct FMetalRenderPipelineHash
{
	friend uint32 GetTypeHash(FMetalRenderPipelineHash const& Hash)
	{
		return HashCombine(GetTypeHash(Hash.RasterBits), GetTypeHash(Hash.TargetBits));
	}
	
	friend bool operator==(FMetalRenderPipelineHash const& Left, FMetalRenderPipelineHash const& Right)
	{
		return Left.RasterBits == Right.RasterBits && Left.TargetBits == Right.TargetBits;
	}
	
	uint64 RasterBits;
	uint64 TargetBits;
};

class DEPRECATED(4.15, "Use GraphicsPipelineState Interface") FMetalBoundShaderState : public FRHIBoundShaderState
{
};

enum EMetalIndexType
{
	EMetalIndexType_None   = 0,
	EMetalIndexType_UInt16 = 1,
	EMetalIndexType_UInt32 = 2,
	EMetalIndexType_Num	   = 3
};

class FMetalGraphicsPipelineState : public FRHIGraphicsPipelineState
{
public:
	FMetalGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Init);
	virtual ~FMetalGraphicsPipelineState();

	FMetalShaderPipeline* GetPipeline(EMetalIndexType IndexType)
	{
		check(IndexType < EMetalIndexType_Num && PipelineStates[IndexType]);
		return PipelineStates[IndexType];
	}

	/** Cached vertex structure */
	TRefCountPtr<FMetalVertexDeclaration> VertexDeclaration;
	
	/** Cached shaders */
	TRefCountPtr<FMetalVertexShader> VertexShader;
	TRefCountPtr<FMetalPixelShader> PixelShader;
	TRefCountPtr<FMetalHullShader> HullShader;
	TRefCountPtr<FMetalDomainShader> DomainShader;
	TRefCountPtr<FMetalGeometryShader> GeometryShader;
	
	/** Cached state objects */
	TRefCountPtr<FMetalDepthStencilState> DepthStencilState;
	TRefCountPtr<FMetalRasterizerState> RasterizerState;
	
private:	
	// Tessellation pipelines have three different variations for the indexing-style.
	FMetalShaderPipeline* PipelineStates[EMetalIndexType_Num];
};

class FMetalComputePipelineState : public FRHIComputePipelineState
{
public:
	FMetalComputePipelineState(FMetalComputeShader* InComputeShader)
	: ComputeShader(InComputeShader)
	{
		check(InComputeShader);
	}
	virtual ~FMetalComputePipelineState() {}

	FMetalComputeShader* GetComputeShader()
	{
		return ComputeShader;
	}

private:
	TRefCountPtr<FMetalComputeShader> ComputeShader;
};

/** Texture/RT wrapper. */
class FMetalSurface
{
public:

	/** 
	 * Constructor that will create Texture and Color/DepthBuffers as needed
	 */
	FMetalSurface(ERHIResourceType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumSamples, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData);

	FMetalSurface(FMetalSurface& Source, NSRange MipRange);
	
	FMetalSurface(FMetalSurface& Source, NSRange MipRange, EPixelFormat Format);
	
	/**
	 * Destructor
	 */
	~FMetalSurface();

	/** Prepare for texture-view support - need only call this once on the source texture which is to be viewed. */
	void PrepareTextureView();
	
	/** @returns A newly allocated buffer object large enough for the surface within the texture specified. */
	id<MTLBuffer> AllocSurface(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride);

	/** Apply the data in Buffer to the surface specified. */
	void UpdateSurface(id<MTLBuffer> Buffer, uint32 MipIndex, uint32 ArrayIndex);
	
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
	 * Locks one of the texture's mip-maps.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 * @return A pointer to the specified texture data.
	 */
	void* AsyncLock(class FRHICommandListImmediate& RHICmdList, uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride, bool bNeedsDefaultRHIFlush);
	
	/** Unlocks a previously locked mip-map.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 */
	void AsyncUnlock(class FRHICommandListImmediate& RHICmdList, uint32 MipIndex, uint32 ArrayIndex);

	/**
	 * Returns how much memory a single mip uses, and optionally returns the stride
	 */
	uint32 GetMipSize(uint32 MipIndex, uint32* Stride, bool bSingleLayer);

	/**
	 * Returns how much memory is used by the surface
	 */
	uint32 GetMemorySize();

	/** Returns the number of faces for the texture */
	uint32 GetNumFaces();
	
	/** Gets the drawable texture if this is a back-buffer surface. */
	id<MTLTexture> GetDrawableTexture();

	id<MTLTexture> Reallocate(id<MTLTexture> Texture, MTLTextureUsage UsageModifier);
	void ReplaceTexture(FMetalContext& Context, id<MTLTexture> OldTexture, id<MTLTexture> NewTexture);
	void MakeAliasable(void);
	void MakeUnAliasable(void);
	
	ERHIResourceType Type;
	EPixelFormat PixelFormat;
	uint8 FormatKey;
	//texture used for store actions and binding to shader params
	id<MTLTexture> Texture;
	//if surface is MSAA, texture used to bind for RT
	id<MTLTexture> MSAATexture;

	//texture used for a resolve target.  Same as texture on iOS.  
	//Dummy target on Mac where RHISupportsSeparateMSAAAndResolveTextures is true.	In this case we don't always want a resolve texture but we
	//have to have one until renderpasses are implemented at a high level.
	// Mac / RHISupportsSeparateMSAAAndResolveTextures == true
	// iOS A9+ where depth resolve is available
	// iOS < A9 where depth resolve is unavailable.
	id<MTLTexture> MSAAResolveTexture;
	id<MTLTexture> StencilTexture;
	uint32 SizeX, SizeY, SizeZ;
	bool bIsCubemap;
	int32 volatile Written;
	
	uint32 Flags;
	// one per mip
	id<MTLBuffer> LockedMemory[16];
	uint32 WriteLock;

	// how much memory is allocated for this texture
	uint64 TotalTextureSize;
	
	// For back-buffers, the owning viewport.
	class FMetalViewport* Viewport;
	
	TSet<class FMetalShaderResourceView*> SRVs;

private:
	void Init(FMetalSurface& Source, NSRange MipRange);
	
	void Init(FMetalSurface& Source, NSRange MipRange, EPixelFormat Format);
	
private:
	// The movie playback IOSurface/CVTexture wrapper to avoid page-off
	CFTypeRef ImageSurfaceRef;
	
	// Texture view surfaces don't own their resources, only reference
	bool bTextureView;
	
	// Count of outstanding async. texture uploads
	static volatile int64 ActiveUploads;
};

class FMetalTexture2D : public FRHITexture2D
{
public:
	/** The surface info */
	FMetalSurface Surface;

	// Constructor, just calls base and Surface constructor
	FMetalTexture2D(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITexture2D(SizeX, SizeY, NumMips, NumSamples, Format, Flags, InClearValue)
		, Surface(RRT_Texture2D, Format, SizeX, SizeY, 1, NumSamples, /*bArray=*/ false, 1, NumMips, Flags, BulkData)
	{
	}
	
	virtual ~FMetalTexture2D()
	{
	}
	
	virtual void* GetTextureBaseRHI() override final
	{
		return &Surface;
	}
	
	virtual void* GetNativeResource() const override final
	{
		return Surface.Texture;
	}
};

class FMetalTexture2DArray : public FRHITexture2DArray
{
public:
	/** The surface info */
	FMetalSurface Surface;

	// Constructor, just calls base and Surface constructor
	FMetalTexture2DArray(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITexture2DArray(SizeX, SizeY, ArraySize, NumMips, Format, Flags, InClearValue)
		, Surface(RRT_Texture2DArray, Format, SizeX, SizeY, 1, /*NumSamples=*/1, /*bArray=*/ true, ArraySize, NumMips, Flags, BulkData)
	{
	}
	
	virtual ~FMetalTexture2DArray()
	{
	}
	
	virtual void* GetTextureBaseRHI() override final
	{
		return &Surface;
	}
};

class FMetalTexture3D : public FRHITexture3D
{
public:
	/** The surface info */
	FMetalSurface Surface;

	// Constructor, just calls base and Surface constructor
	FMetalTexture3D(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITexture3D(SizeX, SizeY, SizeZ, NumMips, Format, Flags, InClearValue)
		, Surface(RRT_Texture3D, Format, SizeX, SizeY, SizeZ, /*NumSamples=*/1, /*bArray=*/ false, 1, NumMips, Flags, BulkData)
	{
	}
	
	virtual ~FMetalTexture3D()
	{
	}
	
	virtual void* GetTextureBaseRHI() override final
	{
		return &Surface;
	}
};

class FMetalTextureCube : public FRHITextureCube
{
public:
	/** The surface info */
	FMetalSurface Surface;

	// Constructor, just calls base and Surface constructor
	FMetalTextureCube(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITextureCube(Size, NumMips, Format, Flags, InClearValue)
		, Surface(RRT_TextureCube, Format, Size, Size, 6, /*NumSamples=*/1, bArray, ArraySize, NumMips, Flags, BulkData)
	{
	}
	
	virtual ~FMetalTextureCube()
	{
	}
	
	virtual void* GetTextureBaseRHI() override final
	{
		return &Surface;
	}
};

template<typename T>
class TMetalPtr
{
public:
	TMetalPtr()
	: Object(nil)
	{
		
	}
	
	explicit TMetalPtr(T Obj)
	: Object(Obj)
	{
		
	}
	
	explicit TMetalPtr(TMetalPtr const& Other)
	: Object(nil)
	{
		operator=(Other);
	}
	
	~TMetalPtr()
	{
		[Object release];
	}
	
	TMetalPtr& operator=(TMetalPtr const& Other)
	{
		if (&Other != this)
		{
			[Other.Object retain];
			[Object release];
			Object = Other.Object;
		}
		return *this;
	}
	
	TMetalPtr& operator=(T const& Other)
	{
		if (Other != Object)
		{
			[Other retain];
			[Object release];
			Object = Other;
		}
		return *this;
	}
	
	operator T() const
	{
		return Object;
	}
	
	T operator*() const
	{
		return Object;
	}
	
private:
	T Object;
};

struct MTLCommandBufferRef
{
	MTLCommandBufferRef(id<MTLCommandBuffer> CmdBuf, NSCondition* Event)
	: CommandBuffer(CmdBuf)
	, Condition(Event)
	, bFinished(false)
	{}
	
	TMetalPtr<id<MTLCommandBuffer>> CommandBuffer;
	TMetalPtr<NSCondition*> Condition;
	bool bFinished;
};

struct FMetalCommandBufferFence
{
	bool Wait(uint64 Millis);
	
	TWeakPtr<MTLCommandBufferRef, ESPMode::ThreadSafe> CommandBufferRef;
};

struct FMetalQueryBuffer : public FRHIResource
{
	FMetalQueryBuffer(FMetalContext* InContext, id<MTLBuffer> InBuffer);
	
	virtual ~FMetalQueryBuffer();
	
	uint64 GetResult(uint32 Offset);
	
	TWeakPtr<struct FMetalQueryBufferPool, ESPMode::ThreadSafe> Pool;
	id<MTLBuffer> Buffer;
	uint32 WriteOffset;
};
typedef TRefCountPtr<FMetalQueryBuffer> FMetalQueryBufferRef;

struct FMetalQueryResult
{
	bool Wait(uint64 Millis);
	uint64 GetResult();
	
	FMetalQueryBufferRef SourceBuffer;
	TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe> CommandBufferFence;
	uint32 Offset;
	bool bCompleted;
	bool bBatchFence;
};

/** Metal occlusion query */
class FMetalRenderQuery : public FRHIRenderQuery
{
public:

	/** Initialization constructor. */
	FMetalRenderQuery(ERenderQueryType InQueryType);

	virtual ~FMetalRenderQuery();

	/**
	 * Kick off an occlusion test 
	 */
	void Begin(FMetalContext* Context, TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe> const& BatchFence);

	/**
	 * Finish up an occlusion test 
	 */
	void End(FMetalContext* Context);
	
	// The type of query
	ERenderQueryType Type;

	// Query buffer allocation details as the buffer is already set on the command-encoder
	FMetalQueryResult Buffer;
	
	// Query result.
	volatile uint64 Result;
	
	// Result availability - if not set the first call to acquire it will read the buffer & cache
	volatile bool bAvailable;
};

/** Index buffer resource class that stores stride information. */
class FMetalIndexBuffer : public FRHIIndexBuffer
{
public:

	/** Constructor */
	FMetalIndexBuffer(uint32 InStride, uint32 InSize, uint32 InUsage);
	~FMetalIndexBuffer();
	
	/**
	 * Allocate the index buffer backing store.
	 */
	void Alloc(uint32 InSize);

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void* Lock(EResourceLockMode LockMode, uint32 Offset, uint32 Size=0);

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void Unlock();
	
	// balsa buffer memory
	id<MTLBuffer> Buffer;
	
	// The matching linear texture for this index buffer. 
	id<MTLTexture> LinearTexture;

	// offet into the buffer (for lock usage)
	uint32 LockOffset;
	
	// Lock size
	uint32 LockSize;
	
	// 16- or 32-bit
	MTLIndexType IndexType;
};

@interface FMetalBufferData : FApplePlatformObject<NSObject>
{
@public
	uint8* Data;
	uint32 Len;	
}
-(instancetype)initWithSize:(uint32)Size;
-(instancetype)initWithBytes:(void const*)Data length:(uint32)Size;
@end

/** Vertex buffer resource class that stores usage type. */
class FMetalVertexBuffer : public FRHIVertexBuffer
{
public:

	/** Constructor */
	FMetalVertexBuffer(uint32 InSize, uint32 InUsage);
	~FMetalVertexBuffer();
	
	/**
	 * Allocate the index buffer backing store.
	 */
	void Alloc(uint32 InSize);
	
	/**
	 * Allocate a linear texture for given format.
	 */
	id<MTLTexture> AllocLinearTexture(EPixelFormat Format);
	
	/**
	 * Get a linear texture for given format.
	 */
	id<MTLTexture> GetLinearTexture(EPixelFormat Format);

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void* Lock(EResourceLockMode LockMode, uint32 Offset, uint32 Size=0);

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void Unlock();
	
	// balsa buffer memory
	id<MTLBuffer> Buffer;
	
	// The map of linear textures for this vertex buffer - may be more than one due to type conversion. 
	TMap<EPixelFormat, id<MTLTexture>> LinearTextures;
	
	/** Buffer for small buffers < 4Kb to avoid heap fragmentation. */
	FMetalBufferData* Data;

	// offset into the buffer (for lock usage)
	uint32 LockOffset;
	
	// Sizeof outstanding lock.
	uint32 LockSize;

	// if the buffer is a zero stride buffer, we need to duplicate and grow on demand, this is the size of one element to copy
	uint32 ZeroStrideElementSize;
};

class FMetalUniformBuffer : public FRHIUniformBuffer
{
public:

	// Constructor
	FMetalUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage);

	// Destructor 
	~FMetalUniformBuffer();
	

	bool IsConstantBuffer() const
	{
		return Buffer.length > 0;
	}

	void const* GetData();

	/** The buffer containing the contents - either a fresh buffer or the ring buffer */
	id<MTLBuffer> Buffer;
	
	/** CPU copy of data so that we can defer upload of smaller buffers */
	FMetalBufferData* Data;
	
	/** This offset is used when passing to setVertexBuffer, etc */
	uint32 Offset;

	uint32 Size; // @todo zebra: HACK! This should be removed and the code that uses it should be changed to use GetSize() instead once we fix the problem with FRHIUniformBufferLayout being released too early

	/** The intended usage of the uniform buffer. */
	EUniformBufferUsage Usage;
	
	/** Resource table containing RHI references. */
	TArray<TRefCountPtr<FRHIResource> > ResourceTable;

};


class FMetalStructuredBuffer : public FRHIStructuredBuffer
{
public:
	// Constructor
	FMetalStructuredBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 InUsage);

	// Destructor
	~FMetalStructuredBuffer();

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void* Lock(EResourceLockMode LockMode, uint32 Offset, uint32 Size=0);
	
	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void Unlock();
	
	// offset into the buffer (for lock usage)
	uint32 LockOffset;
	
	// Sizeof outstanding lock.
	uint32 LockSize;
	
	// the actual buffer
	id<MTLBuffer> Buffer;
};


class FMetalShaderResourceView : public FRHIShaderResourceView
{
public:

	// The vertex buffer this SRV comes from (can be null)
	TRefCountPtr<FMetalVertexBuffer> SourceVertexBuffer;
	
	// The index buffer this SRV comes from (can be null)
	TRefCountPtr<FMetalIndexBuffer> SourceIndexBuffer;

	// The texture that this SRV come from
	TRefCountPtr<FRHITexture> SourceTexture;
	
	// The source structured buffer (can be null)
	TRefCountPtr<FMetalStructuredBuffer> SourceStructuredBuffer;
	
	FMetalSurface* TextureView;
	uint8 MipLevel;
	uint8 NumMips;
	uint8 Format;
	uint8 Stride;

	FMetalShaderResourceView();
	~FMetalShaderResourceView();
	
	id<MTLTexture> GetLinearTexture(bool const bUAV);
};



class FMetalUnorderedAccessView : public FRHIUnorderedAccessView
{
public:
	
	// the potential resources to refer to with the UAV object
	TRefCountPtr<FMetalShaderResourceView> SourceView;
};

class FMetalShaderParameterCache
{
public:
	/** Constructor. */
	FMetalShaderParameterCache();

	/** Destructor. */
	~FMetalShaderParameterCache();

	inline void PrepareGlobalUniforms(uint32 TypeIndex, uint32 UniformArraySize)
	{
		if (PackedGlobalUniformsSizes[TypeIndex] < UniformArraySize)
		{
			ResizeGlobalUniforms(TypeIndex, UniformArraySize);
		}
	}

	/**
	 * Marks all uniform arrays as dirty.
	 */
	void MarkAllDirty();

	/**
	 * Sets values directly into the packed uniform array
	 */
	void Set(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValues);

	/**
	 * Commit shader parameters to the currently bound program.
	 */
	void CommitPackedGlobals(class FMetalStateCache* Cache, class FMetalCommandEncoder* Encoder, EShaderFrequency Frequency, const FMetalShaderBindings& Bindings);
	void CommitPackedUniformBuffers(class FMetalStateCache* Cache, TRefCountPtr<FMetalGraphicsPipelineState> BoundShaderState, FMetalComputeShader* ComputeShader, int32 Stage, const TRefCountPtr<FRHIUniformBuffer>* UniformBuffers, const TArray<CrossCompiler::FUniformBufferCopyInfo>& UniformBuffersCopyInfo);

private:
	/** CPU memory block for storing uniform values. */
	uint8* PackedGlobalUniforms[CrossCompiler::PACKED_TYPEINDEX_MAX];

	struct FRange
	{
		uint32	LowVector;
		uint32	HighVector;
	};
	/** Dirty ranges for each uniform array. */
	FRange	PackedGlobalUniformDirty[CrossCompiler::PACKED_TYPEINDEX_MAX];

	uint32 PackedGlobalUniformsSizes[CrossCompiler::PACKED_TYPEINDEX_MAX];

	void ResizeGlobalUniforms(uint32 TypeIndex, uint32 UniformArraySize);
};

class FMetalComputeFence : public FRHIComputeFence
{
public:
	
	FMetalComputeFence(FName InName)
	: FRHIComputeFence(InName)
	, Fence(nil)
	{}
	
	virtual ~FMetalComputeFence()
	{
		[Fence release];
		Fence = nil;
	}
	
	virtual void Reset() final override
	{
		FRHIComputeFence::Reset();
		[Fence release];
		Fence = nil;
	}
	
	void Write(id InFence)
	{
		check(Fence == nil);
		Fence = [InFence retain];
		FRHIComputeFence::WriteFence();
	}
	
	void Wait(FMetalContext& Context);
	
private:
	id Fence;
};

class FMetalShaderLibrary final : public FRHIShaderLibrary
{	
public:
	FMetalShaderLibrary(EShaderPlatform Platform, id<MTLLibrary> Library, FMetalShaderMap const& Map);
	virtual ~FMetalShaderLibrary();
	
	virtual bool IsNativeLibrary() const override final {return true;}
		
	class FMetalShaderLibraryIterator : public FRHIShaderLibrary::FShaderLibraryIterator
	{
	public:
		FMetalShaderLibraryIterator(FMetalShaderLibrary* MetalShaderLibrary) : FShaderLibraryIterator(MetalShaderLibrary), IteratorImpl(MetalShaderLibrary->Map.HashMap.CreateIterator()) {}
		
		virtual bool IsValid() const final override
		{
			return !!IteratorImpl;
		}
		
		virtual FShaderLibraryEntry operator*() const final override;
		virtual FShaderLibraryIterator& operator++() final override
		{
			++IteratorImpl;
			return *this;
		}
		
	private:		
		TMap<FSHAHash, TPair<uint8, TArray<uint8>>>::TIterator IteratorImpl;
	};
	
	virtual TRefCountPtr<FShaderLibraryIterator> CreateIterator(void) final override
	{
		return new FMetalShaderLibraryIterator(this);
	}
	
	virtual uint32 GetShaderCount(void) const final override { return Map.HashMap.Num(); }
	
private:

	friend class FMetalDynamicRHI;
	
	FPixelShaderRHIRef CreatePixelShader(const FSHAHash& Hash);
	FVertexShaderRHIRef CreateVertexShader(const FSHAHash& Hash);
	FHullShaderRHIRef CreateHullShader(const FSHAHash& Hash);
	FDomainShaderRHIRef CreateDomainShader(const FSHAHash& Hash);
	FGeometryShaderRHIRef CreateGeometryShader(const FSHAHash& Hash);
	FGeometryShaderRHIRef CreateGeometryShaderWithStreamOutput(const FSHAHash& Hash, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream);
	FComputeShaderRHIRef CreateComputeShader(const FSHAHash& Hash);
	
private:
	TMetalPtr<id<MTLLibrary>> Library;
	FMetalShaderMap Map;
};

template<class T>
struct TMetalResourceTraits
{
};
template<>
struct TMetalResourceTraits<FRHIShaderLibrary>
{
	typedef FMetalShaderLibrary TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIVertexDeclaration>
{
	typedef FMetalVertexDeclaration TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIVertexShader>
{
	typedef FMetalVertexShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIGeometryShader>
{
	typedef FMetalGeometryShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIHullShader>
{
	typedef FMetalHullShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIDomainShader>
{
	typedef FMetalDomainShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIPixelShader>
{
	typedef FMetalPixelShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIComputeShader>
{
	typedef FMetalComputeShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHITexture3D>
{
	typedef FMetalTexture3D TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHITexture2D>
{
	typedef FMetalTexture2D TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHITexture2DArray>
{
	typedef FMetalTexture2DArray TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHITextureCube>
{
	typedef FMetalTextureCube TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIRenderQuery>
{
	typedef FMetalRenderQuery TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIUniformBuffer>
{
	typedef FMetalUniformBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIIndexBuffer>
{
	typedef FMetalIndexBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIStructuredBuffer>
{
	typedef FMetalStructuredBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIVertexBuffer>
{
	typedef FMetalVertexBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIShaderResourceView>
{
	typedef FMetalShaderResourceView TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIUnorderedAccessView>
{
	typedef FMetalUnorderedAccessView TConcreteType;
};

template<>
struct TMetalResourceTraits<FRHISamplerState>
{
	typedef FMetalSamplerState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIRasterizerState>
{
	typedef FMetalRasterizerState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIDepthStencilState>
{
	typedef FMetalDepthStencilState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIBlendState>
{
	typedef FMetalBlendState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIComputeFence>
{
	typedef FMetalComputeFence TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIGraphicsPipelineState>
{
	typedef FMetalGraphicsPipelineState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIComputePipelineState>
{
	typedef FMetalComputePipelineState TConcreteType;
};
