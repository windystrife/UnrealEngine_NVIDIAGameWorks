// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Containers/Array.h"
#include "Misc/Crc.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Math/Color.h"
#include "Containers/StaticArray.h"
#include "HAL/ThreadSafeCounter.h"
#include "RHIDefinitions.h"
#include "Templates/RefCounting.h"
#include "PixelFormat.h"
#include "Containers/LockFreeList.h"
#include "Misc/SecureHash.h"

#define DISABLE_RHI_DEFFERED_DELETE 0

struct FClearValueBinding;
struct FRHIResourceInfo;
enum class EClearBinding;

/** The base type of RHI resources. */
class RHI_API FRHIResource
{
public:
	FRHIResource(bool InbDoNotDeferDelete = false)
		: MarkedForDelete(0)
		, bDoNotDeferDelete(InbDoNotDeferDelete)
		, bCommitted(true)
	{
	}
	virtual ~FRHIResource() 
	{
		check(PlatformNeedsExtraDeletionLatency() || (NumRefs.GetValue() == 0 && (CurrentlyDeleting == this || bDoNotDeferDelete || Bypass()))); // this should not have any outstanding refs
	}
	FORCEINLINE_DEBUGGABLE uint32 AddRef() const
	{
		int32 NewValue = NumRefs.Increment();
		checkSlow(NewValue > 0); 
		return uint32(NewValue);
	}
	FORCEINLINE_DEBUGGABLE uint32 Release() const
	{
		int32 NewValue = NumRefs.Decrement();
		if (NewValue == 0)
		{
			if (!DeferDelete())
			{ 
				delete this;
			}
			else
			{
				if (FPlatformAtomics::InterlockedCompareExchange(&MarkedForDelete, 1, 0) == 0)
				{
					PendingDeletes.Push(const_cast<FRHIResource*>(this));
				}
			}
		}
		checkSlow(NewValue >= 0);
		return uint32(NewValue);
	}
	FORCEINLINE_DEBUGGABLE uint32 GetRefCount() const
	{
		int32 CurrentValue = NumRefs.GetValue();
		checkSlow(CurrentValue >= 0); 
		return uint32(CurrentValue);
	}
	void DoNoDeferDelete()
	{
		check(!MarkedForDelete);
		bDoNotDeferDelete = true;
		FPlatformMisc::MemoryBarrier();
		check(!MarkedForDelete);
	}

	static void FlushPendingDeletes();

	FORCEINLINE static bool PlatformNeedsExtraDeletionLatency()
	{
		return GRHINeedsExtraDeletionLatency && GIsRHIInitialized;
	}

	static bool Bypass();

	// Transient resource tracking
	// We do this at a high level so we can catch errors even when transient resources are not supported
	void SetCommitted(bool bInCommitted) 
	{ 
		check(IsInRenderingThread()); 
		bCommitted = bInCommitted;
	}
	bool IsCommitted() const 
	{ 
		check(IsInRenderingThread());
		return bCommitted;
	}

private:
	mutable FThreadSafeCounter NumRefs;
	mutable int32 MarkedForDelete;
	bool bDoNotDeferDelete;
	bool bCommitted;

	static TLockFreePointerListUnordered<FRHIResource, PLATFORM_CACHE_LINE_SIZE> PendingDeletes;
	static FRHIResource* CurrentlyDeleting;

	FORCEINLINE bool DeferDelete() const
	{
#if DISABLE_RHI_DEFFERED_DELETE
		return false;
#else
		// Defer if GRHINeedsExtraDeletionLatency or we are doing threaded rendering (unless otherwise requested).
		return !bDoNotDeferDelete && (GRHINeedsExtraDeletionLatency || !Bypass());
#endif
	}

	// Some APIs don't do internal reference counting, so we have to wait an extra couple of frames before deleting resources
	// to ensure the GPU has completely finished with them. This avoids expensive fences, etc.
	struct ResourcesToDelete
	{
		ResourcesToDelete(uint32 InFrameDeleted = 0)
			: FrameDeleted(InFrameDeleted)
		{

		}

		TArray<FRHIResource*>	Resources;
		uint32					FrameDeleted;
	};

	static TArray<ResourcesToDelete> DeferredDeletionQueue;
	static uint32 CurrentFrame;
};


//
// State blocks
//

class FRHISamplerState : public FRHIResource {};
class FRHIRasterizerState : public FRHIResource {};
class FRHIDepthStencilState : public FRHIResource {};
class FRHIBlendState : public FRHIResource {};

//
// Shader bindings
//

class FRHIVertexDeclaration : public FRHIResource {};
class FRHIBoundShaderState : public FRHIResource {};

//
// Shaders
//

class FRHIShader : public FRHIResource
{
public:
	void SetHash(FSHAHash InHash) { Hash = InHash; }
	FSHAHash GetHash() const { return Hash; }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// for debugging only e.g. MaterialName:ShaderFile.usf or ShaderFile.usf/EntryFunc
	FString ShaderName;
#endif

private:
	FSHAHash Hash;
};

class FRHIVertexShader : public FRHIShader {};
class FRHIHullShader : public FRHIShader {};
class FRHIDomainShader : public FRHIShader {};
class FRHIPixelShader : public FRHIShader {};
class FRHIGeometryShader : public FRHIShader {};
class FRHIComputeShader : public FRHIShader {};

//
// Pipeline States
//

class FRHIGraphicsPipelineState : public FRHIResource {};
class FRHIComputePipelineState : public FRHIResource {};

//
// Buffers
//

/** The layout of a uniform buffer in memory. */
struct FRHIUniformBufferLayout
{
	/** The size of the constant buffer in bytes. */
	uint32 ConstantBufferSize;
	/** The offset to the beginning of the resource table. */
	uint32 ResourceOffset;
	/** The type of each resource (EUniformBufferBaseType). */
	TArray<uint8> Resources;

	uint32 GetHash() const
	{
		if (!bComputedHash)
		{
			uint32 TmpHash = ConstantBufferSize << 16;
			// This is to account for 32vs64 bits difference in pointer sizes.
			TmpHash ^= Align(ResourceOffset, 8);
			uint32 N = Resources.Num();
			while (N >= 4)
			{
				TmpHash ^= (Resources[--N] << 0);
				TmpHash ^= (Resources[--N] << 8);
				TmpHash ^= (Resources[--N] << 16);
				TmpHash ^= (Resources[--N] << 24);
			}
			while (N >= 2)
			{
				TmpHash ^= Resources[--N] << 0;
				TmpHash ^= Resources[--N] << 16;
			}
			while (N > 0)
			{
				TmpHash ^= Resources[--N];
			}
			Hash = TmpHash;
			bComputedHash = true;
		}
		return Hash;
	}

	explicit FRHIUniformBufferLayout(FName InName) :
		ConstantBufferSize(0),
		ResourceOffset(0),
		Name(InName),
		Hash(0),
		bComputedHash(false)
	{
	}

	enum EInit
	{
		Zero
	};
	explicit FRHIUniformBufferLayout(EInit) :
		ConstantBufferSize(0),
		ResourceOffset(0),
		Name(FName()),
		Hash(0),
		bComputedHash(false)
	{
	}

	void CopyFrom(const FRHIUniformBufferLayout& Source)
	{
		ConstantBufferSize = Source.ConstantBufferSize;
		ResourceOffset = Source.ResourceOffset;
		Resources = Source.Resources;
		Name = Source.Name;
		Hash = Source.Hash;
		bComputedHash = Source.bComputedHash;
	}

	const FName GetDebugName() const { return Name; }

private:
	// for debugging / error message
	FName Name;

	mutable uint32 Hash;
	mutable bool bComputedHash;
};

/** Compare two uniform buffer layouts. */
inline bool operator==(const FRHIUniformBufferLayout& A, const FRHIUniformBufferLayout& B)
{
	return A.ConstantBufferSize == B.ConstantBufferSize
		&& A.ResourceOffset == B.ResourceOffset
		&& A.Resources == B.Resources;
}

class FRHIUniformBuffer : public FRHIResource
{
public:

	/** Initialization constructor. */
	FRHIUniformBuffer(const FRHIUniformBufferLayout& InLayout)
	: Layout(&InLayout)
	{}

	/** @return The number of bytes in the uniform buffer. */
	uint32 GetSize() const { return Layout->ConstantBufferSize; }
	const FRHIUniformBufferLayout& GetLayout() const { return *Layout; }

private:
	/** Layout of the uniform buffer. */
	const FRHIUniformBufferLayout* Layout;
};

class FRHIIndexBuffer : public FRHIResource
{
public:

	/** Initialization constructor. */
	FRHIIndexBuffer(uint32 InStride,uint32 InSize,uint32 InUsage)
	: Stride(InStride)
	, Size(InSize)
	, Usage(InUsage)
	{}

	/** @return The stride in bytes of the index buffer; must be 2 or 4. */
	uint32 GetStride() const { return Stride; }

	/** @return The number of bytes in the index buffer. */
	uint32 GetSize() const { return Size; }

	/** @return The usage flags used to create the index buffer. */
	uint32 GetUsage() const { return Usage; }

private:
	uint32 Stride;
	uint32 Size;
	uint32 Usage;
};

class FRHIVertexBuffer : public FRHIResource
{
public:

	/**
	 * Initialization constructor.
	 * @apram InUsage e.g. BUF_UnorderedAccess
	 */
	FRHIVertexBuffer(uint32 InSize,uint32 InUsage)
	: Size(InSize)
	, Usage(InUsage)
	{}

	/** @return The number of bytes in the vertex buffer. */
	uint32 GetSize() const { return Size; }

	/** @return The usage flags used to create the vertex buffer. e.g. BUF_UnorderedAccess */
	uint32 GetUsage() const { return Usage; }

private:
	uint32 Size;
	// e.g. BUF_UnorderedAccess
	uint32 Usage;
};

class FRHIStructuredBuffer : public FRHIResource
{
public:

