// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanResources.h: Vulkan resource RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanConfiguration.h"
#include "VulkanState.h"
#include "VulkanUtil.h"
#include "BoundShaderStateCache.h"
#include "VulkanShaderResources.h"
#include "VulkanState.h"
#include "VulkanMemory.h"

class FVulkanDevice;
class FVulkanQueue;
class FVulkanCmdBuffer;
class FVulkanGlobalUniformPool;
class FVulkanBuffer;
class FVulkanBufferCPU;
struct FVulkanTextureBase;
class FVulkanTexture2D;
struct FVulkanBufferView;
class FVulkanResourceMultiBuffer;
struct FVulkanSemaphore;

namespace VulkanRHI
{
	class FDeviceMemoryAllocation;
	class FOldResourceAllocation;
	struct FPendingBufferLock;
}

enum
{
	NUM_OCCLUSION_QUERIES_PER_POOL = 4096,

	NUM_TIMESTAMP_QUERIES_PER_POOL = 1024,
};


/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FVulkanVertexDeclaration : public FRHIVertexDeclaration
{
public:
	FVertexDeclarationElementList Elements;

	FVulkanVertexDeclaration(const FVertexDeclarationElementList& InElements);

	static void EmptyCache();
};


class FVulkanShader
{
public:
	FVulkanShader(FVulkanDevice* InDevice) :
		ShaderModule(VK_NULL_HANDLE),
		Code(nullptr),
		CodeSize(0),
		Device(InDevice)
	{
	}

	virtual ~FVulkanShader();

	void Create(EShaderFrequency Frequency, const TArray<uint8>& InCode);

	FORCEINLINE const VkShaderModule& GetHandle() const
	{
		return ShaderModule;
	}

	inline const FString& GetDebugName() const
	{
		return DebugName;
	}

	FORCEINLINE const FVulkanCodeHeader& GetCodeHeader() const
	{
		return CodeHeader;
	}

protected:
	/** External bindings for this shader. */
	FVulkanCodeHeader CodeHeader;

	VkShaderModule ShaderModule;

	uint32* Code;
	uint32 CodeSize;
	FString DebugName;

	TArray<ANSICHAR> GlslSource;

	FVulkanDevice* Device;

	friend class FVulkanCommandListContext;
	friend class FVulkanPipelineStateCache;
	friend class FVulkanComputeShaderState;
	friend class FVulkanComputePipeline;
};

/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
template<typename BaseResourceType, EShaderFrequency ShaderType>
class TVulkanBaseShader : public BaseResourceType, public IRefCountedObject, public FVulkanShader
{
public:
	TVulkanBaseShader(FVulkanDevice* InDevice) :
		FVulkanShader(InDevice)
	{
	}

	enum { StaticFrequency = ShaderType };

	void Create(const TArray<uint8>& InCode);

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}
};

typedef TVulkanBaseShader<FRHIVertexShader, SF_Vertex> FVulkanVertexShader;
typedef TVulkanBaseShader<FRHIPixelShader, SF_Pixel> FVulkanPixelShader;
typedef TVulkanBaseShader<FRHIHullShader, SF_Hull> FVulkanHullShader;
typedef TVulkanBaseShader<FRHIDomainShader, SF_Domain> FVulkanDomainShader;
typedef TVulkanBaseShader<FRHIComputeShader, SF_Compute> FVulkanComputeShader;
typedef TVulkanBaseShader<FRHIGeometryShader, SF_Geometry> FVulkanGeometryShader;

class FVulkanBoundShaderState : public FRHIBoundShaderState
{
public:
	FVulkanBoundShaderState(
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI
	);

	virtual ~FVulkanBoundShaderState();

	FORCEINLINE FVulkanVertexShader*   GetVertexShader() const { return (FVulkanVertexShader*)CacheLink.GetVertexShader(); }
	FORCEINLINE FVulkanPixelShader*    GetPixelShader() const { return (FVulkanPixelShader*)CacheLink.GetPixelShader(); }
	FORCEINLINE FVulkanHullShader*     GetHullShader() const { return (FVulkanHullShader*)CacheLink.GetHullShader(); }
	FORCEINLINE FVulkanDomainShader*   GetDomainShader() const { return (FVulkanDomainShader*)CacheLink.GetDomainShader(); }
	FORCEINLINE FVulkanGeometryShader* GetGeometryShader() const { return (FVulkanGeometryShader*)CacheLink.GetGeometryShader(); }

	const FVulkanShader* GetShader(EShaderFrequency Stage) const
	{
		FVulkanShader* ShadersAsArray[SF_Compute] =
		{
			GetVertexShader(),
			GetHullShader(),
			GetDomainShader(),
			GetPixelShader(),
			GetGeometryShader()
		};

		check(Stage < SF_Compute);
		return ShadersAsArray[Stage];
	}

private:
	FCachedBoundShaderStateLink_Threadsafe CacheLink;
};

/** Texture/RT wrapper. */
class FVulkanSurface
{
public:

	// Seperate method for creating image, this can be used to measure image size
	// After VkImage is no longer needed, dont forget to destroy/release it 
	static VkImage CreateImage(
		FVulkanDevice& InDevice,
		VkImageViewType ResourceType,
		EPixelFormat InFormat,
		uint32 SizeX, uint32 SizeY, uint32 SizeZ,
		bool bArray, uint32 ArraySize,
		uint32 NumMips,
		uint32 NumSamples,
		uint32 UEFlags,
		VkMemoryRequirements& OutMemoryRequirements,
		VkFormat* OutStorageFormat = nullptr,
		VkFormat* OutViewFormat = nullptr,
		VkImageCreateInfo* OutInfo = nullptr,
		bool bForceLinearTexture = false);

	FVulkanSurface(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format,
					uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize,
					uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);

	// Constructor for externally owned Image
	FVulkanSurface(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format,
					uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 NumSamples,
					VkImage InImage, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);

	virtual ~FVulkanSurface();

	void Destroy();

#if 0
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
#endif

	/**
	 * Returns how much memory is used by the surface
	 */
	uint32 GetMemorySize() const
	{
		return MemoryRequirements.size;
	}

	/**
	 * Returns one of the texture's mip-maps stride.
	 */
	void GetMipStride(uint32 MipIndex, uint32& Stride);

	/*
	 * Returns the memory offset to the texture's mip-map.
	 */
	void GetMipOffset(uint32 MipIndex, uint32& Offset);

	/**
	* Returns how much memory a single mip uses.
	*/
	void GetMipSize(uint32 MipIndex, uint32& MipBytes);

	inline VkImageViewType GetViewType() const { return ViewType; }

	inline VkImageTiling GetTiling() const { return Tiling; }

	inline uint32 GetNumMips() const { return NumMips; }

	// Full includes Depth+Stencil
	inline VkImageAspectFlags GetFullAspectMask() const
	{
		return FullAspectMask;
	}

	// Only Depth or Stencil
	inline VkImageAspectFlags GetPartialAspectMask() const
	{
		return PartialAspectMask;
	}

	inline bool IsImageOwner() const
	{
		return bIsImageOwner;
	}

	inline VulkanRHI::FDeviceMemoryAllocation* GetAllocation() const
	{
		return Allocation;
	}

	FVulkanDevice* Device;

	VkImage Image;
	
	// Removes SRGB if requested, used to upload data
	VkFormat StorageFormat;
	// Format for SRVs, render targets
	VkFormat ViewFormat;
	uint32 Width, Height, Depth;
	// UE format
	EPixelFormat PixelFormat;
	uint32 UEFlags;
	VkMemoryPropertyFlags MemProps;
	VkMemoryRequirements MemoryRequirements;

	static void InternalLockWrite(FVulkanCommandListContext& Context, FVulkanSurface* Surface, const VkImageSubresourceRange& SubresourceRange, const VkBufferImageCopy& Region, VulkanRHI::FStagingBuffer* StagingBuffer);

private:

	// Used to clear render-target objects on creation
	void InitialClear(FVulkanCommandListContext& Context, const FClearValueBinding& ClearValueBinding, bool bTransitionToPresentable);
	friend struct FRHICommandInitialClearTexture;

private:
	VkImageTiling Tiling;
	VkImageViewType	ViewType;

	bool bIsImageOwner;
	VulkanRHI::FDeviceMemoryAllocation* Allocation;
	TRefCountPtr<VulkanRHI::FOldResourceAllocation> ResourceAllocation;

	uint32 NumMips;

	uint32 NumSamples;

	VkImageAspectFlags FullAspectMask;
	VkImageAspectFlags PartialAspectMask;

	friend struct FVulkanTextureBase;
};


struct FVulkanTextureView
{
	FVulkanTextureView()
		: View(VK_NULL_HANDLE)
		, Image(VK_NULL_HANDLE)
	{
	}

	static VkImageView StaticCreate(FVulkanDevice& Device, VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices, bool bUseIdentitySwizzle = false);

	void Create(FVulkanDevice& Device, VkImage InImage, VkImageViewType ViewType, VkImageAspectFlags AspectFlags, EPixelFormat UEFormat, VkFormat Format, uint32 FirstMip, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices);
	void Destroy(FVulkanDevice& Device);

	VkImageView View;
	VkImage Image;
};

/** The base class of resources that may be bound as shader resources. */
class FVulkanBaseShaderResource : public IRefCountedObject
{
};

struct FVulkanTextureBase : public FVulkanBaseShaderResource
{
	inline static FVulkanTextureBase* Cast(FRHITexture* Texture)
	{
		check(Texture);
		FVulkanTextureBase* OutTexture = (FVulkanTextureBase*)Texture->GetTextureBaseRHI();
		check(OutTexture);
		return OutTexture;
	}


	FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);
	FVulkanTextureBase(FVulkanDevice& Device, VkImageViewType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 NumSamples, VkImage InImage, VkDeviceMemory InMem, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo = FRHIResourceCreateInfo());
	virtual ~FVulkanTextureBase();

	VkImageView CreateRenderTargetView(uint32 MipIndex, uint32 NumMips, uint32 ArraySliceIndex, uint32 NumArraySlices);
	void AliasTextureResources(const FVulkanTextureBase* SrcTexture);

	FVulkanSurface Surface;

	// View with all mips/layers
	FVulkanTextureView DefaultView;
	// View with all mips/layers, but if it's a Depth/Stencil, only the Depth view
	FVulkanTextureView* PartialView;

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	// Surface and view for MSAA render target, valid only when created with NumSamples > 1
	FVulkanSurface* MSAASurface;
	FVulkanTextureView MSAAView;
#endif

	bool bIsAliased;

private:
	void DestroyViews();
};

class FVulkanBackBuffer;
class FVulkanTexture2D : public FRHITexture2D, public FVulkanTextureBase
{
public:
	FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);
	FVulkanTexture2D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, VkImage Image, uint32 UEFlags, const FRHIResourceCreateInfo& CreateInfo);
	virtual ~FVulkanTexture2D();

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}

	virtual FVulkanBackBuffer* GetBackBuffer()
	{
		return nullptr;
	}

	virtual void* GetTextureBaseRHI() override final
	{
		FVulkanTextureBase* Base = static_cast<FVulkanTextureBase*>(this);
		return Base;
	}
};

class FVulkanBackBuffer : public FVulkanTexture2D
{
public:
	FVulkanBackBuffer(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 UEFlags);
	FVulkanBackBuffer(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, VkImage Image, uint32 UEFlags);
	virtual ~FVulkanBackBuffer();

	virtual FVulkanBackBuffer* GetBackBuffer() override final
	{
		return this;
	}
};

class FVulkanTexture2DArray : public FRHITexture2DArray, public FVulkanTextureBase
{
public:
	// Constructor, just calls base and Surface constructor
	FVulkanTexture2DArray(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
	FVulkanTexture2DArray(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, VkImage Image, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
		
	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}

	virtual void* GetTextureBaseRHI() override final
	{
		return (FVulkanTextureBase*)this;
	}
};

class FVulkanTexture3D : public FRHITexture3D, public FVulkanTextureBase
{
public:
	// Constructor, just calls base and Surface constructor
	FVulkanTexture3D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
	FVulkanTexture3D(FVulkanDevice& Device, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, VkImage Image, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
	virtual ~FVulkanTexture3D();

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}

	virtual void* GetTextureBaseRHI() override final
	{
		return (FVulkanTextureBase*)this;
	}
};

class FVulkanTextureCube : public FRHITextureCube, public FVulkanTextureBase
{
public:
	FVulkanTextureCube(FVulkanDevice& Device, EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
	FVulkanTextureCube(FVulkanDevice& Device, EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, VkImage Image, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue);
	virtual ~FVulkanTextureCube();

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}

	virtual void* GetTextureBaseRHI() override final
	{
		return (FVulkanTextureBase*)this;
	}
};

class FVulkanTextureReference : public FRHITextureReference, public FVulkanTextureBase
{
public:
	explicit FVulkanTextureReference(FVulkanDevice& Device, FLastRenderTimeContainer* InLastRenderTime)
	:	FRHITextureReference(InLastRenderTime)
	,	FVulkanTextureBase(Device, VK_IMAGE_VIEW_TYPE_MAX_ENUM, PF_Unknown, 0, 0, 0, 1, 1, VK_NULL_HANDLE, VK_NULL_HANDLE, 0)
	{}

	// IRefCountedObject interface.
	virtual uint32 AddRef() const override final
	{
		return FRHIResource::AddRef();
	}

	virtual uint32 Release() const override final
	{
		return FRHIResource::Release();
	}

	virtual uint32 GetRefCount() const override final
	{
		return FRHIResource::GetRefCount();
	}

	virtual void* GetTextureBaseRHI() override final
	{
		return GetReferencedTexture()->GetTextureBaseRHI();
	}

	void SetReferencedTexture(FRHITexture* InTexture);
};

/** Given a pointer to a RHI texture that was created by the Vulkan RHI, returns a pointer to the FVulkanTextureBase it encapsulates. */
inline FVulkanTextureBase* GetVulkanTextureFromRHITexture(FRHITexture* Texture)
{
	if (!Texture)
	{
		return NULL;
	}
	else if (Texture->GetTexture2D())
	{
		return static_cast<FVulkanTexture2D*>(Texture);
	}
	else if (Texture->GetTextureReference())
	{
		return static_cast<FVulkanTextureReference*>(Texture);
	}
	else if (Texture->GetTexture2DArray())
	{
		return static_cast<FVulkanTexture2DArray*>(Texture);
	}
	else if (Texture->GetTexture3D())
	{
		return static_cast<FVulkanTexture3D*>(Texture);
	}
	else if (Texture->GetTextureCube())
	{
		return static_cast<FVulkanTextureCube*>(Texture);
	}
	else
	{
		UE_LOG(LogVulkanRHI, Fatal, TEXT("Unknown Vulkan RHI texture type"));
		return NULL;
	}
}

class FVulkanQueryPool : public VulkanRHI::FDeviceChild
{
public:
	FVulkanQueryPool(FVulkanDevice* InDevice, uint32 InNumQueries, VkQueryType InQueryType);
	virtual ~FVulkanQueryPool();