	/** Initialization constructor. */
	FRHIStructuredBuffer(uint32 InStride,uint32 InSize,uint32 InUsage)
	: Stride(InStride)
	, Size(InSize)
	, Usage(InUsage)
	{}

	/** @return The stride in bytes of the structured buffer; must be 2 or 4. */
	uint32 GetStride() const { return Stride; }

	/** @return The number of bytes in the structured buffer. */
	uint32 GetSize() const { return Size; }

	/** @return The usage flags used to create the structured buffer. */
	uint32 GetUsage() const { return Usage; }

private:
	uint32 Stride;
	uint32 Size;
	uint32 Usage;
};

//
// Textures
//

class RHI_API FLastRenderTimeContainer
{
public:
	FLastRenderTimeContainer() : LastRenderTime(-FLT_MAX) {}

	double GetLastRenderTime() const { return LastRenderTime; }
	FORCEINLINE_DEBUGGABLE void SetLastRenderTime(double InLastRenderTime) 
	{ 
		// avoid dirty caches from redundant writes
		if (LastRenderTime != InLastRenderTime)
		{
			LastRenderTime = InLastRenderTime;
		}
	}

private:
	/** The last time the resource was rendered. */
	double LastRenderTime;
};

class RHI_API FRHITexture : public FRHIResource
{
public:
	
	/** Initialization constructor. */
	FRHITexture(uint32 InNumMips, uint32 InNumSamples, EPixelFormat InFormat, uint32 InFlags, FLastRenderTimeContainer* InLastRenderTime, const FClearValueBinding& InClearValue)
	: ClearValue(InClearValue)
	, NumMips(InNumMips)
	, NumSamples(InNumSamples)
	, Format(InFormat)
	, Flags(InFlags)
	, LastRenderTime(InLastRenderTime ? *InLastRenderTime : DefaultLastRenderTime)	
	{}

	// Dynamic cast methods.
	virtual class FRHITexture2D* GetTexture2D() { return NULL; }
	virtual class FRHITexture2DArray* GetTexture2DArray() { return NULL; }
	virtual class FRHITexture3D* GetTexture3D() { return NULL; }
	virtual class FRHITextureCube* GetTextureCube() { return NULL; }
	virtual class FRHITextureReference* GetTextureReference() { return NULL; }
	
	/**
	 * Returns access to the platform-specific native resource pointer.  This is designed to be used to provide plugins with access
	 * to the underlying resource and should be used very carefully or not at all.
	 *
	 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason
	 */
	virtual void* GetNativeResource() const
	{
		// Override this in derived classes to expose access to the native texture resource
		return nullptr;
	}

	/**
	 * Returns access to the platform-specific native shader resource view pointer.  This is designed to be used to provide plugins with access
	 * to the underlying resource and should be used very carefully or not at all.
	 *
	 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason
	 */
	virtual void* GetNativeShaderResourceView() const
	{
		// Override this in derived classes to expose access to the native texture resource
		return nullptr;
	}
	
	/**
	 * Returns access to the platform-specific RHI texture baseclass.  This is designed to provide the RHI with fast access to its base classes in the face of multiple inheritance.
	 * @return	The pointer to the platform-specific RHI texture baseclass or NULL if it not initialized or not supported for this RHI
	 */
	virtual void* GetTextureBaseRHI()
	{
		// Override this in derived classes to expose access to the native texture resource
		return nullptr;
	}

	/** @return The number of mip-maps in the texture. */
	uint32 GetNumMips() const { return NumMips; }

	/** @return The format of the pixels in the texture. */
	EPixelFormat GetFormat() const { return Format; }

	/** @return The flags used to create the texture. */
	uint32 GetFlags() const { return Flags; }

	/* @return the number of samples for multi-sampling. */
	uint32 GetNumSamples() const { return NumSamples; }

	/** @return Whether the texture is multi sampled. */
	bool IsMultisampled() const { return NumSamples > 1; }		

	FRHIResourceInfo ResourceInfo;

	/** sets the last time this texture was cached in a resource table. */
	FORCEINLINE_DEBUGGABLE void SetLastRenderTime(float InLastRenderTime)
	{
		LastRenderTime.SetLastRenderTime(InLastRenderTime);
	}

	/** Returns the last render time container, or NULL if none were specified at creation. */
	FLastRenderTimeContainer* GetLastRenderTimeContainer()
	{
		if (&LastRenderTime == &DefaultLastRenderTime)
		{
			return NULL;
		}
		return &LastRenderTime;
	}

	void SetName(const FName& InName)
	{
		TextureName = InName;
	}

	FName GetName() const
	{
		return TextureName;
	}

	bool HasClearValue() const
	{
		return ClearValue.ColorBinding != EClearBinding::ENoneBound;
	}

	FLinearColor GetClearColor() const
	{
		return ClearValue.GetClearColor();
	}

	void GetDepthStencilClearValue(float& OutDepth, uint32& OutStencil) const
	{
		return ClearValue.GetDepthStencil(OutDepth, OutStencil);
	}

	float GetDepthClearValue() const
	{
		float Depth;
		uint32 Stencil;
		ClearValue.GetDepthStencil(Depth, Stencil);
		return Depth;
	}

	uint32 GetStencilClearValue() const
	{
		float Depth;
		uint32 Stencil;
		ClearValue.GetDepthStencil(Depth, Stencil);
		return Stencil;
	}

	const FClearValueBinding GetClearBinding() const
	{
		return ClearValue;
	}

private:
	FClearValueBinding ClearValue;
	uint32 NumMips;
	uint32 NumSamples;
	EPixelFormat Format;
	uint32 Flags;
	FLastRenderTimeContainer& LastRenderTime;
	FLastRenderTimeContainer DefaultLastRenderTime;	
	FName TextureName;
};

class RHI_API FRHITexture2D : public FRHITexture
{
public:
	
	/** Initialization constructor. */
	FRHITexture2D(uint32 InSizeX,uint32 InSizeY,uint32 InNumMips,uint32 InNumSamples,EPixelFormat InFormat,uint32 InFlags, const FClearValueBinding& InClearValue)
	: FRHITexture(InNumMips, InNumSamples, InFormat, InFlags, NULL, InClearValue)
	, SizeX(InSizeX)
	, SizeY(InSizeY)
	{}
	
	// Dynamic cast methods.
	virtual FRHITexture2D* GetTexture2D() { return this; }

	/** @return The width of the texture. */
	uint32 GetSizeX() const { return SizeX; }
	
	/** @return The height of the texture. */
	uint32 GetSizeY() const { return SizeY; }

	inline FIntPoint GetSizeXY() const
	{
		return FIntPoint(SizeX, SizeY);
	}

private:

	uint32 SizeX;
	uint32 SizeY;
};

class RHI_API FRHITexture2DArray : public FRHITexture
{
public:
	
	/** Initialization constructor. */
	FRHITexture2DArray(uint32 InSizeX,uint32 InSizeY,uint32 InSizeZ,uint32 InNumMips,EPixelFormat InFormat,uint32 InFlags, const FClearValueBinding& InClearValue)
	: FRHITexture(InNumMips,1,InFormat,InFlags,NULL, InClearValue)
	, SizeX(InSizeX)
	, SizeY(InSizeY)
	, SizeZ(InSizeZ)
	{}
	
	// Dynamic cast methods.
	virtual FRHITexture2DArray* GetTexture2DArray() { return this; }
	
	/** @return The width of the textures in the array. */
	uint32 GetSizeX() const { return SizeX; }
	
	/** @return The height of the texture in the array. */
	uint32 GetSizeY() const { return SizeY; }

	/** @return The number of textures in the array. */
	uint32 GetSizeZ() const { return SizeZ; }

private:

	uint32 SizeX;
	uint32 SizeY;
	uint32 SizeZ;
};

class RHI_API FRHITexture3D : public FRHITexture
{
public:
	
	/** Initialization constructor. */
	FRHITexture3D(uint32 InSizeX,uint32 InSizeY,uint32 InSizeZ,uint32 InNumMips,EPixelFormat InFormat,uint32 InFlags, const FClearValueBinding& InClearValue)
	: FRHITexture(InNumMips,1,InFormat,InFlags,NULL, InClearValue)
	, SizeX(InSizeX)
	, SizeY(InSizeY)
	, SizeZ(InSizeZ)
	{}
	
	// Dynamic cast methods.
	virtual FRHITexture3D* GetTexture3D() { return this; }
	
	/** @return The width of the texture. */
	uint32 GetSizeX() const { return SizeX; }
	
	/** @return The height of the texture. */
	uint32 GetSizeY() const { return SizeY; }

	/** @return The depth of the texture. */
	uint32 GetSizeZ() const { return SizeZ; }

private:

	uint32 SizeX;
	uint32 SizeY;
	uint32 SizeZ;
};

class RHI_API FRHITextureCube : public FRHITexture
{
public:
	
	/** Initialization constructor. */
	FRHITextureCube(uint32 InSize,uint32 InNumMips,EPixelFormat InFormat,uint32 InFlags, const FClearValueBinding& InClearValue)
	: FRHITexture(InNumMips,1,InFormat,InFlags,NULL, InClearValue)
	, Size(InSize)
	{}
	
	// Dynamic cast methods.
	virtual FRHITextureCube* GetTextureCube() { return this; }
	
	/** @return The width and height of each face of the cubemap. */
	uint32 GetSize() const { return Size; }

private:

	uint32 Size;
};

class RHI_API FRHITextureReference : public FRHITexture
{
public:
	explicit FRHITextureReference(FLastRenderTimeContainer* InLastRenderTime)
		: FRHITexture(0,0,PF_Unknown,0,InLastRenderTime, FClearValueBinding())
	{}

	virtual FRHITextureReference* GetTextureReference() override { return this; }
	inline FRHITexture* GetReferencedTexture() const { return ReferencedTexture.GetReference(); }

	void SetReferencedTexture(FRHITexture* InTexture)
	{
		ReferencedTexture = InTexture;
	}

private:
	TRefCountPtr<FRHITexture> ReferencedTexture;
};