	virtual void Destroy();

	void Reset(FVulkanCmdBuffer* CmdBuffer);

	inline VkQueryPool GetHandle() const
	{
		return QueryPool;
	}

protected:
	VkQueryPool QueryPool;
	const uint32 NumQueries;
	const VkQueryType QueryType;

	TArray<uint64> QueryOutput;

	friend class FVulkanDynamicRHI;
};

class FVulkanBufferedQueryPool : public FVulkanQueryPool
{
public:
	FVulkanBufferedQueryPool(FVulkanDevice* InDevice, uint32 InNumQueries, VkQueryType InQueryType)
		: FVulkanQueryPool(InDevice, InNumQueries, InQueryType)
		, LastBeginIndex(0)
	{
		QueryOutput.SetNum(InNumQueries);
		UsedQueryBits.AddZeroed((InNumQueries + 63) / 64);
		StartedQueryBits.AddZeroed((InNumQueries + 63) / 64);
		ReadResultsBits.AddZeroed((InNumQueries + 63) / 64);
	}

	void MarkQueryAsStarted(uint32 QueryIndex)
	{
		uint32 Word = QueryIndex / 64;
		uint64 Bit = (uint64)1 << (QueryIndex % 64);
		StartedQueryBits[Word] = StartedQueryBits[Word] | Bit;
	}

	bool AcquireQuery(uint32& OutIndex)
	{
		const uint64 AllUsedMask = (uint64)-1;
		for (int32 WordIndex = LastBeginIndex / 64; WordIndex < UsedQueryBits.Num(); ++WordIndex)
		{
			uint64 BeginQueryWord = UsedQueryBits[WordIndex];
			if (BeginQueryWord != AllUsedMask)
			{
				OutIndex = 0;
				while ((BeginQueryWord & 1) == 1)
				{
					++OutIndex;
					BeginQueryWord >>= 1;
				}
				OutIndex += WordIndex * 64;
				uint64 Bit = (uint64)1 << (uint64)(OutIndex % 64);
				UsedQueryBits[WordIndex] = UsedQueryBits[WordIndex] | Bit;
				ReadResultsBits[WordIndex] &= ~Bit;
				LastBeginIndex = OutIndex + 1;
				return true;
			}
		}

		// Full!
		return false;
	}

	void ReleaseQuery(uint32 QueryIndex)
	{
		uint32 Word = QueryIndex / 64;
		uint64 Bit = (uint64)1 << (QueryIndex % 64);
		UsedQueryBits[Word] = UsedQueryBits[Word] & ~Bit;
		ReadResultsBits[Word] = ReadResultsBits[Word] & ~Bit;
		if (QueryIndex < LastBeginIndex)
		{
			// Use the lowest word available
			const uint64 AllUsedMask = (uint64)-1;
			const uint64 LastQueryWord = LastBeginIndex / 64;
			if (LastQueryWord < UsedQueryBits.Num()
				&& UsedQueryBits[LastQueryWord] == AllUsedMask)
			{
				LastBeginIndex = QueryIndex;
			}
		}
	}

	void ResetIfRead(VkCommandBuffer CmdBuffer, uint32 QueryIndex)
	{
		uint32 Word = QueryIndex / 64;
		uint64 Bit = (uint64)1 << (QueryIndex % 64);
		if ((ReadResultsBits[Word] & Bit) == Bit)
		{
			VulkanRHI::vkCmdResetQueryPool(CmdBuffer, QueryPool, QueryIndex, 1);
			ReadResultsBits[Word] = ReadResultsBits[Word] & ~Bit;
		}
	}

	void ResetReadResultBits(VkCommandBuffer CmdBuffer, uint32 QueryIndex, uint32 QueryCount)
	{
		for (uint32 Index = 0; Index < QueryCount; ++Index)
		{
			const uint32 CurrentQueryIndex = QueryIndex + Index;
			const uint32 Word = CurrentQueryIndex / 64;
			const uint64 Bit = (uint64)1 << (CurrentQueryIndex % 64);
			ReadResultsBits[Word] = ReadResultsBits[Word] & ~Bit;
			StartedQueryBits[Word] = StartedQueryBits[Word] & ~Bit;
		}
	}

	bool GetResults(class FVulkanCommandListContext& Context, class FVulkanRenderQuery* Query, bool bWait, uint64& OutNumPixels);

	bool HasRoom() const
	{
		const uint64 AllUsedMask = (uint64)-1;
		if (LastBeginIndex < UsedQueryBits.Num() * 64)
		{
			check((UsedQueryBits[LastBeginIndex / 64] & AllUsedMask) != AllUsedMask);
			return true;
		}

		return false;
	}

	bool HasExpired() const {return false;}

protected:
	TArray<uint64> UsedQueryBits;
	TArray<uint64> StartedQueryBits;
	TArray<uint64> ReadResultsBits;

	// Last potentially free index in the pool
	uint64 LastBeginIndex;