class RHI_API FRHITextureReferenceNullImpl : public FRHITextureReference
{
public:
	FRHITextureReferenceNullImpl()
		: FRHITextureReference(NULL)
	{}

	void SetReferencedTexture(FRHITexture* InTexture)
	{
		FRHITextureReference::SetReferencedTexture(InTexture);
	}
};

//
// Misc
//

class FRHIRenderQuery : public FRHIResource {};

class FRHIComputeFence : public FRHIResource
{
public:

	FRHIComputeFence(FName InName)
		: Name(InName)
		, bWriteEnqueued(false)
	{}

	FORCEINLINE FName GetName() const
	{
		return Name;
	}

	FORCEINLINE bool GetWriteEnqueued() const
	{
		return bWriteEnqueued;
	}

	virtual void Reset()
	{
		bWriteEnqueued = false;
	}

	virtual void WriteFence()
	{
		ensureMsgf(!bWriteEnqueued, TEXT("ComputeFence: %s already written this frame. You should use a new label"), *Name.ToString());
		bWriteEnqueued = true;
	}

private:
	//debug name of the label.
	FName Name;

	//has the label been written to since being created.
	//check this when queuing waits to catch GPU hangs on the CPU at command creation time.
	bool bWriteEnqueued;
};

class FRHIViewport : public FRHIResource 
{
public:
	/**
	 * Returns access to the platform-specific native resource pointer.  This is designed to be used to provide plugins with access
	 * to the underlying resource and should be used very carefully or not at all.
	 *
	 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason
	 */
	virtual void* GetNativeSwapChain() const { return nullptr; }
	/**
	 * Returns access to the platform-specific native resource pointer to a backbuffer texture.  This is designed to be used to provide plugins with access
	 * to the underlying resource and should be used very carefully or not at all.
	 *
	 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason
	 */
	virtual void* GetNativeBackBufferTexture() const { return nullptr; }
	/**
	 * Returns access to the platform-specific native resource pointer to a backbuffer rendertarget. This is designed to be used to provide plugins with access
	 * to the underlying resource and should be used very carefully or not at all.
	 *
	 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason
	 */
	virtual void* GetNativeBackBufferRT() const { return nullptr; }

	/**
	 * Returns access to the platform-specific native window. This is designed to be used to provide plugins with access
	 * to the underlying resource and should be used very carefully or not at all. 
	 *
	 * @return	The pointer to the native resource or NULL if it not initialized or not supported for this resource type for some reason.
	 * AddParam could represent any additional platform-specific data (could be null).
	 */
	virtual void* GetNativeWindow(void** AddParam = nullptr) const { return nullptr; }

	/**
	 * Sets custom Present handler on the viewport
	 */
	virtual void SetCustomPresent(class FRHICustomPresent*) {}

	/**
	 * Returns currently set custom present handler.
	 */
	virtual class FRHICustomPresent* GetCustomPresent() const { return nullptr; }
};

//
// Views
//

class FRHIUnorderedAccessView : public FRHIResource {};
class FRHIShaderResourceView : public FRHIResource {};

// WaveWorks Start
class FRHIWaveWorks : public FRHIResource
{
public:
	FRHIWaveWorks() : Simulation(nullptr) {}

	virtual void UpdateTick(float SimulationTime) {}
	virtual void SetRenderState(const FMatrix ViewMatrix, const TArray<uint32>& ShaderInputMappings) {}

	// create quad tree
	virtual void CreateQuadTree(struct GFSDK_WaveWorks_Quadtree** OutWaveWorksQuadTreeHandle, 
		int32 MeshDim, float MinPatchLength, uint32 AutoRootLOD, float UpperGridCoverage, 
		float SeaLevel, bool UseTessellation, float TessellationLOD, float GeoMoprhingDegree) {}

	// draw quad tree
	virtual void DrawQuadTree(struct GFSDK_WaveWorks_Quadtree* WaveWorksQuadTreeHandle, 
		FMatrix ViewMatrix, FMatrix ProjMatrix, 
		const TArray<uint32>& ShaderInputMappings) {}

	virtual void DestroyQuadTree(struct GFSDK_WaveWorks_Quadtree* WaveWorksQuadTreeHandle) {}

	// get displacement with sample points
	virtual void GetDisplacements(TArray<FVector> InSamplePoints, FWaveWorksSampleDisplacementsDelegate OnRecieveDisplacementDelegate) {}
	virtual void GetIntersectPointWithRay(FVector InOriginPoint, FVector InDirection, float SeaLevel, FWaveWorksRaycastResultDelegate OnRecieveIntersectPointDelegate) {}

	struct GFSDK_WaveWorks_Simulation* Simulation;
};

typedef FRHIWaveWorks*					FWaveWorksRHIParamRef;
typedef TRefCountPtr<FRHIWaveWorks>		FWaveWorksRHIRef;
// WaveWorks End

typedef FRHISamplerState*              FSamplerStateRHIParamRef;
typedef TRefCountPtr<FRHISamplerState> FSamplerStateRHIRef;

typedef FRHIRasterizerState*              FRasterizerStateRHIParamRef;
typedef TRefCountPtr<FRHIRasterizerState> FRasterizerStateRHIRef;

typedef FRHIDepthStencilState*              FDepthStencilStateRHIParamRef;
typedef TRefCountPtr<FRHIDepthStencilState> FDepthStencilStateRHIRef;

typedef FRHIBlendState*              FBlendStateRHIParamRef;
typedef TRefCountPtr<FRHIBlendState> FBlendStateRHIRef;

typedef FRHIVertexDeclaration*              FVertexDeclarationRHIParamRef;
typedef TRefCountPtr<FRHIVertexDeclaration> FVertexDeclarationRHIRef;

typedef FRHIVertexShader*              FVertexShaderRHIParamRef;
typedef TRefCountPtr<FRHIVertexShader> FVertexShaderRHIRef;

typedef FRHIHullShader*              FHullShaderRHIParamRef;
typedef TRefCountPtr<FRHIHullShader> FHullShaderRHIRef;

typedef FRHIDomainShader*              FDomainShaderRHIParamRef;
typedef TRefCountPtr<FRHIDomainShader> FDomainShaderRHIRef;

typedef FRHIPixelShader*              FPixelShaderRHIParamRef;
typedef TRefCountPtr<FRHIPixelShader> FPixelShaderRHIRef;

typedef FRHIGeometryShader*              FGeometryShaderRHIParamRef;
typedef TRefCountPtr<FRHIGeometryShader> FGeometryShaderRHIRef;

typedef FRHIComputeShader*              FComputeShaderRHIParamRef;
typedef TRefCountPtr<FRHIComputeShader> FComputeShaderRHIRef;

typedef FRHIComputeFence*				FComputeFenceRHIParamRef;
typedef TRefCountPtr<FRHIComputeFence>	FComputeFenceRHIRef;

typedef FRHIBoundShaderState*              FBoundShaderStateRHIParamRef;
typedef TRefCountPtr<FRHIBoundShaderState> FBoundShaderStateRHIRef;

typedef FRHIUniformBuffer*              FUniformBufferRHIParamRef;
typedef TRefCountPtr<FRHIUniformBuffer> FUniformBufferRHIRef;

typedef FRHIIndexBuffer*              FIndexBufferRHIParamRef;
typedef TRefCountPtr<FRHIIndexBuffer> FIndexBufferRHIRef;

typedef FRHIVertexBuffer*              FVertexBufferRHIParamRef;
typedef TRefCountPtr<FRHIVertexBuffer> FVertexBufferRHIRef;

typedef FRHIStructuredBuffer*              FStructuredBufferRHIParamRef;
typedef TRefCountPtr<FRHIStructuredBuffer> FStructuredBufferRHIRef;

typedef FRHITexture*              FTextureRHIParamRef;
typedef TRefCountPtr<FRHITexture> FTextureRHIRef;

typedef FRHITexture2D*              FTexture2DRHIParamRef;
typedef TRefCountPtr<FRHITexture2D> FTexture2DRHIRef;

typedef FRHITexture2DArray*              FTexture2DArrayRHIParamRef;
typedef TRefCountPtr<FRHITexture2DArray> FTexture2DArrayRHIRef;

typedef FRHITexture3D*              FTexture3DRHIParamRef;
typedef TRefCountPtr<FRHITexture3D> FTexture3DRHIRef;

typedef FRHITextureCube*              FTextureCubeRHIParamRef;
typedef TRefCountPtr<FRHITextureCube> FTextureCubeRHIRef;

typedef FRHITextureReference*              FTextureReferenceRHIParamRef;
typedef TRefCountPtr<FRHITextureReference> FTextureReferenceRHIRef;

typedef FRHIRenderQuery*              FRenderQueryRHIParamRef;
typedef TRefCountPtr<FRHIRenderQuery> FRenderQueryRHIRef;

typedef FRHIViewport*              FViewportRHIParamRef;
typedef TRefCountPtr<FRHIViewport> FViewportRHIRef;

typedef FRHIUnorderedAccessView*              FUnorderedAccessViewRHIParamRef;
typedef TRefCountPtr<FRHIUnorderedAccessView> FUnorderedAccessViewRHIRef;

typedef FRHIShaderResourceView*              FShaderResourceViewRHIParamRef;
typedef TRefCountPtr<FRHIShaderResourceView> FShaderResourceViewRHIRef;

typedef FRHIGraphicsPipelineState*              FGraphicsPipelineStateRHIParamRef;
typedef TRefCountPtr<FRHIGraphicsPipelineState> FGraphicsPipelineStateRHIRef;


class FRHIRenderTargetView
{
public:
	FTextureRHIParamRef Texture;
	uint32 MipIndex;

	/** Array slice or texture cube face.  Only valid if texture resource was created with TexCreate_TargetArraySlicesIndependently! */
	uint32 ArraySliceIndex;
	
	ERenderTargetLoadAction LoadAction;
	ERenderTargetStoreAction StoreAction;

	FRHIRenderTargetView() : 
		Texture(NULL),
		MipIndex(0),
		ArraySliceIndex(-1),
		LoadAction(ERenderTargetLoadAction::ENoAction),
		StoreAction(ERenderTargetStoreAction::ENoAction)
	{}

	FRHIRenderTargetView(const FRHIRenderTargetView& Other) :
		Texture(Other.Texture),
		MipIndex(Other.MipIndex),
		ArraySliceIndex(Other.ArraySliceIndex),
		LoadAction(Other.LoadAction),
		StoreAction(Other.StoreAction)
	{}

	//common case
	explicit FRHIRenderTargetView(FTextureRHIParamRef InTexture, ERenderTargetLoadAction InLoadAction) :
		Texture(InTexture),
		MipIndex(0),
		ArraySliceIndex(-1),
		LoadAction(InLoadAction),
		StoreAction(ERenderTargetStoreAction::EStore)
	{}

	//common case
	explicit FRHIRenderTargetView(FTextureRHIParamRef InTexture, ERenderTargetLoadAction InLoadAction, uint32 InMipIndex, uint32 InArraySliceIndex) :
		Texture(InTexture),
		MipIndex(InMipIndex),
		ArraySliceIndex(InArraySliceIndex),
		LoadAction(InLoadAction),
		StoreAction(ERenderTargetStoreAction::EStore)
	{}
	
	explicit FRHIRenderTargetView(FTextureRHIParamRef InTexture, uint32 InMipIndex, uint32 InArraySliceIndex, ERenderTargetLoadAction InLoadAction, ERenderTargetStoreAction InStoreAction) :
		Texture(InTexture),
		MipIndex(InMipIndex),
		ArraySliceIndex(InArraySliceIndex),
		LoadAction(InLoadAction),
		StoreAction(InStoreAction)
	{}

	bool operator==(const FRHIRenderTargetView& Other) const
	{
		return 
			Texture == Other.Texture &&
			MipIndex == Other.MipIndex &&
			ArraySliceIndex == Other.ArraySliceIndex &&
			LoadAction == Other.LoadAction &&
			StoreAction == Other.StoreAction;
	}
};

class FExclusiveDepthStencil
{
public:
	enum Type
	{
		// don't use those directly, use the combined versions below
		// 4 bits are used for depth and 4 for stencil to make the hex value readable and non overlapping
		DepthNop =		0x00,
		DepthRead =		0x01,
		DepthWrite =	0x02,
		DepthMask =		0x0f,
		StencilNop =	0x00,
		StencilRead =	0x10,
		StencilWrite =	0x20,
		StencilMask =	0xf0,

		// use those:
		DepthNop_StencilNop = DepthNop + StencilNop,
		DepthRead_StencilNop = DepthRead + StencilNop,
		DepthWrite_StencilNop = DepthWrite + StencilNop,
		DepthNop_StencilRead = DepthNop + StencilRead,
		DepthRead_StencilRead = DepthRead + StencilRead,
		DepthWrite_StencilRead = DepthWrite + StencilRead,
		DepthNop_StencilWrite = DepthNop + StencilWrite,
		DepthRead_StencilWrite = DepthRead + StencilWrite,
		DepthWrite_StencilWrite = DepthWrite + StencilWrite,
	};

private:
	Type Value;

public:
	// constructor
	FExclusiveDepthStencil(Type InValue = DepthNop_StencilNop)
		: Value(InValue)
	{
	}

	inline bool IsUsingDepthStencil() const
	{
		return Value != DepthNop_StencilNop;
	}
	inline bool IsUsingDepth() const
	{
		return (ExtractDepth() != DepthNop);
	}
	inline bool IsUsingStencil() const
	{
		return (ExtractStencil() != StencilNop);
	}
	inline bool IsDepthWrite() const
	{
		return ExtractDepth() == DepthWrite;
	}
	inline bool IsStencilWrite() const
	{
		return ExtractStencil() == StencilWrite;
	}

	inline bool IsAnyWrite() const
	{
		return IsDepthWrite() || IsStencilWrite();
	}

	inline void SetDepthWrite()
	{
		Value = (Type)(ExtractStencil() | DepthWrite);
	}
	inline void SetStencilWrite()
	{
		Value = (Type)(ExtractDepth() | StencilWrite);
	}
	inline void SetDepthStencilWrite(bool bDepth, bool bStencil)
	{
		Value = DepthNop_StencilNop;

		if (bDepth)
		{
			SetDepthWrite();
		}
		if (bStencil)
		{
			SetStencilWrite();
		}
	}
	bool operator==(const FExclusiveDepthStencil& rhs) const
	{
		return Value == rhs.Value;
	}
	inline bool IsValid(FExclusiveDepthStencil& Current) const
	{
		Type Depth = ExtractDepth();

		if (Depth != DepthNop && Depth != Current.ExtractDepth())
		{
			return false;
		}

		Type Stencil = ExtractStencil();

		if (Stencil != StencilNop && Stencil != Current.ExtractStencil())
		{
			return false;
		}

		return true;
	}

	uint32 GetIndex() const
	{
		// Note: The array to index has views created in that specific order.

		// we don't care about the Nop versions so less views are needed
		// we combine Nop and Write
		switch (Value)
		{
			case DepthWrite_StencilNop:
			case DepthNop_StencilWrite:
			case DepthWrite_StencilWrite:
			case DepthNop_StencilNop:
				return 0; // old DSAT_Writable
		
			case DepthRead_StencilNop:
			case DepthRead_StencilWrite:
				return 1; // old DSAT_ReadOnlyDepth

			case DepthNop_StencilRead:
			case DepthWrite_StencilRead:
				return 2; // old DSAT_ReadOnlyStencil

			case DepthRead_StencilRead:
				return 3; // old DSAT_ReadOnlyDepthAndStencil
		}
		// should never happen
		check(0);
		return -1;
	}
	static const uint32 MaxIndex = 4;

private:
	inline Type ExtractDepth() const
	{
		return (Type)(Value & DepthMask);
	}
	inline Type ExtractStencil() const
	{
		return (Type)(Value & StencilMask);
	}
};

class FRHIDepthRenderTargetView
{
public:
	FTextureRHIParamRef Texture;

	ERenderTargetLoadAction		DepthLoadAction;
	ERenderTargetStoreAction	DepthStoreAction;
	ERenderTargetLoadAction		StencilLoadAction;

private:
	ERenderTargetStoreAction	StencilStoreAction;
	FExclusiveDepthStencil		DepthStencilAccess;
public:

	// accessor to prevent write access to StencilStoreAction
	ERenderTargetStoreAction GetStencilStoreAction() const { return StencilStoreAction; }
	// accessor to prevent write access to DepthStencilAccess
	FExclusiveDepthStencil GetDepthStencilAccess() const { return DepthStencilAccess; }

	explicit FRHIDepthRenderTargetView() :
		Texture(nullptr),
		DepthLoadAction(ERenderTargetLoadAction::ENoAction),
		DepthStoreAction(ERenderTargetStoreAction::ENoAction),
		StencilLoadAction(ERenderTargetLoadAction::ENoAction),
		StencilStoreAction(ERenderTargetStoreAction::ENoAction),
		DepthStencilAccess(FExclusiveDepthStencil::DepthNop_StencilNop)
	{
		Validate();
	}

	//common case
	explicit FRHIDepthRenderTargetView(FTextureRHIParamRef InTexture, ERenderTargetLoadAction InLoadAction, ERenderTargetStoreAction InStoreAction) :
		Texture(InTexture),
		DepthLoadAction(InLoadAction),
		DepthStoreAction(InStoreAction),
		StencilLoadAction(InLoadAction),
		StencilStoreAction(InStoreAction),
		DepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		Validate();
	}

	explicit FRHIDepthRenderTargetView(FTextureRHIParamRef InTexture, ERenderTargetLoadAction InLoadAction, ERenderTargetStoreAction InStoreAction, FExclusiveDepthStencil InDepthStencilAccess) :
		Texture(InTexture),
		DepthLoadAction(InLoadAction),
		DepthStoreAction(InStoreAction),
		StencilLoadAction(InLoadAction),
		StencilStoreAction(InStoreAction),
		DepthStencilAccess(InDepthStencilAccess)
	{
		Validate();
	}

	explicit FRHIDepthRenderTargetView(FTextureRHIParamRef InTexture, ERenderTargetLoadAction InDepthLoadAction, ERenderTargetStoreAction InDepthStoreAction, ERenderTargetLoadAction InStencilLoadAction, ERenderTargetStoreAction InStencilStoreAction) :
		Texture(InTexture),
		DepthLoadAction(InDepthLoadAction),
		DepthStoreAction(InDepthStoreAction),
		StencilLoadAction(InStencilLoadAction),
		StencilStoreAction(InStencilStoreAction),
		DepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite)
	{
		Validate();
	}

	explicit FRHIDepthRenderTargetView(FTextureRHIParamRef InTexture, ERenderTargetLoadAction InDepthLoadAction, ERenderTargetStoreAction InDepthStoreAction, ERenderTargetLoadAction InStencilLoadAction, ERenderTargetStoreAction InStencilStoreAction, FExclusiveDepthStencil InDepthStencilAccess) :
		Texture(InTexture),
		DepthLoadAction(InDepthLoadAction),
		DepthStoreAction(InDepthStoreAction),
		StencilLoadAction(InStencilLoadAction),
		StencilStoreAction(InStencilStoreAction),
		DepthStencilAccess(InDepthStencilAccess)
	{
		Validate();
	}

	void Validate() const
	{		
		ensureMsgf(DepthStencilAccess.IsDepthWrite() || DepthStoreAction == ERenderTargetStoreAction::ENoAction, TEXT("Depth is read-only, but we are performing a store.  This is a waste on mobile.  If depth can't change, we don't need to store it out again"));
		ensureMsgf(DepthStencilAccess.IsStencilWrite() || StencilStoreAction == ERenderTargetStoreAction::ENoAction, TEXT("Stencil is read-only, but we are performing a store.  This is a waste on mobile.  If stencil can't change, we don't need to store it out again"));
	}

	bool operator==(const FRHIDepthRenderTargetView& Other) const
	{
		return
			Texture == Other.Texture &&
			DepthLoadAction == Other.DepthLoadAction &&
			DepthStoreAction == Other.DepthStoreAction &&
			StencilLoadAction == Other.StencilLoadAction &&
			StencilStoreAction == Other.StencilStoreAction &&
			DepthStencilAccess == Other.DepthStencilAccess;
	}
};