	friend class FVulkanCommandListContext;
};

class FVulkanRenderQuery : public FRHIRenderQuery
{
public:
	FVulkanRenderQuery(FVulkanDevice* Device, ERenderQueryType InQueryType);
	virtual ~FVulkanRenderQuery();

private:

	enum
	{
		NumQueries = NUM_RENDER_BUFFERS
	};
	
	int32 CurrentQueryIdx;

	// Actual index and pool filled in after RHIBeginOcclusionQueryBatch
	FVulkanQueryPool* QueryPools[NumQueries];
	FVulkanQueryPool* GetActiveQueryPool() const { return QueryPools[CurrentQueryIdx]; }
	void SetActiveQueryPool(FVulkanQueryPool* Pool) { QueryPools[CurrentQueryIdx] = Pool; }
	
	int32 QueryIndices[NumQueries];
	int32 GetActiveQueryIndex() const { return QueryIndices[CurrentQueryIdx]; }
	void SetActiveQueryIndex(int32 QueryIndex) { QueryIndices[CurrentQueryIdx] = QueryIndex; }
	void AdvanceQueryIndex() { CurrentQueryIdx = (CurrentQueryIdx + 1) % NumQueries; }

	ERenderQueryType QueryType;
	FVulkanCmdBuffer* CurrentCmdBuffer;

	friend class FVulkanDynamicRHI;
	friend class FVulkanCommandListContext;
	friend class FVulkanBufferedQueryPool;
	friend class FVulkanGPUTiming;

	void Begin(FVulkanCmdBuffer* CmdBuffer);
	void End(FVulkanCmdBuffer* CmdBuffer);

	bool GetResult(FVulkanDevice* Device, uint64& Result, bool bWait);
};

struct FVulkanBufferView : public FRHIResource, public VulkanRHI::FDeviceChild
{
	FVulkanBufferView(FVulkanDevice* InDevice)
		: VulkanRHI::FDeviceChild(InDevice)
		, View(VK_NULL_HANDLE)
		, Flags(0)
		, Offset(0)
		, Size(0)
	{
	}

	virtual ~FVulkanBufferView()
	{
		Destroy();
	}

	void Create(FVulkanBuffer& Buffer, EPixelFormat Format, uint32 InOffset, uint32 InSize);
	void Create(FVulkanResourceMultiBuffer* Buffer, EPixelFormat Format, uint32 InOffset, uint32 InSize);
	void Create(VkFormat Format, FVulkanResourceMultiBuffer* Buffer, uint32 InOffset, uint32 InSize);
	void Destroy();

	VkBufferView View;
	VkFlags Flags;
	uint32 Offset;
	uint32 Size;
};

class FVulkanBuffer : public FRHIResource
{
public:
	FVulkanBuffer(FVulkanDevice& Device, uint32 InSize, VkFlags InUsage, VkMemoryPropertyFlags InMemPropertyFlags, bool bAllowMultiLock, const char* File, int32 Line);
	virtual ~FVulkanBuffer();

	inline const VkBuffer& GetBufferHandle() const { return Buf; }

	inline uint32 GetSize() const { return Size; }

	void* Lock(uint32 InSize, uint32 InOffset = 0);

	void Unlock();

	inline const VkFlags& GetFlags() const { return Usage; }

private:
	FVulkanDevice& Device;
	VkBuffer Buf;
	VulkanRHI::FDeviceMemoryAllocation* Allocation;
	uint32 Size;
	VkFlags Usage;

	void* BufferPtr;	
	VkMappedMemoryRange MappedRange;

	bool bAllowMultiLock;
	int32 LockStack;
};

struct FVulkanRingBuffer : public VulkanRHI::FDeviceChild
{
public:
	FVulkanRingBuffer(FVulkanDevice* InDevice, uint64 TotalSize, VkFlags Usage, VkMemoryPropertyFlags MemPropertyFlags);
	~FVulkanRingBuffer();

	// allocate some space in the ring buffer
	uint64 AllocateMemory(uint64 Size, uint32 Alignment);

	inline uint32 GetBufferOffset() const
	{
		return BufferSuballocation->GetOffset();
	}

	inline VkBuffer GetHandle() const
	{
		return BufferSuballocation->GetHandle();
	}

	inline void* GetMappedPointer()
	{
		return BufferSuballocation->GetMappedPointer();
	}

protected:
	uint64 BufferSize;
	uint64 BufferOffset;
	uint32 MinAlignment;
	VulkanRHI::FBufferSuballocation* BufferSuballocation;
};

struct FVulkanUniformBufferUploader : public VulkanRHI::FDeviceChild
{
public:
	FVulkanUniformBufferUploader(FVulkanDevice* InDevice, uint64 TotalSize);
	~FVulkanUniformBufferUploader();

	uint8* GetCPUMappedPointer()
	{
		return (uint8*)CPUBuffer->GetMappedPointer();
	}

	uint64 AllocateMemory(uint64 Size, uint32 Alignment)
	{
		return CPUBuffer->AllocateMemory(Size, Alignment);
	}

	VkBuffer GetCPUBufferHandle() const
	{
		return CPUBuffer->GetHandle();
	}