class FRHISetRenderTargetsInfo
{
public:
	// Color Render Targets Info
	FRHIRenderTargetView ColorRenderTarget[MaxSimultaneousRenderTargets];	
	int32 NumColorRenderTargets;
	bool bClearColor;

	// Depth/Stencil Render Target Info
	FRHIDepthRenderTargetView DepthStencilRenderTarget;	
	bool bClearDepth;
	bool bClearStencil;

	// UAVs info.
	FUnorderedAccessViewRHIRef UnorderedAccessView[MaxSimultaneousUAVs];
	int32 NumUAVs;

	FRHISetRenderTargetsInfo() :
		NumColorRenderTargets(0),
		bClearColor(false),		
		bClearDepth(false),
		bClearStencil(false),
		NumUAVs(0)
	{}

	FRHISetRenderTargetsInfo(int32 InNumColorRenderTargets, const FRHIRenderTargetView* InColorRenderTargets, const FRHIDepthRenderTargetView& InDepthStencilRenderTarget) :
		NumColorRenderTargets(InNumColorRenderTargets),
		bClearColor(InNumColorRenderTargets > 0 && InColorRenderTargets[0].LoadAction == ERenderTargetLoadAction::EClear),
		DepthStencilRenderTarget(InDepthStencilRenderTarget),		
		bClearDepth(InDepthStencilRenderTarget.Texture && InDepthStencilRenderTarget.DepthLoadAction == ERenderTargetLoadAction::EClear),
		bClearStencil(InDepthStencilRenderTarget.Texture && InDepthStencilRenderTarget.StencilLoadAction == ERenderTargetLoadAction::EClear),
		NumUAVs(0)
	{
		check(InNumColorRenderTargets <= 0 || InColorRenderTargets);
		for (int32 Index = 0; Index < InNumColorRenderTargets; ++Index)
		{
			ColorRenderTarget[Index] = InColorRenderTargets[Index];			
		}
	}
	// @todo metal mrt: This can go away after all the cleanup is done
	void SetClearDepthStencil(bool bInClearDepth, bool bInClearStencil = false)
	{
		if (bInClearDepth)
		{
			DepthStencilRenderTarget.DepthLoadAction = ERenderTargetLoadAction::EClear;
		}
		if (bInClearStencil)
		{
			DepthStencilRenderTarget.StencilLoadAction = ERenderTargetLoadAction::EClear;
		}
		bClearDepth = bInClearDepth;		
		bClearStencil = bInClearStencil;		
	}

	uint32 CalculateHash() const
	{
		// Need a separate struct so we can memzero/remove dependencies on reference counts
		struct FHashableStruct
		{
			// Depth goes in the last slot
			FRHITexture* Texture[MaxSimultaneousRenderTargets + 1];
			uint32 MipIndex[MaxSimultaneousRenderTargets];
			uint32 ArraySliceIndex[MaxSimultaneousRenderTargets];
			ERenderTargetLoadAction LoadAction[MaxSimultaneousRenderTargets];
			ERenderTargetStoreAction StoreAction[MaxSimultaneousRenderTargets];

			ERenderTargetLoadAction		DepthLoadAction;
			ERenderTargetStoreAction	DepthStoreAction;
			ERenderTargetLoadAction		StencilLoadAction;
			ERenderTargetStoreAction	StencilStoreAction;
			FExclusiveDepthStencil		DepthStencilAccess;

			bool bClearDepth;
			bool bClearStencil;
			bool bClearColor;
			FRHIUnorderedAccessView* UnorderedAccessView[MaxSimultaneousUAVs];

			void Set(const FRHISetRenderTargetsInfo& RTInfo)
			{
				FMemory::Memzero(*this);
				for (int32 Index = 0; Index < RTInfo.NumColorRenderTargets; ++Index)
				{
					Texture[Index] = RTInfo.ColorRenderTarget[Index].Texture;
					MipIndex[Index] = RTInfo.ColorRenderTarget[Index].MipIndex;
					ArraySliceIndex[Index] = RTInfo.ColorRenderTarget[Index].ArraySliceIndex;
					LoadAction[Index] = RTInfo.ColorRenderTarget[Index].LoadAction;
					StoreAction[Index] = RTInfo.ColorRenderTarget[Index].StoreAction;
				}

				Texture[MaxSimultaneousRenderTargets] = RTInfo.DepthStencilRenderTarget.Texture;
				DepthLoadAction = RTInfo.DepthStencilRenderTarget.DepthLoadAction;
				DepthStoreAction = RTInfo.DepthStencilRenderTarget.DepthStoreAction;
				StencilLoadAction = RTInfo.DepthStencilRenderTarget.StencilLoadAction;
				StencilStoreAction = RTInfo.DepthStencilRenderTarget.GetStencilStoreAction();
				DepthStencilAccess = RTInfo.DepthStencilRenderTarget.GetDepthStencilAccess();

				bClearDepth = RTInfo.bClearDepth;
				bClearStencil = RTInfo.bClearStencil;
				bClearColor = RTInfo.bClearColor;

				for (int32 Index = 0; Index < MaxSimultaneousUAVs; ++Index)
				{
					UnorderedAccessView[Index] = RTInfo.UnorderedAccessView[Index];
				}
			}
		};

		FHashableStruct RTHash;
		FMemory::Memzero(RTHash);
		RTHash.Set(*this);
		return FCrc::MemCrc32(&RTHash, sizeof(RTHash));
	}
};

class FRHICustomPresent : public FRHIResource
{
public:
	explicit FRHICustomPresent(FRHIViewport* InViewport) 
		: ViewportRHI(InViewport) 
	{
	}
	
	virtual ~FRHICustomPresent() {} // should release any references to D3D resources.
	
	// Called when viewport is resized.
	virtual void OnBackBufferResize() = 0;

	// Called from render thread to see if a native present will be requested for this frame.
	// @return	true if native Present will be requested for this frame; false otherwise.  Must
	// match value subsequently returned by Present for this frame.
	virtual bool NeedsNativePresent() = 0;

	// Called from RHI thread to perform custom present.
	// @param InOutSyncInterval - in out param, indicates if vsync is on (>0) or off (==0).
	// @return	true if native Present should be also be performed; false otherwise. If it returns
	// true, then InOutSyncInterval could be modified to switch between VSync/NoVSync for the normal 
	// Present.  Must match value previously returned by NeedsNormalPresent for this frame.
	virtual bool Present(int32& InOutSyncInterval) = 0;

	// Called from RHI thread after native Present has been called
	virtual void PostPresent() {};

	// Called when rendering thread is acquired
	virtual void OnAcquireThreadOwnership() {}
	// Called when rendering thread is released
	virtual void OnReleaseThreadOwnership() {}

protected:
	// Weak reference, don't create a circular dependency that would prevent the viewport from being destroyed.
	FRHIViewport* ViewportRHI;
};


typedef FRHICustomPresent*              FCustomPresentRHIParamRef;
typedef TRefCountPtr<FRHICustomPresent> FCustomPresentRHIRef;

// Template magic to convert an FRHI*Shader to its enum
template<typename TRHIShader> struct TRHIShaderToEnum {};
template<> struct TRHIShaderToEnum<FRHIVertexShader>	{ enum { ShaderFrequency = SF_Vertex }; };
template<> struct TRHIShaderToEnum<FRHIHullShader>		{ enum { ShaderFrequency = SF_Hull }; };
template<> struct TRHIShaderToEnum<FRHIDomainShader>	{ enum { ShaderFrequency = SF_Domain }; };
template<> struct TRHIShaderToEnum<FRHIPixelShader>		{ enum { ShaderFrequency = SF_Pixel }; };
template<> struct TRHIShaderToEnum<FRHIGeometryShader>	{ enum { ShaderFrequency = SF_Geometry }; };
template<> struct TRHIShaderToEnum<FRHIComputeShader>	{ enum { ShaderFrequency = SF_Compute }; };
template<> struct TRHIShaderToEnum<FVertexShaderRHIParamRef>	{ enum { ShaderFrequency = SF_Vertex }; };
template<> struct TRHIShaderToEnum<FHullShaderRHIParamRef>		{ enum { ShaderFrequency = SF_Hull }; };
template<> struct TRHIShaderToEnum<FDomainShaderRHIParamRef>	{ enum { ShaderFrequency = SF_Domain }; };
template<> struct TRHIShaderToEnum<FPixelShaderRHIParamRef>		{ enum { ShaderFrequency = SF_Pixel }; };
template<> struct TRHIShaderToEnum<FGeometryShaderRHIParamRef>	{ enum { ShaderFrequency = SF_Geometry }; };
template<> struct TRHIShaderToEnum<FComputeShaderRHIParamRef>	{ enum { ShaderFrequency = SF_Compute }; };
template<> struct TRHIShaderToEnum<FVertexShaderRHIRef>		{ enum { ShaderFrequency = SF_Vertex }; };
template<> struct TRHIShaderToEnum<FHullShaderRHIRef>		{ enum { ShaderFrequency = SF_Hull }; };
template<> struct TRHIShaderToEnum<FDomainShaderRHIRef>		{ enum { ShaderFrequency = SF_Domain }; };
template<> struct TRHIShaderToEnum<FPixelShaderRHIRef>		{ enum { ShaderFrequency = SF_Pixel }; };
template<> struct TRHIShaderToEnum<FGeometryShaderRHIRef>	{ enum { ShaderFrequency = SF_Geometry }; };
template<> struct TRHIShaderToEnum<FComputeShaderRHIRef>	{ enum { ShaderFrequency = SF_Compute }; };

struct FBoundShaderStateInput
{
	FVertexDeclarationRHIParamRef VertexDeclarationRHI;
	FVertexShaderRHIParamRef VertexShaderRHI;
	FHullShaderRHIParamRef HullShaderRHI;
	FDomainShaderRHIParamRef DomainShaderRHI;
	FPixelShaderRHIParamRef PixelShaderRHI;
	FGeometryShaderRHIParamRef GeometryShaderRHI;

	FORCEINLINE FBoundShaderStateInput()
		: VertexDeclarationRHI(nullptr)
		, VertexShaderRHI(nullptr)
		, HullShaderRHI(nullptr)
		, DomainShaderRHI(nullptr)
		, PixelShaderRHI(nullptr)
		, GeometryShaderRHI(nullptr)
	{
	}

	FORCEINLINE FBoundShaderStateInput(
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI
	)
		: VertexDeclarationRHI(InVertexDeclarationRHI)
		, VertexShaderRHI(InVertexShaderRHI)
		, HullShaderRHI(InHullShaderRHI)
		, DomainShaderRHI(InDomainShaderRHI)
		, PixelShaderRHI(InPixelShaderRHI)
		, GeometryShaderRHI(InGeometryShaderRHI)
	{
	}
};

class FGraphicsPipelineStateInitializer
{
public:
	using TRenderTargetFormats = TStaticArray<EPixelFormat, MaxSimultaneousRenderTargets>;
	using TRenderTargetFlags = TStaticArray<uint32, MaxSimultaneousRenderTargets>;
	using TRenderTargetLoadActions = TStaticArray<ERenderTargetLoadAction, MaxSimultaneousRenderTargets>;
	using TRenderTargetStoreActions = TStaticArray<ERenderTargetStoreAction, MaxSimultaneousRenderTargets>;

	FGraphicsPipelineStateInitializer()
		: BlendState(nullptr)
		, RasterizerState(nullptr)
		, DepthStencilState(nullptr)
		, PrimitiveType(PT_Num)
		, RenderTargetsEnabled(0)
		, DepthStencilTargetFormat(PF_Unknown)
		, DepthStencilTargetFlag(0)
		, DepthTargetLoadAction(ERenderTargetLoadAction::ENoAction)
		, DepthTargetStoreAction(ERenderTargetStoreAction::ENoAction)
		, StencilTargetLoadAction(ERenderTargetLoadAction::ENoAction)
		, StencilTargetStoreAction(ERenderTargetStoreAction::ENoAction)
		, NumSamples(0)
	{
		for (uint32 i = 0; i < MaxSimultaneousRenderTargets; ++i)
		{
			RenderTargetFormats[i] = PF_Unknown;
			RenderTargetFlags[i] = 0;
			RenderTargetLoadActions[i] = ERenderTargetLoadAction::ENoAction;
			RenderTargetStoreActions[i] = ERenderTargetStoreAction::ENoAction;
		}
	}

	FGraphicsPipelineStateInitializer(
		FBoundShaderStateInput				InBoundShaderState,
		FBlendStateRHIParamRef				InBlendState,
		FRasterizerStateRHIParamRef			InRasterizerState,
		FDepthStencilStateRHIParamRef		InDepthStencilState,
		EPrimitiveType						InPrimitiveType,
		uint32								InRenderTargetsEnabled,
		const TRenderTargetFormats&			InRenderTargetFormats,
		const TRenderTargetFlags&			InRenderTargetFlags,
		const TRenderTargetLoadActions&		InRenderTargetLoadActions,
		const TRenderTargetStoreActions&	InRenderTargetStoreActions,
		EPixelFormat						InDepthStencilTargetFormat,
		uint32								InDepthStencilTargetFlag,
		ERenderTargetLoadAction				InDepthTargetLoadAction,
		ERenderTargetStoreAction			InDepthTargetStoreAction,
		ERenderTargetLoadAction				InStencilTargetLoadAction,
		ERenderTargetStoreAction			InStencilTargetStoreAction,
		uint32								InNumSamples
		)
		: BoundShaderState(InBoundShaderState)
		, BlendState(InBlendState)
		, RasterizerState(InRasterizerState)
		, DepthStencilState(InDepthStencilState)
		, PrimitiveType(InPrimitiveType)
		, RenderTargetsEnabled(InRenderTargetsEnabled)
		, RenderTargetFormats(InRenderTargetFormats)
		, RenderTargetFlags(InRenderTargetFlags)
		, RenderTargetLoadActions(InRenderTargetLoadActions)
		, RenderTargetStoreActions(InRenderTargetStoreActions)
		, DepthStencilTargetFormat(InDepthStencilTargetFormat)
		, DepthStencilTargetFlag(InDepthStencilTargetFlag)
		, DepthTargetLoadAction(InDepthTargetLoadAction)
		, DepthTargetStoreAction(InDepthTargetStoreAction)
		, StencilTargetLoadAction(InStencilTargetLoadAction)
		, StencilTargetStoreAction(InStencilTargetStoreAction)
		, NumSamples(InNumSamples)
	{
	}

	bool operator==(const FGraphicsPipelineStateInitializer& rhs) const
	{
		if (BoundShaderState.VertexDeclarationRHI != rhs.BoundShaderState.VertexDeclarationRHI || 
			BoundShaderState.VertexShaderRHI != rhs.BoundShaderState.VertexShaderRHI ||
			BoundShaderState.PixelShaderRHI != rhs.BoundShaderState.PixelShaderRHI ||
			BoundShaderState.GeometryShaderRHI != rhs.BoundShaderState.GeometryShaderRHI ||
			BoundShaderState.DomainShaderRHI != rhs.BoundShaderState.DomainShaderRHI ||
			BoundShaderState.HullShaderRHI != rhs.BoundShaderState.HullShaderRHI ||
			BlendState != rhs.BlendState || 
			RasterizerState != rhs.RasterizerState || 
			DepthStencilState != rhs.DepthStencilState ||
			PrimitiveType != rhs.PrimitiveType || 
			RenderTargetsEnabled != rhs.RenderTargetsEnabled ||
			RenderTargetFormats != rhs.RenderTargetFormats || 
			RenderTargetFlags != rhs.RenderTargetFlags || 
			RenderTargetLoadActions != rhs.RenderTargetLoadActions ||
			RenderTargetStoreActions != rhs.RenderTargetStoreActions || 
			DepthStencilTargetFormat != rhs.DepthStencilTargetFormat || 
			DepthStencilTargetFlag != rhs.DepthStencilTargetFlag ||
			DepthTargetLoadAction != rhs.DepthTargetLoadAction ||
			DepthTargetStoreAction != rhs.DepthTargetStoreAction ||
			StencilTargetLoadAction != rhs.StencilTargetLoadAction ||
			StencilTargetStoreAction != rhs.StencilTargetStoreAction || 
			NumSamples != rhs.NumSamples) 
		{
			return false;
		}

		return true;
	}

#define COMPARE_FIELD_BEGIN(Field) \
		if (Field != rhs.Field) \
		{ return Field COMPARE_OP rhs.Field; }

#define COMPARE_FIELD(Field) \
		else if (Field != rhs.Field) \
		{ return Field COMPARE_OP rhs.Field; }

#define COMPARE_FIELD_END \
		else { return false; }

	bool operator<(FGraphicsPipelineStateInitializer& rhs) const
	{
#define COMPARE_OP <

		COMPARE_FIELD_BEGIN(BoundShaderState.VertexDeclarationRHI)
		COMPARE_FIELD(BoundShaderState.VertexShaderRHI)
		COMPARE_FIELD(BoundShaderState.PixelShaderRHI)
		COMPARE_FIELD(BoundShaderState.GeometryShaderRHI)
		COMPARE_FIELD(BoundShaderState.DomainShaderRHI)
		COMPARE_FIELD(BoundShaderState.HullShaderRHI)
		COMPARE_FIELD(BlendState)
		COMPARE_FIELD(RasterizerState)
		COMPARE_FIELD(DepthStencilState)
		COMPARE_FIELD(PrimitiveType)
		COMPARE_FIELD_END;

#undef COMPARE_OP
	}

	bool operator>(FGraphicsPipelineStateInitializer& rhs) const
	{
#define COMPARE_OP >

		COMPARE_FIELD_BEGIN(BoundShaderState.VertexDeclarationRHI)
		COMPARE_FIELD(BoundShaderState.VertexShaderRHI)
		COMPARE_FIELD(BoundShaderState.PixelShaderRHI)
		COMPARE_FIELD(BoundShaderState.GeometryShaderRHI)
		COMPARE_FIELD(BoundShaderState.DomainShaderRHI)
		COMPARE_FIELD(BoundShaderState.HullShaderRHI)
		COMPARE_FIELD(BlendState)
		COMPARE_FIELD(RasterizerState)
		COMPARE_FIELD(DepthStencilState)
		COMPARE_FIELD(PrimitiveType)
		COMPARE_FIELD_END;

#undef COMPARE_OP
	}

#undef COMPARE_FIELD_BEGIN
#undef COMPARE_FIELD
#undef COMPARE_FIELD_END

	uint32 ComputeNumValidRenderTargets() const
	{
		// Get the count of valid render targets (ignore those at the end of the array with PF_Unknown)
		if (RenderTargetsEnabled > 0)
		{
			int32 LastValidTarget = -1;
			for (int32 i = (int32)RenderTargetsEnabled - 1; i >= 0; i--)
			{
				if (RenderTargetFormats[i] != PF_Unknown)
				{
					LastValidTarget = i;
					break;
				}
			}
			return uint32(LastValidTarget + 1);
		}
		return RenderTargetsEnabled;
	}