	inline uint32 GetCPUBufferOffset() const
	{
		return CPUBuffer->GetBufferOffset();
	}

protected:
	FVulkanRingBuffer* CPUBuffer;
	FVulkanRingBuffer* GPUBuffer;
	friend class FVulkanCommandListContext;
};

class FVulkanResourceMultiBuffer : public VulkanRHI::FDeviceChild
{
public:
	FVulkanResourceMultiBuffer(FVulkanDevice* InDevice, VkBufferUsageFlags InBufferUsageFlags, uint32 InSize, uint32 InUEUsage, FRHIResourceCreateInfo& CreateInfo);
	virtual ~FVulkanResourceMultiBuffer();

	inline VkBuffer GetHandle() const
	{
		if (NumBuffers == 0)
		{
			return VolatileLockInfo.GetHandle();
		}
		return Buffers[DynamicBufferIndex]->GetHandle();
	}

	inline bool IsDynamic() const
	{
		return NumBuffers > 1;
	}

	inline int32 GetDynamicIndex() const
	{
		return DynamicBufferIndex;
	}

	inline bool IsVolatile() const
	{
		return NumBuffers == 0;
	}

	inline uint32 GetVolatileLockCounter() const
	{
		check(IsVolatile());
		return VolatileLockInfo.LockCounter;
	}

	inline int32 GetNumBuffers() const
	{
		return NumBuffers;
	}

	// Offset used for Binding a VkBuffer
	inline uint32 GetOffset() const
	{
		if (NumBuffers == 0)
		{
			return VolatileLockInfo.GetBindOffset();
		}
		return Buffers[DynamicBufferIndex]->GetOffset();
	}

	inline VkBufferUsageFlags GetBufferUsageFlags() const
	{
		return BufferUsageFlags;
	}

	void* Lock(bool bFromRenderingThread, EResourceLockMode LockMode, uint32 Size, uint32 Offset);
	void Unlock(bool bFromRenderingThread);

protected:
	uint32 UEUsage;
	VkBufferUsageFlags BufferUsageFlags;
	uint32 NumBuffers;
	uint32 DynamicBufferIndex;
	TArray<TRefCountPtr<VulkanRHI::FBufferSuballocation>> Buffers;
	VulkanRHI::FTempFrameAllocationBuffer::FTempAllocInfo VolatileLockInfo;

	static void InternalUnlock(FVulkanCommandListContext& Context, VulkanRHI::FPendingBufferLock& PendingLock, FVulkanResourceMultiBuffer* MultiBuffer, int32 InDynamicBufferIndex);

	friend class FVulkanCommandListContext;
	friend struct FRHICommandMultiBufferUnlock;
};

class FVulkanIndexBuffer : public FRHIIndexBuffer, public FVulkanResourceMultiBuffer
{
public:
	FVulkanIndexBuffer(FVulkanDevice* InDevice, uint32 InStride, uint32 InSize, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);

	inline VkIndexType GetIndexType() const
	{
		return IndexType;
	}

private:
	VkIndexType IndexType;
};

class FVulkanVertexBuffer : public FRHIVertexBuffer, public FVulkanResourceMultiBuffer
{
public:
	FVulkanVertexBuffer(FVulkanDevice* InDevice, uint32 InSize, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);
};

class FVulkanUniformBuffer : public FRHIUniformBuffer, public FVulkanResourceMultiBuffer
{
public:
	FVulkanUniformBuffer(FVulkanDevice& Device, const FRHIUniformBufferLayout& InLayout, const void* Contents, EUniformBufferUsage Usage);

	~FVulkanUniformBuffer();

	TArray<uint8> ConstantData;

	const TArray<TRefCountPtr<FRHIResource>>& GetResourceTable() const { return ResourceTable; }

private:
	TArray<TRefCountPtr<FRHIResource>> ResourceTable;
};


class FVulkanStructuredBuffer : public FRHIStructuredBuffer, public FVulkanResourceMultiBuffer
{
public:
	FVulkanStructuredBuffer(FVulkanDevice* InDevice, uint32 Stride, uint32 Size, FRHIResourceCreateInfo& CreateInfo, uint32 InUsage);

	~FVulkanStructuredBuffer();

};



class FVulkanUnorderedAccessView : public FRHIUnorderedAccessView, public VulkanRHI::FDeviceChild
{
public:
	// the potential resources to refer to with the UAV object
	TRefCountPtr<FVulkanStructuredBuffer> SourceStructuredBuffer;

	FVulkanUnorderedAccessView(FVulkanDevice* Device)
		: VulkanRHI::FDeviceChild(Device)
		, MipLevel(0)
		, BufferViewFormat(PF_Unknown)
		, VolatileLockCounter(MAX_uint32)
	{
	}

	~FVulkanUnorderedAccessView();

	void UpdateView();

	// The texture that this UAV come from
	TRefCountPtr<FRHITexture> SourceTexture;
	FVulkanTextureView TextureView;
	uint32 MipLevel;