	// TODO: [PSO API] - As we migrate reuse existing API objects, but eventually we can move to the direct initializers. 
	// When we do that work, move this to RHI.h as its more appropriate there, but here for now since dependent typdefs are here.
	FBoundShaderStateInput			BoundShaderState;
	FBlendStateRHIParamRef			BlendState;
	FRasterizerStateRHIParamRef		RasterizerState;
	FDepthStencilStateRHIParamRef	DepthStencilState;
	EPrimitiveType					PrimitiveType;
	uint32							RenderTargetsEnabled;
	TRenderTargetFormats			RenderTargetFormats;
	TRenderTargetFlags				RenderTargetFlags;
	TRenderTargetLoadActions		RenderTargetLoadActions;
	TRenderTargetStoreActions		RenderTargetStoreActions;
	EPixelFormat					DepthStencilTargetFormat;
	uint32							DepthStencilTargetFlag;
	ERenderTargetLoadAction			DepthTargetLoadAction;
	ERenderTargetStoreAction		DepthTargetStoreAction;
	ERenderTargetLoadAction			StencilTargetLoadAction;
	ERenderTargetStoreAction		StencilTargetStoreAction;
	uint32							NumSamples;

	friend class FMeshDrawingPolicy;
};

// This PSO is used as a fallback for RHIs that dont support PSOs. It is used to set the graphics state using the legacy state setting APIs
class FRHIGraphicsPipelineStateFallBack : public FRHIGraphicsPipelineState
{
public:
	FRHIGraphicsPipelineStateFallBack() {}

	FRHIGraphicsPipelineStateFallBack(const FGraphicsPipelineStateInitializer& Init)
		: Initializer(Init)
	{
	}

	FGraphicsPipelineStateInitializer Initializer;
};

class FRHIComputePipelineStateFallback : public FRHIComputePipelineState
{
public:
	FRHIComputePipelineStateFallback(FRHIComputeShader* InComputeShader)
		: ComputeShader(InComputeShader)
	{
		check(InComputeShader);
	}

	FRHIComputeShader* GetComputeShader()
	{
		return ComputeShader;
	}

protected:
	TRefCountPtr<FRHIComputeShader> ComputeShader;
};

//
// Shader Library
//

class FRHIShaderLibrary : public FRHIResource
{
public:
	FRHIShaderLibrary(EShaderPlatform InPlatform) : Platform(InPlatform) {}
	virtual ~FRHIShaderLibrary() {}
	
	FORCEINLINE EShaderPlatform GetPlatform(void) const { return Platform; }
	
	virtual bool IsNativeLibrary() const = 0;
	
	//Library iteration
	struct FShaderLibraryEntry
	{
		FShaderLibraryEntry(): Frequency(SF_NumFrequencies), Platform(SP_NumPlatforms) {}
		
		FSHAHash Hash;
		EShaderFrequency Frequency;
		EShaderPlatform Platform;
		
		bool IsValid() const {return (Frequency < SF_NumFrequencies) && (Platform < SP_NumPlatforms);}
	};
	
	class FShaderLibraryIterator : public FRHIResource
	{
	public:
		FShaderLibraryIterator(FRHIShaderLibrary* ShaderLibrary) : ShaderLibrarySource(ShaderLibrary) {}
		virtual ~FShaderLibraryIterator() {}
	
		//Is the iterator valid
		virtual bool IsValid() const					 = 0;
		
		//Iterator position access
		virtual FShaderLibraryEntry operator*()	const	 = 0;
		
		//Iterator next operation
		virtual FShaderLibraryIterator& operator++()	 = 0;
		
		//Access the library we are iterating through - allow query e.g. GetPlatform from iterator object
		FRHIShaderLibrary* GetLibrary() const			 {return ShaderLibrarySource;};
		
	private:
		//Control source object lifetime while iterator is 'active'
		TRefCountPtr<FRHIShaderLibrary> ShaderLibrarySource;
	};
	
	virtual TRefCountPtr<FShaderLibraryIterator> CreateIterator(void) = 0;
	
	virtual uint32 GetShaderCount(void) const = 0;

protected:
	EShaderPlatform Platform;
};

typedef FRHIShaderLibrary*				FRHIShaderLibraryParamRef;
typedef TRefCountPtr<FRHIShaderLibrary>	FRHIShaderLibraryRef;


enum class ERenderTargetActions : uint8
{
	LoadOpMask = 2,

#define RTACTION_MAKE_MASK(Load, Store) (((uint8)ERenderTargetLoadAction::Load << (uint8)LoadOpMask) | (uint8)ERenderTargetStoreAction::Store)

	DontLoad_DontStore =	RTACTION_MAKE_MASK(ENoAction, ENoAction),

	DontLoad_Store =		RTACTION_MAKE_MASK(ENoAction, EStore),
	Clear_Store =			RTACTION_MAKE_MASK(EClear, EStore),
	Load_Store =			RTACTION_MAKE_MASK(ELoad, EStore),

	Clear_DontStore =		RTACTION_MAKE_MASK(EClear, ENoAction),
	Load_DontStore =		RTACTION_MAKE_MASK(ELoad, ENoAction),
	Clear_Resolve =			RTACTION_MAKE_MASK(EClear, EMultisampleResolve),
	Load_Resolve =			RTACTION_MAKE_MASK(ELoad, EMultisampleResolve),

#undef RTACTION_MAKE_MASK
};

inline ERenderTargetLoadAction GetLoadAction(ERenderTargetActions Action)
{
	return (ERenderTargetLoadAction)((uint8)Action >> (uint8)ERenderTargetActions::LoadOpMask);
}

inline ERenderTargetStoreAction GetStoreAction(ERenderTargetActions Action)
{
	return (ERenderTargetStoreAction)((uint8)Action & ((1 << (uint8)ERenderTargetActions::LoadOpMask) - 1));
}

enum class EDepthStencilTargetActions : uint8
{
	DepthMask = 4,

#define RTACTION_MAKE_MASK(Depth, Stencil) (((uint8)ERenderTargetActions::Depth << (uint8)DepthMask) | (uint8)ERenderTargetActions::Stencil)

	DontLoad_DontStore =						RTACTION_MAKE_MASK(DontLoad_DontStore, DontLoad_DontStore),
	DontLoad_StoreDepthStencil =				RTACTION_MAKE_MASK(DontLoad_Store, DontLoad_Store),
	ClearDepthStencil_StoreDepthStencil =		RTACTION_MAKE_MASK(Clear_Store, Clear_Store),
	LoadDepthStencil_StoreDepthStencil =		RTACTION_MAKE_MASK(Load_Store, Load_Store),
	LoadDepthNotStencil_DontStore =				RTACTION_MAKE_MASK(Load_DontStore, DontLoad_DontStore),
	LoadDepthStencil_StoreStencilNotDepth =		RTACTION_MAKE_MASK(Load_DontStore, Load_Store),

	ClearDepthStencil_DontStoreDepthStencil =	RTACTION_MAKE_MASK(Clear_DontStore, Clear_DontStore),
	LoadDepthStencil_DontStoreDepthStencil =	RTACTION_MAKE_MASK(Load_DontStore, Load_DontStore),
	ClearDepthStencil_StoreDepthNotStencil =	RTACTION_MAKE_MASK(Clear_Store, Clear_DontStore),
	ClearDepthStencil_StoreStencilNotDepth =	RTACTION_MAKE_MASK(Clear_DontStore, Clear_Store),
	ClearDepthStencil_ResolveDepthNotStencil =	RTACTION_MAKE_MASK(Clear_Resolve, Clear_DontStore),
	ClearDepthStencil_ResolveStencilNotDepth =	RTACTION_MAKE_MASK(Clear_DontStore, Clear_Resolve),

#undef RTACTION_MAKE_MASK
};

inline ERenderTargetActions GetDepthActions(EDepthStencilTargetActions Action)
{
	return (ERenderTargetActions)((uint8)Action >> (uint8)EDepthStencilTargetActions::DepthMask);
}

inline ERenderTargetActions GetStencilActions(EDepthStencilTargetActions Action)
{
	return (ERenderTargetActions)((uint8)Action & ((1 << (uint8)EDepthStencilTargetActions::DepthMask) - 1));
}

struct FRHIRenderPassInfo
{
	struct FColorEntry
	{
		FRHITexture* RenderTarget;
		FRHITexture* ResolveTarget;
		int32 ArraySlice;
		uint8 MipIndex;
		ERenderTargetActions Action;
	};
	FColorEntry ColorRenderTargets[MaxSimultaneousRenderTargets];

	struct FDepthStencilEntry
	{
		FRHITexture* DepthStencilTarget;
		FRHITexture* ResolveTarget;
		EDepthStencilTargetActions Action;
	};
	FDepthStencilEntry DepthStencilRenderTarget;

	// Special case when we want to bind the depth target as read-only AND sample as texture
	bool bDepthReadOnly = false;

	// Some RHIs require a hint that occlusion queries will be used in this render pass
	bool bOcclusionQueries = false;

	// Color, no depth
	explicit FRHIRenderPassInfo(FRHITexture* ColorRT, ERenderTargetActions ColorAction, FRHITexture* ResolveRT = nullptr)
	{
		check(ColorRT);
		ColorRenderTargets[0].RenderTarget = ColorRT;
		ColorRenderTargets[0].ResolveTarget = ResolveRT;
		ColorRenderTargets[0].ArraySlice = -1;
		ColorRenderTargets[0].MipIndex = 0;
		ColorRenderTargets[0].Action = ColorAction;
		DepthStencilRenderTarget.DepthStencilTarget = nullptr;
		DepthStencilRenderTarget.Action = EDepthStencilTargetActions::DontLoad_DontStore;
		bIsMSAA = ColorRT->GetNumSamples() > 1;
		FMemory::Memzero(&ColorRenderTargets[1], sizeof(FColorEntry) * (MaxSimultaneousRenderTargets - 1));
	}