	// The vertex buffer this UAV comes from (can be null)
	TRefCountPtr<FVulkanVertexBuffer> SourceVertexBuffer;
	TRefCountPtr<FVulkanIndexBuffer> SourceIndexBuffer;
	TRefCountPtr<FVulkanBufferView> BufferView;
	EPixelFormat BufferViewFormat;

protected:
	// Used to check on volatile buffers if a new BufferView is required
	uint32 VolatileLockCounter;
};


class FVulkanShaderResourceView : public FRHIShaderResourceView, public VulkanRHI::FDeviceChild
{
public:
	FVulkanShaderResourceView(FVulkanDevice* Device, FVulkanResourceMultiBuffer* InSourceBuffer, uint32 InSize, EPixelFormat InFormat);

	FVulkanShaderResourceView(FVulkanDevice* Device, FRHITexture* InSourceTexture, uint32 InMipLevel, int32 InNumMips, EPixelFormat InFormat)
		: VulkanRHI::FDeviceChild(Device)
		, BufferViewFormat(InFormat)
		, SourceTexture(InSourceTexture)
		, SourceStructuredBuffer(nullptr)
		, MipLevel(InMipLevel)
		, NumMips(InNumMips)
		, Size(0)
		, SourceBuffer(nullptr)
		, VolatileLockCounter(MAX_uint32)
	{
	}

	FVulkanShaderResourceView(FVulkanDevice* Device, FVulkanStructuredBuffer* InStructuredBuffer)
		: VulkanRHI::FDeviceChild(Device)
		, BufferViewFormat(PF_Unknown)
		, SourceTexture(nullptr)
		, SourceStructuredBuffer(InStructuredBuffer)
		, MipLevel(0)
		, NumMips(0)
		, Size(InStructuredBuffer->GetSize())
		, SourceBuffer(nullptr)
		, VolatileLockCounter(MAX_uint32)
	{
	}


	void UpdateView();

	inline FVulkanBufferView* GetBufferView()
	{
		return BufferViews[BufferIndex];
	}

	EPixelFormat BufferViewFormat;

	// The texture that this SRV come from
	TRefCountPtr<FRHITexture> SourceTexture;
	FVulkanTextureView TextureView;
	FVulkanStructuredBuffer* SourceStructuredBuffer;
	uint32 MipLevel;
	uint32 NumMips;

	~FVulkanShaderResourceView();

	TArray<TRefCountPtr<FVulkanBufferView>> BufferViews;
	uint32 BufferIndex = 0;
	uint32 Size;
	// The buffer this SRV comes from (can be null)
	FVulkanResourceMultiBuffer* SourceBuffer;

protected:
	// Used to check on volatile buffers if a new BufferView is required
	uint32 VolatileLockCounter;
};

class FVulkanComputeFence : public FRHIComputeFence, public VulkanRHI::FGPUEvent
{
public:
	FVulkanComputeFence(FVulkanDevice* InDevice, FName InName);
	virtual ~FVulkanComputeFence();

	void WriteCmd(VkCommandBuffer CmdBuffer);
};


class FVulkanVertexInputStateInfo
{
public:
	FVulkanVertexInputStateInfo();

	void Generate(FVulkanVertexDeclaration* VertexDeclaration, uint32 VertexHeaderInOutAttributeMask);

	inline uint32 GetHash() const
	{
		check(Info.sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
		return Hash;
	}

	inline const VkPipelineVertexInputStateCreateInfo& GetInfo() const
	{
		return Info;
	}

protected:
	VkPipelineVertexInputStateCreateInfo Info;
	uint32 Hash;

	uint32 BindingsNum;
	uint32 BindingsMask;

	//#todo-rco: Remove these TMaps
	TMap<uint32, uint32> BindingToStream;
	TMap<uint32, uint32> StreamToBinding;
	VkVertexInputBindingDescription Bindings[MaxVertexElementCount];

	uint32 AttributesNum;
	VkVertexInputAttributeDescription Attributes[MaxVertexElementCount];

	friend class FVulkanPendingGfxState;
	friend class FVulkanPipelineStateCache;
};

// This class holds the staging area for packed global uniform buffers for a given shader
class FPackedUniformBuffers
{
public:
	// One buffer is a chunk of bytes
	typedef TArray<uint8> FPackedBuffer;

	FPackedUniformBuffers()
		: CodeHeader(nullptr)
	{
	}

	void Init(const FVulkanCodeHeader* InCodeHeader, uint64& OutPackedUniformBufferStagingMask, uint64& OutUniformBuffersWithDataMask)
	{
		CodeHeader = InCodeHeader;
		PackedUniformBuffers.AddDefaulted(CodeHeader->NEWPackedGlobalUBSizes.Num());
		for (int32 Index = 0; Index < CodeHeader->NEWPackedGlobalUBSizes.Num(); ++Index)
		{
			PackedUniformBuffers[Index].AddUninitialized(CodeHeader->NEWPackedGlobalUBSizes[Index]);
		}

		OutPackedUniformBufferStagingMask = ((uint64)1 << (uint64)CodeHeader->NEWPackedGlobalUBSizes.Num()) - 1;
		OutUniformBuffersWithDataMask = InCodeHeader->UniformBuffersWithDescriptorMask;
	}