	// Color MRTs, no depth
	explicit FRHIRenderPassInfo(int32 NumColorRTs, FRHITexture* ColorRTs[], ERenderTargetActions ColorAction)
	{
		check(NumColorRTs > 0);
		for (int32 Index = 0; Index < NumColorRTs; ++Index)
		{
			check(ColorRTs[Index]);
			ColorRenderTargets[Index].RenderTarget = ColorRTs[Index];
			ColorRenderTargets[Index].ResolveTarget = nullptr;
			ColorRenderTargets[Index].ArraySlice = -1;
			ColorRenderTargets[Index].MipIndex = 0;
			ColorRenderTargets[Index].Action = ColorAction;
		}
		DepthStencilRenderTarget.DepthStencilTarget = nullptr;
		DepthStencilRenderTarget.Action = EDepthStencilTargetActions::DontLoad_DontStore;
		if (NumColorRTs < MaxSimultaneousRenderTargets)
		{
			FMemory::Memzero(&ColorRenderTargets[NumColorRTs], sizeof(FColorEntry) * (MaxSimultaneousRenderTargets - NumColorRTs));
		}
	}

	// Color MRTs and depth
	explicit FRHIRenderPassInfo(int32 NumColorRTs, FRHITexture* ColorRTs[], ERenderTargetActions ColorAction, FRHITexture* DepthRT, EDepthStencilTargetActions DepthActions)
	{
		check(NumColorRTs > 0);
		for (int32 Index = 0; Index < NumColorRTs; ++Index)
		{
			check(ColorRTs[Index]);
			ColorRenderTargets[Index].RenderTarget = ColorRTs[Index];
			ColorRenderTargets[Index].ResolveTarget = nullptr;
			ColorRenderTargets[Index].ArraySlice = -1;
			ColorRenderTargets[Index].MipIndex = 0;
			ColorRenderTargets[Index].Action = ColorAction;
		}
		check(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = nullptr;
		DepthStencilRenderTarget.Action = DepthActions;
		bIsMSAA = DepthRT->GetNumSamples() > 1;
		if (NumColorRTs < MaxSimultaneousRenderTargets)
		{
			FMemory::Memzero(&ColorRenderTargets[NumColorRTs], sizeof(FColorEntry) * (MaxSimultaneousRenderTargets - NumColorRTs));
		}
	}

	// Depth, no color
	explicit FRHIRenderPassInfo(FRHITexture* DepthRT, EDepthStencilTargetActions DepthActions, FRHITexture* ResolveDepthRT = nullptr)
	{
		check(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = ResolveDepthRT;
		DepthStencilRenderTarget.Action = DepthActions;
		bIsMSAA = DepthRT->GetNumSamples() > 1;
		FMemory::Memzero(ColorRenderTargets, sizeof(FColorEntry) * MaxSimultaneousRenderTargets);
	}

	// Color and depth
	explicit FRHIRenderPassInfo(FRHITexture* ColorRT, ERenderTargetActions ColorAction, FRHITexture* DepthRT, EDepthStencilTargetActions DepthActions)
	{
		check(ColorRT);
		ColorRenderTargets[0].RenderTarget = ColorRT;
		ColorRenderTargets[0].ResolveTarget = nullptr;
		ColorRenderTargets[0].ArraySlice = -1;
		ColorRenderTargets[0].MipIndex = 0;
		ColorRenderTargets[0].Action = ColorAction;
		bIsMSAA = ColorRT->GetNumSamples() > 1;
		check(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = nullptr;
		DepthStencilRenderTarget.Action = DepthActions;
		FMemory::Memzero(&ColorRenderTargets[1], sizeof(FColorEntry) * (MaxSimultaneousRenderTargets - 1));
	}

	// Color and depth
	explicit FRHIRenderPassInfo(FRHITexture* ColorRT, ERenderTargetActions ColorAction, FRHITexture* ResolveColorRT,
		FRHITexture* DepthRT, EDepthStencilTargetActions DepthActions, FRHITexture* ResolveDepthRT)
	{
		check(ColorRT);
		ColorRenderTargets[0].RenderTarget = ColorRT;
		ColorRenderTargets[0].ResolveTarget = ResolveColorRT;
		ColorRenderTargets[0].ArraySlice = -1;
		ColorRenderTargets[0].MipIndex = 0;
		ColorRenderTargets[0].Action = ColorAction;
		bIsMSAA = ColorRT->GetNumSamples() > 1;
		check(DepthRT);
		DepthStencilRenderTarget.DepthStencilTarget = DepthRT;
		DepthStencilRenderTarget.ResolveTarget = ResolveDepthRT;
		DepthStencilRenderTarget.Action = DepthActions;
		FMemory::Memzero(&ColorRenderTargets[1], sizeof(FColorEntry) * (MaxSimultaneousRenderTargets - 1));
	}

	explicit FRHIRenderPassInfo()
	{
		FMemory::Memzero(*this);
	}

	void SetDepthReadOnly(bool bInDepthReadOnly)
	{
		bDepthReadOnly = bInDepthReadOnly;
	}

	void SetOcclusionQueries(bool bInOcclusionQueries)
	{
		bOcclusionQueries = bInOcclusionQueries;
	}

	void DEPRECATED_SetExclusiveDepthStencil(FExclusiveDepthStencil InEDS)
	{
		DEPRECATED_EDS = InEDS;
		bDEPRECATEDHasEDS = true;
	}

	inline bool IsMSAA() const
	{
		return bIsMSAA;
	}

	inline void Validate() const
	{
		int32 NumSamples = -1;	// -1 means nothing found yet
		int32 ColorIndex = 0;
		for (; ColorIndex < MaxSimultaneousRenderTargets; ++ColorIndex)
		{
			const FColorEntry& Entry = ColorRenderTargets[ColorIndex];
			if (Entry.RenderTarget)
			{
				// Ensure NumSamples matches amongst all color RTs
				if (NumSamples == -1)
				{
					NumSamples = Entry.RenderTarget->GetNumSamples();
				}
				else
				{
					ensure(Entry.RenderTarget->GetNumSamples() == NumSamples);
				}

				ERenderTargetStoreAction Store = GetStoreAction(Entry.Action);
				// Don't try to resolve a non-msaa
				ensure(Store != ERenderTargetStoreAction::EMultisampleResolve || Entry.RenderTarget->GetNumSamples() > 1);
				// Don't resolve to null
				ensure(Store != ERenderTargetStoreAction::EMultisampleResolve || Entry.ResolveTarget);
			}
			else
			{
				break;
			}
		}

		int32 NumColorRenderTargets = ColorIndex;
		for (; ColorIndex < MaxSimultaneousRenderTargets; ++ColorIndex)
		{
			// Gap in the sequence of valid render targets (ie RT0, null, RT2, ...)
			ensureMsgf(!ColorRenderTargets[ColorIndex].RenderTarget, TEXT("Missing color render target on slot %d"), ColorIndex - 1);
		}

		if (DepthStencilRenderTarget.DepthStencilTarget)
		{
			// Ensure NumSamples matches with color RT
			if (NumSamples != -1)
			{
				ensure(DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples() == NumSamples);
			}
			ERenderTargetStoreAction DepthStore = GetStoreAction(GetDepthActions(DepthStencilRenderTarget.Action));
			ERenderTargetStoreAction StencilStore = GetStoreAction(GetStencilActions(DepthStencilRenderTarget.Action));
			bool bIsStore = DepthStore == ERenderTargetStoreAction::EMultisampleResolve || StencilStore == ERenderTargetStoreAction::EMultisampleResolve;
			// Don't try to resolve a non-msaa
			ensure(!bIsStore || DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples() > 1);
			// Don't resolve to null
			ensure(!bIsStore || DepthStencilRenderTarget.ResolveTarget);
			// Don't write to depth if read-only
			ensure(!bDepthReadOnly || DepthStore != ERenderTargetStoreAction::EStore);
		}
	}

	RHI_API void ConvertToRenderTargetsInfo(FRHISetRenderTargetsInfo& OutRTInfo) const;

private:
	FExclusiveDepthStencil DEPRECATED_EDS;
	bool bDEPRECATEDHasEDS = false;
	bool bIsMSAA = false;
};

class FRHIRenderPass : public FRHIResource {};
class FRHIParallelRenderPass : public FRHIRenderPass {};
class FRHIRenderSubPass : public FRHIRenderPass {};

class FRHIRenderPassFallback : public FRHIRenderPass
{
public:
	explicit FRHIRenderPassFallback(const FRHIRenderPassInfo& InInfo, const TCHAR* InName)
		: Info(InInfo)
		, Name(InName)
		, bEnded(false)
	{
	}

	virtual ~FRHIRenderPassFallback()
	{
		check(bEnded);
	}

	void SetEnded()
	{
		check(!bEnded);
		bEnded = true;
	}

protected:
	FRHIRenderPassInfo Info;
	FString Name;
	bool bEnded;
};

class FRHIParallelRenderPassFallback : public FRHIParallelRenderPass
{
public:
	explicit FRHIParallelRenderPassFallback(const FRHIRenderPassInfo& InInfo, const TCHAR* InName)
		: Info(InInfo)
		, Name(InName)
		, bEnded(false)
	{
	}

	virtual ~FRHIParallelRenderPassFallback()
	{
		check(SubPasses.Num() == 0);
		check(bEnded);
	}

	void SetEnded()
	{
		check(!bEnded);
		bEnded = true;
	}

	void BeginSubPass(FRHIRenderSubPass* InSubPass)
	{
		SubPasses.Add(InSubPass);
	}

	void EndSubPass(FRHIRenderSubPass* InSubPass)
	{
		int32 Found = SubPasses.Remove(InSubPass);
		check(Found > 0);
	}

protected:
	FRHIRenderPassInfo Info;
	TArray<FRHIRenderSubPass*> SubPasses;
	FString Name;
	bool bEnded;
};

class FRHIRenderSubPassFallback : public FRHIRenderSubPass
{
public:
	explicit FRHIRenderSubPassFallback(FRHIParallelRenderPassFallback* InParent)
		: Parent(InParent)
		, bEnded(false)
	{
	}

	virtual ~FRHIRenderSubPassFallback()
	{
		check(bEnded);
	}

	void SetEnded()
	{
		check(!bEnded);
		bEnded = true;
	}

	FRHIParallelRenderPassFallback* GetParent()
	{
		return Parent;
	}

protected:
	FRHIParallelRenderPassFallback* Parent;
	bool bEnded;
};