	inline void SetPackedGlobalParameter(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue, uint64& InOutPackedUniformBufferStagingDirty)
	{
		FPackedBuffer& StagingBuffer = PackedUniformBuffers[BufferIndex];
		check(ByteOffset + NumBytes <= (uint32)StagingBuffer.Num());
		FMemory::Memcpy(StagingBuffer.GetData() + ByteOffset, NewValue, NumBytes);
		InOutPackedUniformBufferStagingDirty = InOutPackedUniformBufferStagingDirty | ((uint64)1 << (uint64)BufferIndex);
	}

	// Copies a 'real' constant buffer into the packed globals uniform buffer (only the used ranges)
	inline void SetEmulatedUniformBufferIntoPacked(uint32 BindPoint, const TArray<uint8>& ConstantData, uint64& NEWPackedUniformBufferStagingDirty)
	{
		// Emulated UBs. Assumes UniformBuffersCopyInfo table is sorted by CopyInfo.SourceUBIndex
		if (BindPoint < (uint32)CodeHeader->NEWEmulatedUBCopyRanges.Num())
		{
			uint32 Range = CodeHeader->NEWEmulatedUBCopyRanges[BindPoint];
			uint16 Start = (Range >> 16) & 0xffff;
			uint16 Count = Range & 0xffff;
			for (int32 Index = Start; Index < Start + Count; ++Index)
			{
				const CrossCompiler::FUniformBufferCopyInfo& CopyInfo = CodeHeader->UniformBuffersCopyInfo[Index];
				check(CopyInfo.SourceUBIndex == BindPoint);
				FPackedBuffer& StagingBuffer = PackedUniformBuffers[(int32)CopyInfo.DestUBIndex];
				//check(ByteOffset + NumBytes <= (uint32)StagingBuffer.Num());
				FMemory::Memcpy(StagingBuffer.GetData() + CopyInfo.DestOffsetInFloats * 4, ConstantData.GetData() + CopyInfo.SourceOffsetInFloats * 4, CopyInfo.SizeInFloats * 4);
				NEWPackedUniformBufferStagingDirty = NEWPackedUniformBufferStagingDirty | ((uint64)1 << (uint64)CopyInfo.DestUBIndex);
			}
		}
	}

	inline const FPackedBuffer& GetBuffer(int32 Index) const
	{
		return PackedUniformBuffers[Index];
	}

protected:
	const FVulkanCodeHeader* CodeHeader;
	TArray<FPackedBuffer> PackedUniformBuffers;
};




template<class T>
struct TVulkanResourceTraits
{
};
template<>
struct TVulkanResourceTraits<FRHIVertexDeclaration>
{
	typedef FVulkanVertexDeclaration TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIVertexShader>
{
	typedef FVulkanVertexShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIGeometryShader>
{
	typedef FVulkanGeometryShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIHullShader>
{
	typedef FVulkanHullShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIDomainShader>
{
	typedef FVulkanDomainShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIPixelShader>
{
	typedef FVulkanPixelShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIComputeShader>
{
	typedef FVulkanComputeShader TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHITexture3D>
{
	typedef FVulkanTexture3D TConcreteType;
};
//template<>
//struct TVulkanResourceTraits<FRHITexture>
//{
//	typedef FVulkanTexture TConcreteType;
//};
template<>
struct TVulkanResourceTraits<FRHITexture2D>
{
	typedef FVulkanTexture2D TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHITexture2DArray>
{
	typedef FVulkanTexture2DArray TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHITextureCube>
{
	typedef FVulkanTextureCube TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIRenderQuery>
{
	typedef FVulkanRenderQuery TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIUniformBuffer>
{
	typedef FVulkanUniformBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIIndexBuffer>
{
	typedef FVulkanIndexBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIStructuredBuffer>
{
	typedef FVulkanStructuredBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIVertexBuffer>
{
	typedef FVulkanVertexBuffer TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIShaderResourceView>
{
	typedef FVulkanShaderResourceView TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIUnorderedAccessView>
{
	typedef FVulkanUnorderedAccessView TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHISamplerState>
{
	typedef FVulkanSamplerState TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIRasterizerState>
{
	typedef FVulkanRasterizerState TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIDepthStencilState>
{
	typedef FVulkanDepthStencilState TConcreteType;
};
template<>
struct TVulkanResourceTraits<FRHIBlendState>
{
	typedef FVulkanBlendState TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHIComputeFence>
{
	typedef FVulkanComputeFence TConcreteType;
};

template<>
struct TVulkanResourceTraits<FRHIBoundShaderState>
{
	typedef FVulkanBoundShaderState TConcreteType;
};

template<typename TRHIType>
static FORCEINLINE typename TVulkanResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
{
	return static_cast<typename TVulkanResourceTraits<TRHIType>::TConcreteType*>(Resource);
}

template<typename TRHIType>
static FORCEINLINE typename TVulkanResourceTraits<TRHIType>::TConcreteType* ResourceCast(const TRHIType* Resource)
{
	return static_cast<const typename TVulkanResourceTraits<TRHIType>::TConcreteType*>(Resource);
}
