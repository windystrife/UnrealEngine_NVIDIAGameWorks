// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12VertexBuffer.cpp: D3D texture RHI implementation.
	=============================================================================*/

#include "D3D12RHIPrivate.h"

 
int64 FD3D12GlobalStats::GDedicatedVideoMemory = 0;
int64 FD3D12GlobalStats::GDedicatedSystemMemory = 0;
int64 FD3D12GlobalStats::GSharedSystemMemory = 0;
int64 FD3D12GlobalStats::GTotalGraphicsMemory = 0;

int32 GAdjustTexturePoolSizeBasedOnBudget = 0;
static FAutoConsoleVariableRef CVarAdjustTexturePoolSizeBasedOnBudget(
	TEXT("D3D12.AdjustTexturePoolSizeBasedOnBudget"),
	GAdjustTexturePoolSizeBasedOnBudget,
	TEXT("Indicates if the RHI should lower the texture pool size when the application is over the memory budget provided by the OS. This can result in lower quality textures (but hopefully improve performance).")
	);


static TAutoConsoleVariable<int32> CVarUseUpdateTexture3DComputeShader(
	TEXT("D3D12.UseUpdateTexture3DComputeShader"),
	PLATFORM_XBOXONE,
	TEXT("If enabled, use a compute shader for UpdateTexture3D. Avoids alignment restrictions")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: on"),
	ECVF_RenderThreadSafe );

// Forward Decls for template types
template TD3D12Texture2D<FD3D12BaseTexture2D>::TD3D12Texture2D(class FD3D12Device* InParent, uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples,
	EPixelFormat InFormat, bool bInCubemap, uint32 InFlags, const FClearValueBinding& InClearValue, const FD3D12TextureLayout* InTextureLayout
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	, void* InRawTextureMemory
#endif
	);
template TD3D12Texture2D<FD3D12BaseTexture2DArray>::TD3D12Texture2D(class FD3D12Device* InParent, uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples,
	EPixelFormat InFormat, bool bInCubemap, uint32 InFlags, const FClearValueBinding& InClearValue, const FD3D12TextureLayout* InTextureLayout
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	, void* InRawTextureMemory
#endif
	);
template TD3D12Texture2D<FD3D12BaseTextureCube>::TD3D12Texture2D(class FD3D12Device* InParent, uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 InNumMips, uint32 InNumSamples,
	EPixelFormat InFormat, bool bInCubemap, uint32 InFlags, const FClearValueBinding& InClearValue, const FD3D12TextureLayout* InTextureLayout
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	, void* InRawTextureMemory
#endif
	);

/// @cond DOXYGEN_WARNINGS

template void FD3D12TextureStats::D3D12TextureAllocated(TD3D12Texture2D<FD3D12BaseTexture2D>& Texture);
template void FD3D12TextureStats::D3D12TextureAllocated(TD3D12Texture2D<FD3D12BaseTexture2DArray>& Texture);
template void FD3D12TextureStats::D3D12TextureAllocated(TD3D12Texture2D<FD3D12BaseTextureCube>& Texture);

template void FD3D12TextureStats::D3D12TextureDeleted(TD3D12Texture2D<FD3D12BaseTexture2D>& Texture);
template void FD3D12TextureStats::D3D12TextureDeleted(TD3D12Texture2D<FD3D12BaseTexture2DArray>& Texture);
template void FD3D12TextureStats::D3D12TextureDeleted(TD3D12Texture2D<FD3D12BaseTextureCube>& Texture);

/// @endcond

struct FRHICommandUpdateTexture : public FRHICommand<FRHICommandUpdateTexture>
{
	FD3D12TextureBase* TextureBase;
	const D3D12_TEXTURE_COPY_LOCATION DestCopyLocation;
	uint32 DestX;
	uint32 DestY;
	uint32 DestZ;
	FD3D12ResourceLocation SourceCopyLocation;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT SourceFootprint;

	FORCEINLINE_DEBUGGABLE FRHICommandUpdateTexture(FD3D12TextureBase* InTextureBase, const D3D12_TEXTURE_COPY_LOCATION& InDestCopyLocation, uint32 InDestX, uint32 InDestY, uint32 InDestZ, FD3D12ResourceLocation& Source, D3D12_PLACED_SUBRESOURCE_FOOTPRINT& InSourceFootprint)
		: TextureBase(InTextureBase)
		, DestCopyLocation(InDestCopyLocation)
		, DestX(InDestX)
		, DestY(InDestY)
		, DestZ(InDestZ)
		, SourceCopyLocation(nullptr)
		, SourceFootprint(InSourceFootprint)
	{
		DestCopyLocation.pResource->AddRef();
		FD3D12ResourceLocation::TransferOwnership(SourceCopyLocation, Source);
	}

	~FRHICommandUpdateTexture()
	{
		DestCopyLocation.pResource->Release();

	}

	void Execute(FRHICommandListBase& CmdList)
	{
		CD3DX12_TEXTURE_COPY_LOCATION Location(SourceCopyLocation.GetResource()->GetResource(), SourceFootprint);

		TextureBase->UpdateTexture(DestCopyLocation, DestX, DestY, DestZ, Location);
	}
};

///////////////////////////////////////////////////////////////////////////////////////////
// Texture Stats
///////////////////////////////////////////////////////////////////////////////////////////

bool FD3D12TextureStats::ShouldCountAsTextureMemory(uint32 MiscFlags)
{
	// Shouldn't be used for DEPTH, RENDER TARGET, or UNORDERED ACCESS
	return (0 == (MiscFlags & (D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)));
}

// @param b3D true:3D, false:2D or cube map
TStatId FD3D12TextureStats::GetD3D12StatEnum(uint32 MiscFlags, bool bCubeMap, bool b3D)
{
#if STATS
	if (ShouldCountAsTextureMemory(MiscFlags))
	{
		// normal texture
		if (bCubeMap)
		{
			return GET_STATID(STAT_TextureMemoryCube);
		}
		else if (b3D)
		{
			return GET_STATID(STAT_TextureMemory3D);
		}
		else
		{
			return GET_STATID(STAT_TextureMemory2D);
		}
	}
	else
	{
		// render target
		if (bCubeMap)
		{
			return GET_STATID(STAT_RenderTargetMemoryCube);
		}
		else if (b3D)
		{
			return GET_STATID(STAT_RenderTargetMemory3D);
		}
		else
		{
			return GET_STATID(STAT_RenderTargetMemory2D);
		}
	}
#endif
	return TStatId();
}

// Note: This function can be called from many different threads
// @param TextureSize >0 to allocate, <0 to deallocate
// @param b3D true:3D, false:2D or cube map
void FD3D12TextureStats::UpdateD3D12TextureStats(const D3D12_RESOURCE_DESC& Desc, int64 TextureSize, bool b3D, bool bCubeMap)
{
	if (TextureSize == 0)
	{
		return;
	}

	const int64 AlignedSize = (TextureSize > 0) ? Align(TextureSize, 1024) / 1024 : -(Align(-TextureSize, 1024) / 1024);
	if (ShouldCountAsTextureMemory(Desc.Flags))
	{
		FPlatformAtomics::InterlockedAdd(&GCurrentTextureMemorySize, AlignedSize);
	}
	else
	{
		FPlatformAtomics::InterlockedAdd(&GCurrentRendertargetMemorySize, AlignedSize);
	}

	INC_MEMORY_STAT_BY_FName(GetD3D12StatEnum(Desc.Flags, bCubeMap, b3D).GetName(), TextureSize);

	if (TextureSize > 0)
	{
		INC_DWORD_STAT(STAT_D3D12TexturesAllocated);
	}
	else
	{
		INC_DWORD_STAT(STAT_D3D12TexturesReleased);
	}
}

template<typename BaseResourceType>
void FD3D12TextureStats::D3D12TextureAllocated(TD3D12Texture2D<BaseResourceType>& Texture)
{
	FD3D12Resource* D3D12Texture2D = Texture.GetResource();

	// Ignore placed textures as their memory is already allocated and accounted for
	if (D3D12Texture2D && D3D12Texture2D->IsPlacedResource() == false)
	{
		if ((Texture.Flags & TexCreate_Virtual) == TexCreate_Virtual)
		{
			Texture.SetMemorySize(0);
		}
		else
		{
			const D3D12_RESOURCE_DESC& Desc = D3D12Texture2D->GetDesc();
			const D3D12_RESOURCE_ALLOCATION_INFO AllocationInfo = Texture.GetParentDevice()->GetDevice()->GetResourceAllocationInfo(0, 1, &Desc);
			const int64 TextureSize = AllocationInfo.SizeInBytes;

			Texture.SetMemorySize(TextureSize);

			UpdateD3D12TextureStats(Desc, TextureSize, false, Texture.IsCubemap());
		}
	}
}

template<typename BaseResourceType>
void FD3D12TextureStats::D3D12TextureDeleted(TD3D12Texture2D<BaseResourceType>& Texture)
{
	FD3D12Resource* D3D12Texture2D = Texture.GetResource();

	// Ignore placed textures as their memory is already allocated and accounted for
	if (D3D12Texture2D && D3D12Texture2D->IsPlacedResource() == false)
	{
		const D3D12_RESOURCE_DESC& Desc = D3D12Texture2D->GetDesc();
		const int64 TextureSize = Texture.GetMemorySize();
		check(TextureSize > 0 || (Texture.Flags & TexCreate_Virtual));

		UpdateD3D12TextureStats(Desc, -TextureSize, false, Texture.IsCubemap());
	}
}

void FD3D12TextureStats::D3D12TextureAllocated2D(FD3D12Texture2D& Texture)
{
	D3D12TextureAllocated(Texture);
}

void FD3D12TextureStats::D3D12TextureAllocated(FD3D12Texture3D& Texture)
{
	FD3D12Resource* D3D12Texture3D = Texture.GetResource();

	if (D3D12Texture3D)
	{
		const D3D12_RESOURCE_DESC& Desc = D3D12Texture3D->GetDesc();
		const D3D12_RESOURCE_ALLOCATION_INFO AllocationInfo = Texture.GetParentDevice()->GetDevice()->GetResourceAllocationInfo(0, 1, &Desc);
		const int64 TextureSize = AllocationInfo.SizeInBytes;

		Texture.SetMemorySize(TextureSize);

		UpdateD3D12TextureStats(Desc, TextureSize, true, false);
	}
}

void FD3D12TextureStats::D3D12TextureDeleted(FD3D12Texture3D& Texture)
{
	FD3D12Resource* D3D12Texture3D = Texture.GetResource();

	if (D3D12Texture3D)
	{
		const D3D12_RESOURCE_DESC& Desc = D3D12Texture3D->GetDesc();
		const int64 TextureSize = Texture.GetMemorySize();
		if (TextureSize > 0)
		{
			UpdateD3D12TextureStats(Desc, -TextureSize, true, false);
		}
	}
}

using namespace D3D12RHI;

template<typename BaseResourceType>
TD3D12Texture2D<BaseResourceType>::~TD3D12Texture2D()
{
	if (GetParentDevice()->GetNodeMask() == GDefaultGPUMask)
	{
		// Only call this once for a LDA chain
		FD3D12TextureStats::D3D12TextureDeleted(*this);
	}
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	GetParentDevice()->GetOwningRHI()->DestroyVirtualTexture(GetFlags(), GetRawTextureMemory(), GetMemorySize());
#endif

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	//Make sure the RHI isn't deleted. This can happen sometimes on exit
	if (GDynamicRHI)
	{
		NVRHI::FRendererInterfaceD3D12* VxgiRenderer = ((FD3D12DynamicRHI*)GDynamicRHI)->VxgiRendererD3D12;
		if (VxgiRenderer)
			VxgiRenderer->forgetAboutTexture(this);
	}
#endif
	// NVCHANGE_END: Add VXGI
}

FD3D12Texture3D::~FD3D12Texture3D()
{
	if (GetParentDevice()->GetNodeMask() == GDefaultGPUMask)
	{
		// Only call this once for a LDA chain
		FD3D12TextureStats::D3D12TextureDeleted(*this);
	}

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	//Make sure the RHI isn't deleted. This can happen sometimes on exit
	if (GDynamicRHI)
	{
		NVRHI::FRendererInterfaceD3D12* VxgiRenderer = ((FD3D12DynamicRHI*)GDynamicRHI)->VxgiRendererD3D12;
		if (VxgiRenderer)
			VxgiRenderer->forgetAboutTexture(this);
	}

#endif
	// NVCHANGE_END: Add VXGI
}

uint64 FD3D12DynamicRHI::RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
{
	D3D12_RESOURCE_DESC Desc = {};
	Desc.DepthOrArraySize = 1;
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	Desc.Format = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;
	Desc.Height = SizeY;
	Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	Desc.MipLevels = NumMips;
	Desc.SampleDesc.Count = NumSamples;
	Desc.Width = SizeX;

	const D3D12_RESOURCE_ALLOCATION_INFO AllocationInfo = GetRHIDevice()->GetDevice()->GetResourceAllocationInfo(0, 1, &Desc);
	OutAlign = static_cast<uint32>(AllocationInfo.Alignment);

	return AllocationInfo.SizeInBytes;
}

uint64 FD3D12DynamicRHI::RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	D3D12_RESOURCE_DESC Desc = {};
	Desc.DepthOrArraySize = SizeZ;
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	Desc.Format = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;
	Desc.Height = SizeY;
	Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	Desc.MipLevels = NumMips;
	Desc.SampleDesc.Count = 1;
	Desc.Width = SizeX;

	const D3D12_RESOURCE_ALLOCATION_INFO AllocationInfo = GetRHIDevice()->GetDevice()->GetResourceAllocationInfo(0, 1, &Desc);
	OutAlign = static_cast<uint32>(AllocationInfo.Alignment);

	return AllocationInfo.SizeInBytes;
}

uint64 FD3D12DynamicRHI::RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	D3D12_RESOURCE_DESC Desc = {};
	Desc.DepthOrArraySize = 6;
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	Desc.Format = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;
	Desc.Height = Size;
	Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	Desc.MipLevels = NumMips;
	Desc.SampleDesc.Count = 1;
	Desc.Width = Size;

	const D3D12_RESOURCE_ALLOCATION_INFO AllocationInfo = GetRHIDevice()->GetDevice()->GetResourceAllocationInfo(0, 1, &Desc);
	OutAlign = static_cast<uint32>(AllocationInfo.Alignment);

	return AllocationInfo.SizeInBytes;
}

/**
 * Retrieves texture memory stats.
 */
void FD3D12DynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	OutStats.DedicatedVideoMemory = FD3D12GlobalStats::GDedicatedVideoMemory;
	OutStats.DedicatedSystemMemory = FD3D12GlobalStats::GDedicatedSystemMemory;
	OutStats.SharedSystemMemory = FD3D12GlobalStats::GSharedSystemMemory;
	OutStats.TotalGraphicsMemory = FD3D12GlobalStats::GTotalGraphicsMemory ? FD3D12GlobalStats::GTotalGraphicsMemory : -1;

	OutStats.AllocatedMemorySize = int64(GCurrentTextureMemorySize) * 1024;
	OutStats.LargestContiguousAllocation = OutStats.AllocatedMemorySize;
	OutStats.TexturePoolSize = GTexturePoolSize;
	OutStats.PendingMemoryAdjustment = 0;

#if PLATFORM_WINDOWS
	if (GAdjustTexturePoolSizeBasedOnBudget)
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO LocalVideoMemoryInfo;
		GetAdapter().GetCurrentDevice()->GetLocalVideoMemoryInfo(&LocalVideoMemoryInfo);

		// Applications must explicitly manage their usage of physical memory and keep usage within the budget 
		// assigned to the application process. Processes that cannot keep their usage within their assigned budgets 
		// will likely experience stuttering, as they are intermittently frozen and paged out to allow other processes to run.
		const int64 TargetBudget = LocalVideoMemoryInfo.Budget * 0.90f;	// Target using 90% of our budget to account for some fragmentation.
		OutStats.TotalGraphicsMemory = TargetBudget;

		const int64 BudgetPadding = TargetBudget * 0.05f;
		const int64 AvailableSpace = TargetBudget - int64(LocalVideoMemoryInfo.CurrentUsage);	// Note: AvailableSpace can be negative
		const int64 PreviousTexturePoolSize = RequestedTexturePoolSize;
		const bool bOverbudget = AvailableSpace < 0;

		// Only change the pool size if overbudget, or a reasonable amount of memory is available
		const int64 MinTexturePoolSize = int64(100 * 1024 * 1024);
		if (bOverbudget)
		{
			// Attempt to lower the texture pool size to meet the budget.
			const bool bOverActualBudget = LocalVideoMemoryInfo.CurrentUsage > LocalVideoMemoryInfo.Budget;
			UE_CLOG(bOverActualBudget, LogD3D12RHI, Warning,
				TEXT("Video memory usage is overbudget by %llu MB (using %lld MB/%lld MB budget). Usage breakdown: %lld MB (Textures), %lld MB (Render targets). Last requested texture pool size is %lld MB. This can cause stuttering due to paging."),
				(LocalVideoMemoryInfo.CurrentUsage - LocalVideoMemoryInfo.Budget) / 1024ll / 1024ll,
				LocalVideoMemoryInfo.CurrentUsage / 1024ll / 1024ll,
				LocalVideoMemoryInfo.Budget / 1024ll / 1024ll,
				GCurrentTextureMemorySize / 1024ll,
				GCurrentRendertargetMemorySize / 1024ll,
				PreviousTexturePoolSize / 1024ll / 1024ll);

			const int64 DesiredTexturePoolSize = PreviousTexturePoolSize + AvailableSpace - BudgetPadding;
			OutStats.TexturePoolSize = FMath::Max(DesiredTexturePoolSize, MinTexturePoolSize);

			UE_CLOG(bOverActualBudget && (OutStats.TexturePoolSize >= PreviousTexturePoolSize) && (OutStats.TexturePoolSize > MinTexturePoolSize), LogD3D12RHI, Fatal,
				TEXT("Video memory usage is overbudget by %llu MB and the texture pool size didn't shrink."),
				(LocalVideoMemoryInfo.CurrentUsage - LocalVideoMemoryInfo.Budget) / 1024ll / 1024ll);
		}
		else if (AvailableSpace > BudgetPadding)
		{
			// Increase the texture pool size to improve quality if we have a reasonable amount of memory available.
			int64 DesiredTexturePoolSize = PreviousTexturePoolSize + AvailableSpace - BudgetPadding;
			if (GPoolSizeVRAMPercentage > 0)
			{
				// The texture pool size is a percentage of GTotalGraphicsMemory.
				const float PoolSize = float(GPoolSizeVRAMPercentage) * 0.01f * float(OutStats.TotalGraphicsMemory);

				// Truncate texture pool size to MB (but still counted in bytes).
				DesiredTexturePoolSize = int64(FGenericPlatformMath::TruncToFloat(PoolSize / 1024.0f / 1024.0f)) * 1024 * 1024;
			}

			// Make sure the desired texture pool size doesn't make us go overbudget.
			const bool bIsLimitedTexturePoolSize = GTexturePoolSize > 0;
			const int64 LimitedMaxTexturePoolSize = bIsLimitedTexturePoolSize ? GTexturePoolSize : INT64_MAX;
			const int64 MaxTexturePoolSize = FMath::Min(PreviousTexturePoolSize + AvailableSpace - BudgetPadding, LimitedMaxTexturePoolSize);	// Max texture pool size without going overbudget or the pre-defined max.
			OutStats.TexturePoolSize = FMath::Min(DesiredTexturePoolSize, MaxTexturePoolSize);
		}
		else
		{
			// Keep the previous requested texture pool size.
			OutStats.TexturePoolSize = PreviousTexturePoolSize;
		}

		check(OutStats.TexturePoolSize >= MinTexturePoolSize);
	}

	// Cache the last requested texture pool size.
	RequestedTexturePoolSize = OutStats.TexturePoolSize;
#endif // PLATFORM_WINDOWS
}

/**
 * Fills a texture with to visualize the texture pool memory.
 *
 * @param	TextureData		Start address
 * @param	SizeX			Number of pixels along X
 * @param	SizeY			Number of pixels along Y
 * @param	Pitch			Number of bytes between each row
 * @param	PixelSize		Number of bytes each pixel represents
 *
 * @return true if successful, false otherwise
 */
bool FD3D12DynamicRHI::RHIGetTextureMemoryVisualizeData(FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/)
{
	// currently only implemented for console (Note: Keep this function for further extension. Talk to NiklasS for more info.)
	return false;
}

/** If true, guard texture creates with SEH to log more information about a driver crash we are seeing during texture streaming. */
#define GUARDED_TEXTURE_CREATES (PLATFORM_WINDOWS && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))

/**
 * Creates a 2D texture optionally guarded by a structured exception handler.
 */
void SafeCreateTexture2D(FD3D12Device* pDevice, 
	FD3D12Adapter* Adapter,
	const D3D12_RESOURCE_DESC& TextureDesc,
	const D3D12_CLEAR_VALUE* ClearValue, 
	FD3D12ResourceLocation* OutTexture2D, 
	uint8 Format, 
	uint32 Flags,
	D3D12_RESOURCE_STATES InitialState)
{

#if GUARDED_TEXTURE_CREATES
	bool bDriverCrash = true;
	__try
	{
#endif // #if GUARDED_TEXTURE_CREATES

		const D3D12_HEAP_TYPE heapType = (Flags & TexCreate_CPUReadback) ? D3D12_HEAP_TYPE_READBACK : D3D12_HEAP_TYPE_DEFAULT;
		const uint64 BlockSizeX = GPixelFormats[Format].BlockSizeX;
		const uint64 BlockSizeY = GPixelFormats[Format].BlockSizeY;
		const uint64 BlockBytes = GPixelFormats[Format].BlockBytes;
		const uint64 MipSizeX = FMath::Max(TextureDesc.Width, BlockSizeX);
		const uint64 MipSizeY = FMath::Max((uint64)TextureDesc.Height, BlockSizeY);
		const uint64 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
		const uint64 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
		const uint64 XBytesAligned = Align(NumBlocksX * BlockBytes, FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
		const uint64 MipBytesAligned = Align(NumBlocksY * XBytesAligned, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

		switch (heapType)
		{
		case D3D12_HEAP_TYPE_READBACK:
			{
				FD3D12Resource* Resource = nullptr;
				VERIFYD3D12CREATETEXTURERESULT(
					Adapter->CreateBuffer(heapType, pDevice->GetNodeMask(), pDevice->GetVisibilityMask(), MipBytesAligned, &Resource),
					TextureDesc.Width,
					TextureDesc.Height,
					TextureDesc.DepthOrArraySize,
					TextureDesc.Format,
					TextureDesc.MipLevels,
					TextureDesc.Flags
					);
				OutTexture2D->AsStandAlone(Resource);

				if (IsCPUWritable(heapType))
				{
					OutTexture2D->SetMappedBaseAddress(Resource->Map());
				}
			}
			break;

		case D3D12_HEAP_TYPE_DEFAULT:
			VERIFYD3D12CREATETEXTURERESULT(
				pDevice->GetTextureAllocator().AllocateTexture(TextureDesc, ClearValue, Format, *OutTexture2D, InitialState),
				TextureDesc.Width,
				TextureDesc.Height,
				TextureDesc.DepthOrArraySize,
				TextureDesc.Format,
				TextureDesc.MipLevels,
				TextureDesc.Flags
				);

			break;

		default:
			check(false);	// Need to create a resource here
		}

#if GUARDED_TEXTURE_CREATES
		bDriverCrash = false;
	}
	__finally
	{
		if (bDriverCrash)
		{
			UE_LOG(LogD3D12RHI, Error,
				TEXT("Driver crashed while creating texture: %ux%ux%u %s(0x%08x) with %u mips"),
				TextureDesc.Width,
				TextureDesc.Height,
				TextureDesc.DepthOrArraySize,
				GetD3D12TextureFormatString(TextureDesc.Format),
				(uint32)TextureDesc.Format,
				TextureDesc.MipLevels
				);
		}
	}
#endif // #if GUARDED_TEXTURE_CREATES
}


template<typename BaseResourceType>
TD3D12Texture2D<BaseResourceType>* FD3D12DynamicRHI::CreateD3D12Texture2D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, bool bTextureArray, bool bCubeTexture, uint8 Format,
	uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
#if PLATFORM_WINDOWS
	check(SizeX > 0 && SizeY > 0 && NumMips > 0);

	if (bCubeTexture)
	{
		check(SizeX <= GetMaxCubeTextureDimension());
		check(SizeX == SizeY);
	}
	else
	{
		check(SizeX <= GetMax2DTextureDimension());
		check(SizeY <= GetMax2DTextureDimension());
	}

	if (bTextureArray)
	{
		check(SizeZ <= GetMaxTextureArrayLayers());
	}

	// Render target allocation with UAV flag will silently fail in feature level 10
	check(FeatureLevel >= D3D_FEATURE_LEVEL_11_0 || !(Flags & TexCreate_UAV));

	SCOPE_CYCLE_COUNTER(STAT_D3D12CreateTextureTime);

	if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES2)
	{
		// Remove sRGB read flag when not supported
		Flags &= ~TexCreate_SRGB;
	}

	const bool bSRGB = (Flags & TexCreate_SRGB) != 0;

	const DXGI_FORMAT PlatformResourceFormat = GetPlatformTextureResourceFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat, Flags);
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformDepthStencilFormat = FindDepthStencilDXGIFormat(PlatformResourceFormat);

	// Determine the MSAA settings to use for the texture.
	D3D12_DSV_DIMENSION DepthStencilViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	D3D12_RTV_DIMENSION RenderTargetViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	D3D12_SRV_DIMENSION ShaderResourceViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	bool bCreateShaderResource = true;

	uint32 ActualMSAACount = NumSamples;

	uint32 ActualMSAAQuality = GetMaxMSAAQuality(ActualMSAACount);

	// 0xffffffff means not supported
	if (ActualMSAAQuality == 0xffffffff || (Flags & TexCreate_Shared) != 0)
	{
		// no MSAA
		ActualMSAACount = 1;
		ActualMSAAQuality = 0;
	}

	if (ActualMSAACount > 1)
	{
		DepthStencilViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		RenderTargetViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		ShaderResourceViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
	}

	if (Flags & TexCreate_CPUReadback)
	{
		check(!(Flags & TexCreate_RenderTargetable));
		check(!(Flags & TexCreate_DepthStencilTargetable));
		check(!(Flags & TexCreate_ShaderResource));
		bCreateShaderResource = false;
	}

	// Describe the texture.
	D3D12_RESOURCE_DESC TextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		PlatformResourceFormat,
		SizeX,
		SizeY,
		SizeZ,  // Array size
		NumMips,
		ActualMSAACount,
		ActualMSAAQuality,
		D3D12_RESOURCE_FLAG_NONE);  // Add misc flags later

	// Set up the texture bind flags.
	bool bCreateRTV = false;
	bool bCreateDSV = false;

	if (Flags & TexCreate_RenderTargetable)
	{
		check(!(Flags & TexCreate_DepthStencilTargetable));
		check(!(Flags & TexCreate_ResolveTargetable));
		TextureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		bCreateRTV = true;
	}
	else if (Flags & TexCreate_DepthStencilTargetable)
	{
		check(!(Flags & TexCreate_RenderTargetable));
		check(!(Flags & TexCreate_ResolveTargetable));
		TextureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		bCreateDSV = true;
	}
	else if (Flags & TexCreate_ResolveTargetable)
	{
		check(!(Flags & TexCreate_RenderTargetable));
		check(!(Flags & TexCreate_DepthStencilTargetable));
		if (Format == PF_DepthStencil || Format == PF_ShadowDepth || Format == PF_D24)
		{
			TextureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			bCreateDSV = true;
		}
		else
		{
			TextureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			bCreateRTV = true;
		}
	}

	if (Flags & TexCreate_UAV)
	{
		TextureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	if (bCreateDSV && !(Flags & TexCreate_ShaderResource))
	{
		// Only deny shader resources if it's a depth resource that will never be used as SRV
		TextureDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		bCreateShaderResource = false;
	}

	Flags &= ~TexCreate_Virtual;

	FD3D12Adapter* Adapter = &GetAdapter();

	D3D12_CLEAR_VALUE *ClearValuePtr = nullptr;
	D3D12_CLEAR_VALUE ClearValue;
	if (bCreateDSV && CreateInfo.ClearValueBinding.ColorBinding == EClearBinding::EDepthStencilBound)
	{
		ClearValue = CD3DX12_CLEAR_VALUE(PlatformDepthStencilFormat, CreateInfo.ClearValueBinding.Value.DSValue.Depth, (uint8)CreateInfo.ClearValueBinding.Value.DSValue.Stencil);
		ClearValuePtr = &ClearValue;
	}
	else if (bCreateRTV && CreateInfo.ClearValueBinding.ColorBinding == EClearBinding::EColorBound)
	{
		ClearValue = CD3DX12_CLEAR_VALUE(PlatformRenderTargetFormat, CreateInfo.ClearValueBinding.Value.Color);
		ClearValuePtr = &ClearValue;
	}

	// The state this resource will be in when it leaves this function
	const FD3D12Resource::FD3D12ResourceTypeHelper Type(TextureDesc, D3D12_HEAP_TYPE_DEFAULT);
	const D3D12_RESOURCE_STATES DestinationState = Type.GetOptimalInitialState(false);

	TD3D12Texture2D<BaseResourceType>* D3D12TextureOut = Adapter->CreateLinkedObject<TD3D12Texture2D<BaseResourceType>>([&](FD3D12Device* Device)
	{
		TD3D12Texture2D<BaseResourceType>* NewTexture = new TD3D12Texture2D<BaseResourceType>(Device,
			SizeX,
			SizeY,
			SizeZ,
			NumMips,
			ActualMSAACount,
			(EPixelFormat)Format,
			bCubeTexture,
			Flags,
			CreateInfo.ClearValueBinding);

		FD3D12ResourceLocation& Location = NewTexture->ResourceLocation;

		SafeCreateTexture2D(Device,
			Adapter,
			TextureDesc,
			ClearValuePtr,
			&Location,
			Format,
			Flags,
			(CreateInfo.BulkData != NULL) ? D3D12_RESOURCE_STATE_COPY_DEST : DestinationState);

		uint32 RTVIndex = 0;

		if (bCreateRTV)
		{
			const bool bCreateRTVsPerSlice = (Flags & TexCreate_TargetArraySlicesIndependently) && (bTextureArray || bCubeTexture);
			NewTexture->SetNumRenderTargetViews(bCreateRTVsPerSlice ? NumMips * TextureDesc.DepthOrArraySize : NumMips);

			// Create a render target view for each mip
			for (uint32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
			{
				if (bCreateRTVsPerSlice)
				{
					NewTexture->SetCreatedRTVsPerSlice(true, TextureDesc.DepthOrArraySize);

					for (uint32 SliceIndex = 0; SliceIndex < TextureDesc.DepthOrArraySize; SliceIndex++)
					{
						D3D12_RENDER_TARGET_VIEW_DESC RTVDesc;
						FMemory::Memzero(&RTVDesc, sizeof(RTVDesc));
						RTVDesc.Format = PlatformRenderTargetFormat;
						RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
						RTVDesc.Texture2DArray.FirstArraySlice = SliceIndex;
						RTVDesc.Texture2DArray.ArraySize = 1;
						RTVDesc.Texture2DArray.MipSlice = MipIndex;
						RTVDesc.Texture2DArray.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, RTVDesc.Format);

						NewTexture->SetRenderTargetViewIndex(FD3D12RenderTargetView::CreateRenderTargetView(Device, &Location, RTVDesc), RTVIndex++);
					}
				}
				else
				{
					D3D12_RENDER_TARGET_VIEW_DESC RTVDesc;
					FMemory::Memzero(&RTVDesc, sizeof(RTVDesc));
					RTVDesc.Format = PlatformRenderTargetFormat;
					if (bTextureArray || bCubeTexture)
					{
						RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
						RTVDesc.Texture2DArray.FirstArraySlice = 0;
						RTVDesc.Texture2DArray.ArraySize = TextureDesc.DepthOrArraySize;
						RTVDesc.Texture2DArray.MipSlice = MipIndex;
						RTVDesc.Texture2DArray.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, RTVDesc.Format);
					}
					else
					{
						RTVDesc.ViewDimension = RenderTargetViewDimension;
						RTVDesc.Texture2D.MipSlice = MipIndex;
						RTVDesc.Texture2D.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, RTVDesc.Format);
					}

					NewTexture->SetRenderTargetViewIndex(FD3D12RenderTargetView::CreateRenderTargetView(Device, &Location, RTVDesc), RTVIndex++);
				}
			}
		}

		if (bCreateDSV)
		{
			// Create a depth-stencil-view for the texture.
			D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
			DSVDesc.Format = FindDepthStencilDXGIFormat(PlatformResourceFormat);
			if (bTextureArray || bCubeTexture)
			{
				DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				DSVDesc.Texture2DArray.FirstArraySlice = 0;
				DSVDesc.Texture2DArray.ArraySize = TextureDesc.DepthOrArraySize;
				DSVDesc.Texture2DArray.MipSlice = 0;
			}
			else
			{
				DSVDesc.ViewDimension = DepthStencilViewDimension;
				DSVDesc.Texture2D.MipSlice = 0;
			}

			const bool HasStencil = HasStencilBits(DSVDesc.Format);
			for (uint32 AccessType = 0; AccessType < FExclusiveDepthStencil::MaxIndex; ++AccessType)
			{
				// Create a read-only access views for the texture.
				DSVDesc.Flags = (AccessType & FExclusiveDepthStencil::DepthRead_StencilWrite) ? D3D12_DSV_FLAG_READ_ONLY_DEPTH : D3D12_DSV_FLAG_NONE;
				if (HasStencil)
				{
					DSVDesc.Flags |= (AccessType & FExclusiveDepthStencil::DepthWrite_StencilRead) ? D3D12_DSV_FLAG_READ_ONLY_STENCIL : D3D12_DSV_FLAG_NONE;
				}

				NewTexture->SetDepthStencilView(FD3D12DepthStencilView::CreateDepthStencilView(Device, &Location, DSVDesc, HasStencil), AccessType);
			}
		}

		if (Flags & TexCreate_CPUReadback)
		{
			const uint32 BlockBytes = GPixelFormats[Format].BlockBytes;
			const uint32 XBytesAligned = Align((uint32)SizeX * BlockBytes, FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
			D3D12_SUBRESOURCE_FOOTPRINT DestSubresource;
			DestSubresource.Depth = SizeZ;
			DestSubresource.Height = SizeY;
			DestSubresource.Width = SizeX;
			DestSubresource.Format = PlatformResourceFormat;
			DestSubresource.RowPitch = XBytesAligned;

			check(DestSubresource.RowPitch % FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);	// Make sure we align correctly.

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT PlacedTexture2D = { 0 };
			PlacedTexture2D.Offset = 0;
			PlacedTexture2D.Footprint = DestSubresource;

			NewTexture->SetReadBackHeapDesc(PlacedTexture2D);
		}

		// Create a shader resource view for the texture.
		if (bCreateShaderResource)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
			SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			SRVDesc.Format = PlatformShaderResourceFormat;

			if (bCubeTexture && bTextureArray)
			{
				SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
				SRVDesc.TextureCubeArray.MostDetailedMip = 0;
				SRVDesc.TextureCubeArray.MipLevels = NumMips;
				SRVDesc.TextureCubeArray.First2DArrayFace = 0;
				SRVDesc.TextureCubeArray.NumCubes = SizeZ / 6;
			}
			else if (bCubeTexture)
			{
				SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
				SRVDesc.TextureCube.MostDetailedMip = 0;
				SRVDesc.TextureCube.MipLevels = NumMips;
			}
			else if (bTextureArray)
			{
				SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				SRVDesc.Texture2DArray.MostDetailedMip = 0;
				SRVDesc.Texture2DArray.MipLevels = NumMips;
				SRVDesc.Texture2DArray.FirstArraySlice = 0;
				SRVDesc.Texture2DArray.ArraySize = TextureDesc.DepthOrArraySize;
				SRVDesc.Texture2DArray.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, SRVDesc.Format);
			}
			else
			{
				SRVDesc.ViewDimension = ShaderResourceViewDimension;
				SRVDesc.Texture2D.MostDetailedMip = 0;
				SRVDesc.Texture2D.MipLevels = NumMips;
				SRVDesc.Texture2D.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, SRVDesc.Format);
			}

			FD3D12ShaderResourceView* SRV = FD3D12ShaderResourceView::CreateShaderResourceView(Device, &Location, SRVDesc);
			NewTexture->SetShaderResourceView(SRV);
		}

		return NewTexture;
	});

	FD3D12TextureStats::D3D12TextureAllocated(*D3D12TextureOut);

	// Initialize if data is given
	if (CreateInfo.BulkData != NULL)
	{
		TArray<D3D12_SUBRESOURCE_DATA> SubResourceData;

		uint8* Data = (uint8*)CreateInfo.BulkData->GetResourceBulkData();

		// each mip of each array slice counts as a subresource
		SubResourceData.AddZeroed(NumMips * SizeZ);

		uint32 SliceOffset = 0;
		for (uint32 ArraySliceIndex = 0; ArraySliceIndex < SizeZ; ++ArraySliceIndex)
		{
			uint32 MipOffset = 0;
			for (uint32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
			{
				uint32 DataOffset = SliceOffset + MipOffset;
				uint32 SubResourceIndex = ArraySliceIndex * NumMips + MipIndex;

				uint32 NumBlocksX = FMath::Max<uint32>(1, (SizeX >> MipIndex) / GPixelFormats[Format].BlockSizeX);
				uint32 NumBlocksY = FMath::Max<uint32>(1, (SizeY >> MipIndex) / GPixelFormats[Format].BlockSizeY);

				SubResourceData[SubResourceIndex].pData = &Data[DataOffset];
				SubResourceData[SubResourceIndex].RowPitch = NumBlocksX * GPixelFormats[Format].BlockBytes;
				SubResourceData[SubResourceIndex].SlicePitch = NumBlocksX * NumBlocksY * SubResourceData[MipIndex].RowPitch;

				MipOffset += NumBlocksY * SubResourceData[MipIndex].RowPitch;
			}
			SliceOffset += MipOffset;
		}

		uint64 Size = GetRequiredIntermediateSize(D3D12TextureOut->GetResource()->GetResource(), 0, NumMips * SizeZ);

		auto& FastAllocator = Adapter->GetDevice()->GetDefaultFastAllocator();

		FD3D12ResourceLocation TempResourceLocation(FastAllocator.GetParentDevice());
		void* pData = FastAllocator.Allocate<FD3D12ScopeLock>(Size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &TempResourceLocation);

		// Begin with the root texture (i.e. device node 0)
		TD3D12Texture2D<BaseResourceType>* CurrentTexture = D3D12TextureOut;

		// Initialize all the textures in the chain
		while (CurrentTexture != nullptr)
		{
			FD3D12Device* Device = CurrentTexture->GetParentDevice();
			FD3D12CommandListHandle& hCommandList = Device->GetDefaultCommandContext().CommandListHandle;

			FD3D12Resource* Resource = CurrentTexture->GetResource();

			hCommandList.GetCurrentOwningContext()->numCopies++;
			UpdateSubresources(hCommandList.GraphicsCommandList(),
				Resource->GetResource(),
				TempResourceLocation.GetResource()->GetResource(),
				TempResourceLocation.GetOffsetFromBaseOfResource(),
				0, NumMips * SizeZ,
				SubResourceData.GetData());

			hCommandList.UpdateResidency(Resource);

			hCommandList.AddTransitionBarrier(Resource, D3D12_RESOURCE_STATE_COPY_DEST, DestinationState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

			CurrentTexture = (TD3D12Texture2D<BaseResourceType>*) CurrentTexture->GetNextObject();
		}
	}

	if (CreateInfo.BulkData)
	{
		CreateInfo.BulkData->Discard();
	}

	return D3D12TextureOut;
#else
	checkf(false, TEXT("XBOX_CODE_MERGE : Removed. The Xbox platform version should be used."));
	return nullptr;
#endif // PLATFORM_WINDOWS
}

FD3D12Texture3D* FD3D12DynamicRHI::CreateD3D12Texture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
#if PLATFORM_WINDOWS
	SCOPE_CYCLE_COUNTER(STAT_D3D12CreateTextureTime);

	const bool bSRGB = (Flags & TexCreate_SRGB) != 0;

	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);

	// Describe the texture.
	D3D12_RESOURCE_DESC TextureDesc = CD3DX12_RESOURCE_DESC::Tex3D(
		PlatformResourceFormat,
		SizeX,
		SizeY,
		SizeZ,
		NumMips);

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	if (TextureDesc.Format == DXGI_FORMAT_R32_FLOAT)
		TextureDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	else if (TextureDesc.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
		TextureDesc.Format = DXGI_FORMAT_R10G10B10A2_TYPELESS;
#endif
	// NVCHANGE_END: Add VXGI

	if (Flags & TexCreate_UAV)
	{
		TextureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	bool bCreateRTV = false;

	if (Flags & TexCreate_RenderTargetable)
	{
		TextureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		bCreateRTV = true;
	}

	// Set up the texture bind flags.
	check(!(Flags & TexCreate_DepthStencilTargetable));
	check(!(Flags & TexCreate_ResolveTargetable));
	check(Flags & TexCreate_ShaderResource);

	D3D12_CLEAR_VALUE *ClearValuePtr = nullptr;
	D3D12_CLEAR_VALUE ClearValue;
	if (bCreateRTV && CreateInfo.ClearValueBinding.ColorBinding == EClearBinding::EColorBound)
	{
		ClearValue = CD3DX12_CLEAR_VALUE(PlatformResourceFormat, CreateInfo.ClearValueBinding.Value.Color);
		ClearValuePtr = &ClearValue;
	}

	// The state this resource will be in when it leaves this function
	const FD3D12Resource::FD3D12ResourceTypeHelper Type(TextureDesc, D3D12_HEAP_TYPE_DEFAULT);
	const D3D12_RESOURCE_STATES DestinationState = Type.GetOptimalInitialState(false);

	FD3D12Adapter* Adapter = &GetAdapter();

	FD3D12Texture3D* D3D12TextureOut = Adapter->CreateLinkedObject<FD3D12Texture3D>([&](FD3D12Device* Device)
	{
		FD3D12Texture3D* Texture3D = new FD3D12Texture3D(Device, SizeX, SizeY, SizeZ, NumMips, (EPixelFormat)Format, Flags, CreateInfo.ClearValueBinding);

		HRESULT hr = Device->GetTextureAllocator().AllocateTexture(TextureDesc, ClearValuePtr, Format, Texture3D->ResourceLocation, (CreateInfo.BulkData != NULL) ? D3D12_RESOURCE_STATE_COPY_DEST : DestinationState);
		check(SUCCEEDED(hr));

		if (bCreateRTV)
		{
			// Create a render-target-view for the texture.
			D3D12_RENDER_TARGET_VIEW_DESC RTVDesc;
			FMemory::Memzero(&RTVDesc, sizeof(RTVDesc));
			RTVDesc.Format = PlatformRenderTargetFormat;
			RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
			RTVDesc.Texture3D.MipSlice = 0;
			RTVDesc.Texture3D.FirstWSlice = 0;
			RTVDesc.Texture3D.WSize = SizeZ;

			Texture3D->SetRenderTargetView(FD3D12RenderTargetView::CreateRenderTargetView(Device, &Texture3D->ResourceLocation, RTVDesc));
		}

		// Create a shader resource view for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.Format = PlatformShaderResourceFormat;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		SRVDesc.Texture3D.MipLevels = NumMips;
		SRVDesc.Texture3D.MostDetailedMip = 0;

		FD3D12ShaderResourceView* SRV = FD3D12ShaderResourceView::CreateShaderResourceView(Device, &Texture3D->ResourceLocation, SRVDesc);
		Texture3D->SetShaderResourceView(SRV);

		return Texture3D;
	});

	// Intialize if data given
	if (D3D12TextureOut)
	{
		if (CreateInfo.BulkData)
		{
			TArray<D3D12_SUBRESOURCE_DATA> SubResourceData;

			uint8* Data = (uint8*)CreateInfo.BulkData->GetResourceBulkData();
			SubResourceData.AddZeroed(NumMips);
			uint32 MipOffset = 0;
			for (uint32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
			{
				SubResourceData[MipIndex].pData = &Data[MipOffset];
				SubResourceData[MipIndex].RowPitch = FMath::Max<uint32>(1, SizeX >> MipIndex) * GPixelFormats[Format].BlockBytes;
				SubResourceData[MipIndex].SlicePitch = FMath::Max<uint32>(1, SizeY >> MipIndex) * SubResourceData[MipIndex].RowPitch;
				MipOffset += FMath::Max<uint32>(1, SizeZ >> MipIndex) * SubResourceData[MipIndex].SlicePitch;
			}

			const uint64 Size = GetRequiredIntermediateSize(D3D12TextureOut->GetResource()->GetResource(), 0, NumMips);

			FD3D12ResourceLocation TempResourceLocation(GetRHIDevice());
			void* pData = GetRHIDevice()->GetDefaultFastAllocator().Allocate<FD3D12ScopeLock>(Size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &TempResourceLocation);

			FD3D12Texture3D* CurrentTexture = D3D12TextureOut;

			// Initialize each resource in the chain
			while (CurrentTexture != nullptr)
			{
				FD3D12Device* Device = CurrentTexture->GetParentDevice();
				FD3D12Resource* Resource = CurrentTexture->GetResource();

				FD3D12CommandListHandle& hCommandList = Device->GetDefaultCommandContext().CommandListHandle;
				hCommandList.GetCurrentOwningContext()->numCopies++;
				UpdateSubresources(
					hCommandList.GraphicsCommandList(),
					Resource->GetResource(),
					TempResourceLocation.GetResource()->GetResource(),
					TempResourceLocation.GetOffsetFromBaseOfResource(),
					0, NumMips,
					SubResourceData.GetData());
				hCommandList.UpdateResidency(Resource);

				hCommandList.AddTransitionBarrier(Resource, D3D12_RESOURCE_STATE_COPY_DEST, DestinationState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

				CurrentTexture = (FD3D12Texture3D*)CurrentTexture->GetNextObject();
			}
		}

		FD3D12TextureStats::D3D12TextureAllocated(*D3D12TextureOut);
	}

	if (CreateInfo.BulkData)
	{
		CreateInfo.BulkData->Discard();
	}

	return D3D12TextureOut;
#else
	checkf(false, TEXT("XBOX_CODE_MERGE : Removed. The Xbox platform version should be used."));
	return nullptr;
#endif // PLATFORM_WINDOWS
}

/*-----------------------------------------------------------------------------
	2D texture support.
	-----------------------------------------------------------------------------*/

FTexture2DRHIRef FD3D12DynamicRHI::RHICreateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	if (CreateInfo.BulkData != nullptr)
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);

		return GDynamicRHI->RHICreateTexture2D(SizeX, SizeY, Format, NumMips, NumSamples, Flags, CreateInfo);
	}

	return GDynamicRHI->RHICreateTexture2D(SizeX, SizeY, Format, NumMips, NumSamples, Flags, CreateInfo);
}

FTexture2DRHIRef FD3D12DynamicRHI::RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return CreateD3D12Texture2D<FD3D12BaseTexture2D>(SizeX, SizeY, 1, false, false, Format, NumMips, NumSamples, Flags, CreateInfo);
}

FTexture2DRHIRef FD3D12DynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, void** InitialMipData, uint32 NumInitialMips)
{
	check(GRHISupportsAsyncTextureCreation);
	
	const uint32 InvalidFlags = TexCreate_RenderTargetable | TexCreate_ResolveTargetable | TexCreate_DepthStencilTargetable | TexCreate_GenerateMipCapable | TexCreate_UAV | TexCreate_Presentable | TexCreate_CPUReadback;
	check((Flags & InvalidFlags) == 0);

	if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES2)
	{
		// Remove sRGB read flag when not supported
		Flags &= ~TexCreate_SRGB;
	}

	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, ((Flags & TexCreate_SRGB) != 0));
	const D3D12_RESOURCE_DESC TextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		PlatformResourceFormat,
		SizeX,
		SizeY,
		1,
		NumMips,
		1,  // Sample count
		0);  // Sample quality

	D3D12_SUBRESOURCE_DATA SubResourceData[MAX_TEXTURE_MIP_COUNT] = { 0 };
	for (uint32 MipIndex = 0; MipIndex < NumInitialMips; ++MipIndex)
	{
		uint32 NumBlocksX = FMath::Max<uint32>(1, (SizeX >> MipIndex) / GPixelFormats[Format].BlockSizeX);
		uint32 NumBlocksY = FMath::Max<uint32>(1, (SizeY >> MipIndex) / GPixelFormats[Format].BlockSizeY);

		SubResourceData[MipIndex].pData = InitialMipData[MipIndex];
		SubResourceData[MipIndex].RowPitch = NumBlocksX * GPixelFormats[Format].BlockBytes;
		SubResourceData[MipIndex].SlicePitch = NumBlocksX * NumBlocksY * GPixelFormats[Format].BlockBytes;
	}

	void* TempBuffer = ZeroBuffer;
	uint32 TempBufferSize = ZeroBufferSize;
	for (uint32 MipIndex = NumInitialMips; MipIndex < NumMips; ++MipIndex)
	{
		uint32 NumBlocksX = FMath::Max<uint32>(1, (SizeX >> MipIndex) / GPixelFormats[Format].BlockSizeX);
		uint32 NumBlocksY = FMath::Max<uint32>(1, (SizeY >> MipIndex) / GPixelFormats[Format].BlockSizeY);
		uint32 MipSize = NumBlocksX * NumBlocksY * GPixelFormats[Format].BlockBytes;

		if (MipSize > TempBufferSize)
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("Temp texture streaming buffer not large enough, needed %d bytes"), MipSize);
			check(TempBufferSize == ZeroBufferSize);
			TempBufferSize = MipSize;
			TempBuffer = FMemory::Malloc(TempBufferSize);
			FMemory::Memzero(TempBuffer, TempBufferSize);
		}

		SubResourceData[MipIndex].pData = TempBuffer;
		SubResourceData[MipIndex].RowPitch = NumBlocksX * GPixelFormats[Format].BlockBytes;
		SubResourceData[MipIndex].SlicePitch = MipSize;
	}

	// All resources used in a COPY command list must begin in the COMMON state. 
	// COPY_SOURCE and COPY_DEST are "promotable" states. You can create async texture resources in the COMMON state and still avoid any state transitions by relying on state promotion. 
	// Also remember that ALL touched resources in a COPY command list decay to COMMON after ExecuteCommandLists completes.
	const D3D12_RESOURCE_STATES InitialState = D3D12_RESOURCE_STATE_COMMON;

	FD3D12Adapter* Adapter = &GetAdapter();
	FD3D12Texture2D* TextureOut = Adapter->CreateLinkedObject<FD3D12Texture2D>([&](FD3D12Device* Device)
	{
		FD3D12Texture2D* NewTexture = new FD3D12Texture2D(Device,
			SizeX,
			SizeY,
			0,
			NumMips,
			/*ActualMSAACount=*/ 1,
			(EPixelFormat)Format,
			/*bInCubemap=*/ false,
			Flags,
			FClearValueBinding());

		SafeCreateTexture2D(Device,
			Adapter,
			TextureDesc,
			nullptr,
			&NewTexture->ResourceLocation,
			Format,
			Flags,
			InitialState);

		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.Format = PlatformShaderResourceFormat;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.MipLevels = NumMips;
		SRVDesc.Texture2D.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, SRVDesc.Format);

		// Create a wrapper for the SRV and set it on the texture
		FD3D12ShaderResourceView* SRV = FD3D12ShaderResourceView::CreateShaderResourceView(Device, &NewTexture->ResourceLocation, SRVDesc);
		NewTexture->SetShaderResourceView(SRV);

		return NewTexture;
	});

	if (TextureOut)
	{
		// SubResourceData is only used in async texture creation (RHIAsyncCreateTexture2D). We need to manually transition the resource to
		// its 'default state', which is what the rest of the RHI (including InitializeTexture2DData) expects for SRV-only resources.

		check((TextureDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0);

		const uint64 Size = GetRequiredIntermediateSize(TextureOut->GetResource()->GetResource(), 0, NumMips);
		FD3D12FastAllocator& FastAllocator = GetHelperThreadDynamicUploadHeapAllocator();
		FD3D12ResourceLocation TempResourceLocation(FastAllocator.GetParentDevice());
		FastAllocator.Allocate<FD3D12ScopeLock>(Size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &TempResourceLocation);

		FD3D12Texture2D* CurrentTexture = TextureOut;
		while (CurrentTexture != nullptr)
		{
			FD3D12Device* Device = CurrentTexture->GetParentDevice();
			FD3D12Resource* Resource = CurrentTexture->GetResource();

			FD3D12CommandAllocatorManager& CommandAllocatorManager = Device->GetTextureStreamingCommandAllocatorManager();
			FD3D12CommandAllocator* CurrentCommandAllocator = CommandAllocatorManager.ObtainCommandAllocator();
			FD3D12CommandListHandle hCopyCommandList = Device->GetCopyCommandListManager().ObtainCommandList(*CurrentCommandAllocator);
			hCopyCommandList.SetCurrentOwningContext(&Device->GetDefaultCommandContext());

			hCopyCommandList.GetCurrentOwningContext()->numCopies++;
			UpdateSubresources(
				(ID3D12GraphicsCommandList*)hCopyCommandList.CommandList(),
				Resource->GetResource(),
				TempResourceLocation.GetResource()->GetResource(),
				TempResourceLocation.GetOffsetFromBaseOfResource(),
				0, NumMips,
				SubResourceData);

			hCopyCommandList.UpdateResidency(Resource);

			// Wait for the copy context to finish before continuing as this function is only expected to return once all the texture streaming has finished.
			hCopyCommandList.Close();
			Device->GetCopyCommandListManager().ExecuteCommandList(hCopyCommandList, true);

			CommandAllocatorManager.ReleaseCommandAllocator(CurrentCommandAllocator);

			CurrentTexture = (FD3D12Texture2D*)CurrentTexture->GetNextObject();
		}

		FD3D12TextureStats::D3D12TextureAllocated(*TextureOut);
	}

	if (TempBufferSize != ZeroBufferSize)
	{
		FMemory::Free(TempBuffer);
	}


	return TextureOut;
}

void FD3D12DynamicRHI::RHICopySharedMips(FTexture2DRHIParamRef DestTexture2DRHI, FTexture2DRHIParamRef SrcTexture2DRHI)
{
	FD3D12Texture2D*  DestTexture2D = FD3D12DynamicRHI::ResourceCast(DestTexture2DRHI);
	FD3D12Texture2D*  SrcTexture2D = FD3D12DynamicRHI::ResourceCast(SrcTexture2DRHI);

	// Use the GPU to asynchronously copy the old mip-maps into the new texture.
	const uint32 NumSharedMips = FMath::Min(DestTexture2D->GetNumMips(), SrcTexture2D->GetNumMips());
	const uint32 SourceMipOffset = SrcTexture2D->GetNumMips() - NumSharedMips;
	const uint32 DestMipOffset = DestTexture2D->GetNumMips() - NumSharedMips;

	uint32 srcSubresource = 0;
	uint32 destSubresource = 0;

	FD3D12Adapter* Adapter = &GetAdapter();

	while(DestTexture2D && SrcTexture2D)
	{
		FD3D12Device* Device = DestTexture2D->GetParentDevice();

		FD3D12CommandListHandle& hCommandList = Device->GetDefaultCommandContext().CommandListHandle;

		{
			FScopeResourceBarrier ScopeResourceBarrierDest(hCommandList, DestTexture2D->GetResource(), DestTexture2D->GetResource()->GetDefaultResourceState(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			FScopeResourceBarrier ScopeResourceBarrierSrc(hCommandList, SrcTexture2D->GetResource(), SrcTexture2D->GetResource()->GetDefaultResourceState(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			hCommandList.FlushResourceBarriers();

			for (uint32 MipIndex = 0; MipIndex < NumSharedMips; ++MipIndex)
			{
				// Use the GPU to copy between mip-maps.
				srcSubresource = CalcSubresource(MipIndex + SourceMipOffset, 0, SrcTexture2D->GetNumMips());
				destSubresource = CalcSubresource(MipIndex + DestMipOffset, 0, DestTexture2D->GetNumMips());

				CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(DestTexture2D->GetResource()->GetResource(), destSubresource);
				CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(SrcTexture2D->GetResource()->GetResource(), srcSubresource);

				Device->GetDefaultCommandContext().numCopies++;
				hCommandList->CopyTextureRegion(
					&DestCopyLocation,
					0, 0, 0,
					&SourceCopyLocation,
					nullptr);

				hCommandList.UpdateResidency(DestTexture2D->GetResource());
				hCommandList.UpdateResidency(SrcTexture2D->GetResource());
			}
		}

		DEBUG_RHI_EXECUTE_COMMAND_LIST(this);

		DestTexture2D = (FD3D12Texture2D*) DestTexture2D->GetNextObject();
		SrcTexture2D = (FD3D12Texture2D*) SrcTexture2D->GetNextObject();
	}
}

FTexture2DArrayRHIRef FD3D12DynamicRHI::RHICreateTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	if (CreateInfo.BulkData != nullptr)
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);

		return RHICreateTexture2DArray(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
	}

	return RHICreateTexture2DArray(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
}

FTexture2DArrayRHIRef FD3D12DynamicRHI::RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	check(SizeZ >= 1);
	return CreateD3D12Texture2D<FD3D12BaseTexture2DArray>(SizeX, SizeY, SizeZ, true, false, Format, NumMips, 1, Flags, CreateInfo);
}

FTexture3DRHIRef FD3D12DynamicRHI::RHICreateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	if (CreateInfo.BulkData != nullptr)
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);

		return RHICreateTexture3D(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
	}

	return RHICreateTexture3D(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
}

FTexture3DRHIRef FD3D12DynamicRHI::RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	check(SizeZ >= 1);
#if PLATFORM_WINDOWS
	return CreateD3D12Texture3D(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
#else
	checkf(false, TEXT("XBOX_CODE_MERGE : Removed. The Xbox platform version should be used."));
	return nullptr;
#endif // PLATFORM_WINDOWS
}

void FD3D12DynamicRHI::RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
{
	if (Ref)
	{
		OutInfo = Ref->ResourceInfo;
	}
}

/** Generates mip maps for the surface. */
void FD3D12DynamicRHI::RHIGenerateMips(FTextureRHIParamRef TextureRHI)
{
	// MS: GenerateMips has been removed in D3D12.  However, this code path isn't executed in 
	// available UE content, so there is no need to re-implement GenerateMips for now.
	FD3D12TextureBase* Texture = GetD3D12TextureFromRHITexture(TextureRHI);
	// Surface must have been created with D3D11_BIND_RENDER_TARGET for GenerateMips to work
	check(Texture->GetShaderResourceView() && Texture->GetRenderTargetView(0, -1));
	GetRHIDevice()->RegisterGPUWork(0);
}

/**
 * Computes the size in memory required by a given texture.
 *
 * @param	TextureRHI		- Texture we want to know the size of
 * @return					- Size in Bytes
 */
uint32 FD3D12DynamicRHI::RHIComputeMemorySize(FTextureRHIParamRef TextureRHI)
{
	if (!TextureRHI)
	{
		return 0;
	}

	FD3D12TextureBase* Texture = GetD3D12TextureFromRHITexture(TextureRHI);
	return Texture->GetMemorySize();
}

/**
 * Starts an asynchronous texture reallocation. It may complete immediately if the reallocation
 * could be performed without any reshuffling of texture memory, or if there isn't enough memory.
 * The specified status counter will be decremented by 1 when the reallocation is complete (success or failure).
 *
 * Returns a new reference to the texture, which will represent the new mip count when the reallocation is complete.
 * RHIGetAsyncReallocateTexture2DStatus() can be used to check the status of an ongoing or completed reallocation.
 *
 * @param Texture2D		- Texture to reallocate
 * @param NewMipCount	- New number of mip-levels
 * @param NewSizeX		- New width, in pixels
 * @param NewSizeY		- New height, in pixels
 * @param RequestStatus	- Will be decremented by 1 when the reallocation is complete (success or failure).
 * @return				- New reference to the texture, or an invalid reference upon failure
 */
FTexture2DRHIRef FD3D12DynamicRHI::RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2DRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	FD3D12Texture2D*  Texture2D = FD3D12DynamicRHI::ResourceCast(Texture2DRHI);

	// Allocate a new texture.
	FRHIResourceCreateInfo CreateInfo;
	FD3D12Texture2D* NewTexture2D = CreateD3D12Texture2D<FD3D12BaseTexture2D>(NewSizeX, NewSizeY, 1, false, false, Texture2D->GetFormat(), NewMipCount, 1, Texture2D->GetFlags(), CreateInfo);
	FD3D12Texture2D* OriginalTexture = NewTexture2D;

	// Use the GPU to asynchronously copy the old mip-maps into the new texture.
	const uint32 NumSharedMips = FMath::Min(Texture2D->GetNumMips(), NewTexture2D->GetNumMips());
	const uint32 SourceMipOffset = Texture2D->GetNumMips() - NumSharedMips;
	const uint32 DestMipOffset = NewTexture2D->GetNumMips() - NumSharedMips;

	uint32 destSubresource = 0;
	uint32 srcSubresource = 0;

	while(Texture2D && NewTexture2D)
	{
		FD3D12Device* Device = Texture2D->GetParentDevice();

		FD3D12CommandListHandle& hCommandList = Device->GetDefaultCommandContext().CommandListHandle;

		FScopeResourceBarrier ScopeResourceBarrierDest(hCommandList, NewTexture2D->GetResource(), NewTexture2D->GetResource()->GetDefaultResourceState(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		FScopeResourceBarrier ScopeResourceBarrierSource(hCommandList, Texture2D->GetResource(), Texture2D->GetResource()->GetDefaultResourceState(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		hCommandList.FlushResourceBarriers();	// Must flush so the desired state is actually set.

		for (uint32 MipIndex = 0; MipIndex < NumSharedMips; ++MipIndex)
		{
			// Use the GPU to copy between mip-maps.
			// This is serialized with other D3D commands, so it isn't necessary to increment Counter to signal a pending asynchronous copy.

			srcSubresource = CalcSubresource(MipIndex + SourceMipOffset, 0, Texture2D->GetNumMips());
			destSubresource = CalcSubresource(MipIndex + DestMipOffset, 0, NewTexture2D->GetNumMips());

			CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(NewTexture2D->GetResource()->GetResource(), destSubresource);
			CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(Texture2D->GetResource()->GetResource(), srcSubresource);

			Device->GetDefaultCommandContext().numCopies++;
			hCommandList->CopyTextureRegion(
				&DestCopyLocation,
				0, 0, 0,
				&SourceCopyLocation,
				nullptr);

			hCommandList.UpdateResidency(NewTexture2D->GetResource());
			hCommandList.UpdateResidency(Texture2D->GetResource());

			DEBUG_RHI_EXECUTE_COMMAND_LIST(this);
		}

		Texture2D = (FD3D12Texture2D*)Texture2D->GetNextObject();
		NewTexture2D = (FD3D12Texture2D*)NewTexture2D->GetNextObject();
	}

	// Decrement the thread-safe counter used to track the completion of the reallocation, since D3D handles sequencing the
	// async mip copies with other D3D calls.
	RequestStatus->Decrement();

	return OriginalTexture;
}

/**
 * Returns the status of an ongoing or completed texture reallocation:
 *	TexRealloc_Succeeded	- The texture is ok, reallocation is not in progress.
 *	TexRealloc_Failed		- The texture is bad, reallocation is not in progress.
 *	TexRealloc_InProgress	- The texture is currently being reallocated async.
 *
 * @param Texture2D		- Texture to check the reallocation status for
 * @return				- Current reallocation status
 */
ETextureReallocationStatus FD3D12DynamicRHI::RHIFinalizeAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	return TexRealloc_Succeeded;
}

/**
 * Cancels an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to cancel
 * @param bBlockUntilCompleted	If true, blocks until the cancellation is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus FD3D12DynamicRHI::RHICancelAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	return TexRealloc_Succeeded;
}

template<typename RHIResourceType>
void* TD3D12Texture2D<RHIResourceType>::Lock(class FRHICommandListImmediate* RHICmdList, uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12LockTextureTime);

	FD3D12Device* Device = GetParentDevice();
	FD3D12Adapter* Adapter = Device->GetParentAdapter();

	// Calculate the subresource index corresponding to the specified mip-map.
	const uint32 Subresource = CalcSubresource(MipIndex, ArrayIndex, this->GetNumMips());

	check(LockedMap.Find(Subresource) == nullptr);
	FD3D12LockedResource* LockedResource = new FD3D12LockedResource(Device);

	// Calculate the dimensions of the mip-map.
	const uint32 BlockSizeX = GPixelFormats[this->GetFormat()].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[this->GetFormat()].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[this->GetFormat()].BlockBytes;
	const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> MipIndex, BlockSizeX);
	const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> MipIndex, BlockSizeY);
	const uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	const uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;

	const uint32 XBytesAligned = Align(NumBlocksX * BlockBytes, FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	const uint32 MipBytesAligned = XBytesAligned * NumBlocksY;

	FD3D12CommandListHandle& hCommandList = Device->GetDefaultCommandContext().CommandListHandle;

	void* Data = nullptr;

#if	PLATFORM_SUPPORTS_VIRTUAL_TEXTURES

	if (GetParentDevice()->GetOwningRHI()->HandleSpecialLock(Data, MipIndex, ArrayIndex, GetFlags(), LockMode, GetTextureLayout(), RawTextureMemory, DestStride))
	{
		// nothing left to do...
		check(Data != nullptr);
	}
	else
#endif
	if (LockMode == RLM_WriteOnly)
	{
		// If we're writing to the texture, allocate a system memory buffer to receive the new contents.
		// Use an upload heap to copy data to a default resource.
		//const uint32 bufferSize = (uint32)GetRequiredIntermediateSize(this->GetResource()->GetResource(), Subresource, 1);
		const uint32 bufferSize = Align(MipBytesAligned, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

		void* pData = Device->GetDefaultFastAllocator().Allocate<FD3D12ScopeLock>(bufferSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &LockedResource->ResourceLocation);
		if (nullptr == pData)
		{
			check(false);
			return nullptr;
		}

		DestStride = XBytesAligned;
		LockedResource->LockedPitch = XBytesAligned;

		check(LockedResource->LockedPitch % FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);

		Data = LockedResource->ResourceLocation.GetMappedBaseAddress();
	}
	else
	{
		LockedResource->bLockedForReadOnly = true;

		//TODO: Make this work for AFR (it's probably a very rare occurance though)
		check(Adapter->GetNumGPUNodes() == 1);

		// If we're reading from the texture, we create a staging resource, copy the texture contents to it, and map it.

		// Create the staging texture.
		const D3D12_RESOURCE_DESC& StagingTextureDesc = GetResource()->GetDesc();
		FD3D12Resource* StagingTexture = nullptr;

		const GPUNodeMask Node = Device->GetNodeMask();
		VERIFYD3D12RESULT(Adapter->CreateBuffer(D3D12_HEAP_TYPE_READBACK, Node, Node, MipBytesAligned, &StagingTexture));

		LockedResource->ResourceLocation.AsStandAlone(StagingTexture, MipBytesAligned);

		// Copy the mip-map data from the real resource into the staging resource
		D3D12_SUBRESOURCE_FOOTPRINT destSubresource;
		destSubresource.Depth = 1;
		destSubresource.Height = MipSizeY;
		destSubresource.Width = MipSizeX;
		destSubresource.Format = StagingTextureDesc.Format;
		destSubresource.RowPitch = XBytesAligned;
		check(destSubresource.RowPitch % FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedTexture2D = { 0 };
		placedTexture2D.Offset = 0;
		placedTexture2D.Footprint = destSubresource;

		CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(StagingTexture->GetResource(), placedTexture2D);
		CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(GetResource()->GetResource(), Subresource);

		const auto& pfnCopyTextureRegion = [&]()
		{
			FScopeResourceBarrier ScopeResourceBarrierSource(hCommandList, GetResource(), GetResource()->GetDefaultResourceState(), D3D12_RESOURCE_STATE_COPY_SOURCE, SourceCopyLocation.SubresourceIndex);

			Device->GetDefaultCommandContext().numCopies++;
			hCommandList.FlushResourceBarriers();
			hCommandList->CopyTextureRegion(
				&DestCopyLocation,
				0, 0, 0,
				&SourceCopyLocation,
				nullptr);

			hCommandList.UpdateResidency(GetResource());
		};

		if (RHICmdList != nullptr)
		{
			check(IsInRHIThread() == false);

			RHICmdList->ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			pfnCopyTextureRegion();
		}
		else
		{
			check(IsInRHIThread());

			pfnCopyTextureRegion();
		}

		// We need to execute the command list so we can read the data from the map below
		Device->GetDefaultCommandContext().FlushCommands(true);

		LockedResource->LockedPitch = XBytesAligned;
		DestStride = XBytesAligned;
		check(LockedResource->LockedPitch % FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);
		check(DestStride == XBytesAligned);

		Data = LockedResource->ResourceLocation.GetMappedBaseAddress();
	}

	LockedMap.Add(Subresource, LockedResource);

	check(Data != nullptr);
	return Data;
}

void FD3D12TextureBase::UpdateTexture(const D3D12_TEXTURE_COPY_LOCATION& DestCopyLocation, uint32 DestX, uint32 DestY, uint32 DestZ, const D3D12_TEXTURE_COPY_LOCATION& SourceCopyLocation)
{
	FD3D12CommandContext& DefaultContext = GetParentDevice()->GetDefaultCommandContext();
	FD3D12CommandListHandle& hCommandList = DefaultContext.CommandListHandle;

	FConditionalScopeResourceBarrier ScopeResourceBarrierDest(hCommandList, GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, DestCopyLocation.SubresourceIndex);
	// Don't need to transition upload heaps

	DefaultContext.numCopies++;
	hCommandList.FlushResourceBarriers();
	hCommandList->CopyTextureRegion(
		&DestCopyLocation,
		DestX, DestY, DestZ,
		&SourceCopyLocation,
		nullptr);

	hCommandList.UpdateResidency(GetResource());

	DEBUG_EXECUTE_COMMAND_CONTEXT(DefaultContext);
}

template<typename RHIResourceType>
void TD3D12Texture2D<RHIResourceType>::Unlock(class FRHICommandListImmediate* RHICmdList, uint32 MipIndex, uint32 ArrayIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12UnlockTextureTime);

	UnlockInternal(RHICmdList, nullptr, MipIndex, ArrayIndex);
}

template<typename RHIResourceType>
void TD3D12Texture2D<RHIResourceType>::UnlockInternal(class FRHICommandListImmediate* RHICmdList, TD3D12Texture2D<RHIResourceType>* Previous, uint32 MipIndex, uint32 ArrayIndex)
{
	// Calculate the subresource index corresponding to the specified mip-map.
	const uint32 Subresource = CalcSubresource(MipIndex, ArrayIndex, this->GetNumMips());

	// Calculate the dimensions of the mip-map.
	const uint32 BlockSizeX = GPixelFormats[this->GetFormat()].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[this->GetFormat()].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[this->GetFormat()].BlockBytes;
	const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> MipIndex, BlockSizeX);
	const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> MipIndex, BlockSizeY);

	TMap<uint32, FD3D12LockedResource*>& Map = (Previous) ? Previous->LockedMap : LockedMap;
	FD3D12LockedResource* LockedResource = Map[Subresource];

	check(LockedResource);

#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	if (GetParentDevice()->GetOwningRHI()->HandleSpecialUnlock(RHICmdList, MipIndex, GetFlags(), GetTextureLayout(), RawTextureMemory))
	{
		// nothing left to do...
	}
	else
#endif
	{
		if (!LockedResource->bLockedForReadOnly)
		{
			FD3D12Resource* Resource = GetResource();
			FD3D12ResourceLocation& UploadLocation = LockedResource->ResourceLocation;

			// Copy the mip-map data from the real resource into the staging resource
			const D3D12_RESOURCE_DESC& ResourceDesc = Resource->GetDesc();
			D3D12_SUBRESOURCE_FOOTPRINT BufferPitchDesc;
			BufferPitchDesc.Depth = 1;
			BufferPitchDesc.Height = MipSizeY;
			BufferPitchDesc.Width = MipSizeX;
			BufferPitchDesc.Format = ResourceDesc.Format;
			BufferPitchDesc.RowPitch = LockedResource->LockedPitch;
			check(BufferPitchDesc.RowPitch % FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT PlacedTexture2D = { 0 };
			PlacedTexture2D.Offset = UploadLocation.GetOffsetFromBaseOfResource();
			PlacedTexture2D.Footprint = BufferPitchDesc;

			CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(Resource->GetResource(), Subresource);

			FD3D12CommandListHandle& hCommandList = GetParentDevice()->GetDefaultCommandContext().CommandListHandle;

			// If we are on the render thread, queue up the copy on the RHIThread so it happens at the correct time.
			if (ShouldDeferCmdListOperation(RHICmdList))
			{
				new (RHICmdList->AllocCommand<FRHICommandUpdateTexture>()) FRHICommandUpdateTexture(this, DestCopyLocation, 0, 0, 0, UploadLocation, PlacedTexture2D);
			}
			else
			{
				CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(UploadLocation.GetResource()->GetResource(), PlacedTexture2D);

				UpdateTexture(DestCopyLocation, 0, 0, 0, SourceCopyLocation);
			}

			// Recurse to update all of the resources in the LDA chain
			if (GetNextObject())
			{
				// We pass the first link in the chain as that's the guy that got locked
				((TD3D12Texture2D<RHIResourceType>*)GetNextObject())->UnlockInternal(RHICmdList, (Previous) ? Previous : this, MipIndex, ArrayIndex);
			}
		}
	}

	// Remove the lock from the outstanding lock list.
	delete(LockedResource);
	Map.Remove(Subresource);
}

template<typename RHIResourceType>
void TD3D12Texture2D<RHIResourceType>::UpdateTexture2D(class FRHICommandListImmediate* RHICmdList, uint32 MipIndex, const FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	D3D12_BOX DestBox =
	{
		UpdateRegion.DestX, UpdateRegion.DestY, 0,
		UpdateRegion.DestX + UpdateRegion.Width, UpdateRegion.DestY + UpdateRegion.Height, 1
	};

	check(GPixelFormats[this->GetFormat()].BlockSizeX == 1);
	check(GPixelFormats[this->GetFormat()].BlockSizeY == 1);

	const uint32 AlignedSourcePitch = Align(SourcePitch, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
	const uint32 bufferSize = Align(UpdateRegion.Height*AlignedSourcePitch, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

	FD3D12Texture2D* Texture = this;
	while (Texture)
	{
		FD3D12ResourceLocation UploadHeapResourceLocation(GetParentDevice());
		void* pData = GetParentDevice()->GetDefaultFastAllocator().Allocate<FD3D12ScopeLock>(bufferSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &UploadHeapResourceLocation);
		check(nullptr != pData);

		byte* pRowData = (byte*)pData;
		byte* pSourceRowData = (byte*)SourceData;
		uint32 CopyPitch = UpdateRegion.Width * GPixelFormats[this->GetFormat()].BlockBytes;
		check(CopyPitch <= SourcePitch);
		for (uint32 i = 0; i < UpdateRegion.Height; i++)
		{
			FMemory::Memcpy(pRowData, pSourceRowData, CopyPitch);
			pSourceRowData += SourcePitch;
			pRowData += AlignedSourcePitch;
		}

		D3D12_SUBRESOURCE_FOOTPRINT SourceSubresource;
		SourceSubresource.Depth = 1;
		SourceSubresource.Height = UpdateRegion.Height;
		SourceSubresource.Width = UpdateRegion.Width;
		SourceSubresource.Format = (DXGI_FORMAT)GPixelFormats[this->GetFormat()].PlatformFormat;
		SourceSubresource.RowPitch = AlignedSourcePitch;
		check(SourceSubresource.RowPitch % FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT PlacedTexture2D = { 0 };
		PlacedTexture2D.Offset = UploadHeapResourceLocation.GetOffsetFromBaseOfResource();
		PlacedTexture2D.Footprint = SourceSubresource;

		CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(Texture->GetResource()->GetResource(), MipIndex);
		
		// If we are on the render thread, queue up the copy on the RHIThread so it happens at the correct time.
		if (ShouldDeferCmdListOperation(RHICmdList))
		{
			new (RHICmdList->AllocCommand<FRHICommandUpdateTexture>()) FRHICommandUpdateTexture(this, DestCopyLocation, UpdateRegion.DestX, UpdateRegion.DestY, 0, UploadHeapResourceLocation, PlacedTexture2D);
		}
		else
		{
			CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(UploadHeapResourceLocation.GetResource()->GetResource(), PlacedTexture2D);
			UpdateTexture(DestCopyLocation, UpdateRegion.DestX, UpdateRegion.DestY, 0, SourceCopyLocation);
		}

		Texture = (FD3D12Texture2D*)Texture->GetNextObject();
	}
}

void* FD3D12DynamicRHI::LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	if (bNeedsDefaultRHIFlush)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockTexture2D_Flush);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		return RHILockTexture2D(TextureRHI, MipIndex, LockMode, DestStride, bLockWithinMiptail);
	}

	check(TextureRHI);
	FD3D12Texture2D* Texture = FD3D12DynamicRHI::ResourceCast(TextureRHI);
	return Texture->Lock(&RHICmdList, MipIndex, 0, LockMode, DestStride);
}

void* FD3D12DynamicRHI::RHILockTexture2D(FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	check(TextureRHI);
	FD3D12Texture2D*  Texture = FD3D12DynamicRHI::ResourceCast(TextureRHI);
	return Texture->Lock(nullptr, MipIndex, 0, LockMode, DestStride);
}

void FD3D12DynamicRHI::UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	if (bNeedsDefaultRHIFlush)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockTexture2D_Flush);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		RHIUnlockTexture2D(TextureRHI, MipIndex, bLockWithinMiptail);
		return;
	}

	check(TextureRHI);
	FD3D12Texture2D* Texture = FD3D12DynamicRHI::ResourceCast(TextureRHI);
	Texture->Unlock(&RHICmdList, MipIndex, 0);
}

void FD3D12DynamicRHI::RHIUnlockTexture2D(FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, bool bLockWithinMiptail)
{
	check(TextureRHI);
	FD3D12Texture2D*  Texture = FD3D12DynamicRHI::ResourceCast(TextureRHI);
	Texture->Unlock(nullptr, MipIndex, 0);
}

void* FD3D12DynamicRHI::RHILockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	check(TextureRHI);
	FD3D12Texture2DArray*  Texture = FD3D12DynamicRHI::ResourceCast(TextureRHI);
	return Texture->Lock(nullptr, MipIndex, TextureIndex, LockMode, DestStride);
}

void FD3D12DynamicRHI::RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	check(TextureRHI);
	FD3D12Texture2DArray*  Texture = FD3D12DynamicRHI::ResourceCast(TextureRHI);
	Texture->Unlock(nullptr, MipIndex, TextureIndex);
}

void FD3D12DynamicRHI::UpdateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	check(TextureRHI);
	FD3D12Texture2D* Texture = FD3D12DynamicRHI::ResourceCast(TextureRHI);
	Texture->UpdateTexture2D(&RHICmdList, MipIndex, UpdateRegion, SourcePitch, SourceData);
}

void FD3D12DynamicRHI::RHIUpdateTexture2D(FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, const FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	check(TextureRHI);
	FD3D12Texture2D* Texture = FD3D12DynamicRHI::ResourceCast(TextureRHI);
	Texture->UpdateTexture2D(nullptr, MipIndex, UpdateRegion, SourcePitch, SourceData);
}

FUpdateTexture3DData FD3D12DynamicRHI::BeginUpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion)
{
	check(IsInRenderingThread());
	// This stall could potentially be removed, provided the fast allocator is thread-safe. However we 
	// currently need to stall in the End method anyway (see below)
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	return BeginUpdateTexture3D_Internal(Texture, MipIndex, UpdateRegion);
}

void FD3D12DynamicRHI::EndUpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FUpdateTexture3DData& UpdateData)
{
	check(IsInRenderingThread());
	// TODO: move this command entirely to the RHI thread so we can remove these stalls
	// and fix potential ordering issue with non-compute-shader version
	FScopedRHIThreadStaller StallRHIThread(RHICmdList);
	EndUpdateTexture3D_Internal(UpdateData);
}

void FD3D12DynamicRHI::RHIUpdateTexture3D(FTexture3DRHIParamRef TextureRHI, uint32 MipIndex, const FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	check(IsInRenderingThread());

	FUpdateTexture3DData UpdateData = BeginUpdateTexture3D_Internal(TextureRHI, MipIndex, UpdateRegion);

	// Copy the data into the UpdateData destination buffer
	check(nullptr != UpdateData.Data);

	FD3D12Texture3D*  Texture = FD3D12DynamicRHI::ResourceCast(TextureRHI);
	uint32 CopyPitch = UpdateRegion.Width * GPixelFormats[Texture->GetFormat()].BlockBytes;
	check(CopyPitch <= SourceRowPitch);
	check(UpdateData.RowPitch*UpdateRegion.Depth*UpdateRegion.Height <= UpdateData.DataSizeBytes);

	for (uint32 i = 0; i < UpdateRegion.Depth; i++)
	{
		uint8* DestRowData = UpdateData.Data + UpdateData.DepthPitch * i;
		const uint8* SourceRowData = SourceData + SourceDepthPitch * i;
		for (uint32 j = 0; j < UpdateRegion.Height; j++)
		{
			FMemory::Memcpy(DestRowData, SourceRowData, CopyPitch);
			SourceRowData += SourceRowPitch;
			DestRowData += UpdateData.RowPitch;
		}
	}

	EndUpdateTexture3D_Internal(UpdateData);
}


FUpdateTexture3DData FD3D12DynamicRHI::BeginUpdateTexture3D_Internal(FTexture3DRHIParamRef TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion)
{
	check(IsInRenderingThread());
	FUpdateTexture3DData UpdateData(TextureRHI, MipIndex, UpdateRegion, 0, 0, nullptr, 0, GFrameNumberRenderThread);

	// Initialize the platform data
	static_assert(sizeof(FD3D12UpdateTexture3DData) < sizeof(UpdateData.PlatformData), "Platform data in FUpdateTexture3DData too small to support D3D12");
	FD3D12UpdateTexture3DData* UpdateDataD3D12 = new (&UpdateData.PlatformData[0]) FD3D12UpdateTexture3DData;
	UpdateDataD3D12->bComputeShaderCopy = false;
	UpdateDataD3D12->UploadHeapResourceLocation = nullptr;

	FD3D12Texture3D* Texture = FD3D12DynamicRHI::ResourceCast(TextureRHI);

	bool bDoComputeShaderCopy = false;
	if (CVarUseUpdateTexture3DComputeShader.GetValueOnRenderThread() != 0 && Texture->GetResource()->GetHeap())
	{
		// Try a compute shader update. This does a memory allocation internally
		bDoComputeShaderCopy = BeginUpdateTexture3D_ComputeShader(UpdateData, UpdateDataD3D12);
	}

	if (!bDoComputeShaderCopy)
	{
		const int32 FormatSize = GPixelFormats[TextureRHI->GetFormat()].BlockBytes;
		const int32 OriginalRowPitch = UpdateRegion.Width * FormatSize;
		const int32 OriginalDepthPitch = UpdateRegion.Width * UpdateRegion.Height * FormatSize;

		// No compute shader update was possible or supported, so fall back to the old method.
		D3D12_BOX DestBox =
		{
			UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.DestZ,
			UpdateRegion.DestX + UpdateRegion.Width, UpdateRegion.DestY + UpdateRegion.Height, UpdateRegion.DestZ + UpdateRegion.Depth
		};

		check(GPixelFormats[Texture->GetFormat()].BlockSizeX == 1);
		check(GPixelFormats[Texture->GetFormat()].BlockSizeY == 1);

		UpdateData.RowPitch = Align(OriginalRowPitch, FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
		UpdateData.DepthPitch = UpdateData.RowPitch * UpdateRegion.Height;
		const uint32 BufferSize = Align(UpdateRegion.Height*UpdateRegion.Depth*UpdateData.RowPitch, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
		UpdateData.DataSizeBytes = BufferSize;

		UpdateDataD3D12->UploadHeapResourceLocation = new FD3D12ResourceLocation(GetRHIDevice());
		UpdateData.Data = (uint8*)GetRHIDevice()->GetDefaultFastAllocator().Allocate<FD3D12ScopeLock>(BufferSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, UpdateDataD3D12->UploadHeapResourceLocation);
		check(UpdateData.Data != nullptr);
	}
	return UpdateData;
}

void FD3D12DynamicRHI::EndUpdateTexture3D_Internal(FUpdateTexture3DData& UpdateData)
{
	check(IsInRenderingThread());
	check(GFrameNumberRenderThread == UpdateData.FrameNumber);

	FD3D12Texture3D*  Texture = FD3D12DynamicRHI::ResourceCast(UpdateData.Texture);

	FD3D12Device* Device = Texture->GetParentDevice();
	FD3D12CommandListHandle& hCommandList = Device->GetDefaultCommandContext().CommandListHandle;
#if USE_PIX
	PIXBeginEvent(hCommandList.GraphicsCommandList(), PIX_COLOR(255, 255, 255), TEXT("EndUpdateTexture3D"));
#endif

	FD3D12UpdateTexture3DData* UpdateDataD3D12 = reinterpret_cast<FD3D12UpdateTexture3DData*>(&UpdateData.PlatformData[0]);
	check( UpdateDataD3D12->UploadHeapResourceLocation != nullptr );

	if (UpdateDataD3D12->bComputeShaderCopy)
	{
		EndUpdateTexture3D_ComputeShader(UpdateData, UpdateDataD3D12);
	}
	else
	{
		D3D12_SUBRESOURCE_FOOTPRINT sourceSubresource;
		sourceSubresource.Depth = UpdateData.UpdateRegion.Depth;
		sourceSubresource.Height = UpdateData.UpdateRegion.Height;
		sourceSubresource.Width = UpdateData.UpdateRegion.Width;
		sourceSubresource.Format = (DXGI_FORMAT)GPixelFormats[Texture->GetFormat()].PlatformFormat;
		sourceSubresource.RowPitch = UpdateData.RowPitch;
		check(sourceSubresource.RowPitch % FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedTexture3D = { 0 };
		placedTexture3D.Offset = UpdateDataD3D12->UploadHeapResourceLocation->GetOffsetFromBaseOfResource();
		placedTexture3D.Footprint = sourceSubresource;

		FD3D12Resource* UploadBuffer = UpdateDataD3D12->UploadHeapResourceLocation->GetResource();

		while (Texture)
		{
			CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(Texture->GetResource()->GetResource(), UpdateData.MipIndex);
			CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(UploadBuffer->GetResource(), placedTexture3D);

			FScopeResourceBarrier ScopeResourceBarrierDest(hCommandList, Texture->GetResource(), Texture->GetResource()->GetDefaultResourceState(), D3D12_RESOURCE_STATE_COPY_DEST, DestCopyLocation.SubresourceIndex);

			Device->GetDefaultCommandContext().numCopies++;
			hCommandList.FlushResourceBarriers();
			hCommandList->CopyTextureRegion(
				&DestCopyLocation,
				UpdateData.UpdateRegion.DestX, UpdateData.UpdateRegion.DestY, UpdateData.UpdateRegion.DestZ,
				&SourceCopyLocation,
				nullptr);

			hCommandList.UpdateResidency(Texture->GetResource());

			DEBUG_RHI_EXECUTE_COMMAND_LIST(this);

			Texture = (FD3D12Texture3D*)Texture->GetNextObject();
		}
		delete UpdateDataD3D12->UploadHeapResourceLocation;
	}
#if USE_PIX
	PIXEndEvent(hCommandList.GraphicsCommandList());
#endif
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
	-----------------------------------------------------------------------------*/
FTextureCubeRHIRef FD3D12DynamicRHI::RHICreateTextureCube_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	if (CreateInfo.BulkData != nullptr)
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);

		return RHICreateTextureCube(Size, Format, NumMips, Flags, CreateInfo);
	}

	return RHICreateTextureCube(Size, Format, NumMips, Flags, CreateInfo);
}

FTextureCubeRHIRef FD3D12DynamicRHI::RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return CreateD3D12Texture2D<FD3D12BaseTextureCube>(Size, Size, 6, false, true, Format, NumMips, 1, Flags, CreateInfo);
}

FTextureCubeRHIRef FD3D12DynamicRHI::RHICreateTextureCubeArray_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	if (CreateInfo.BulkData != nullptr)
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);

		return RHICreateTextureCubeArray(Size, ArraySize, Format, NumMips, Flags, CreateInfo);
	}

	return RHICreateTextureCubeArray(Size, ArraySize, Format, NumMips, Flags, CreateInfo);
}

FTextureCubeRHIRef FD3D12DynamicRHI::RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return CreateD3D12Texture2D<FD3D12BaseTextureCube>(Size, Size, 6 * ArraySize, true, true, Format, NumMips, 1, Flags, CreateInfo);
}

void* FD3D12DynamicRHI::RHILockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	FD3D12TextureCube*  TextureCube = FD3D12DynamicRHI::ResourceCast(TextureCubeRHI);
	GetRHIDevice()->GetDefaultCommandContext().ConditionalClearShaderResource(&TextureCube->ResourceLocation);
	uint32 D3DFace = GetD3D12CubeFace((ECubeFace)FaceIndex);
	return TextureCube->Lock(nullptr, MipIndex, D3DFace + ArrayIndex * 6, LockMode, DestStride);
}
void FD3D12DynamicRHI::RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	FD3D12TextureCube*  TextureCube = FD3D12DynamicRHI::ResourceCast(TextureCubeRHI);
	uint32 D3DFace = GetD3D12CubeFace((ECubeFace)FaceIndex);
	TextureCube->Unlock(nullptr, MipIndex, D3DFace + ArrayIndex * 6);
}

void FD3D12DynamicRHI::RHIBindDebugLabelName(FTextureRHIParamRef TextureRHI, const TCHAR* Name)
{
#if NAME_OBJECTS
	FName DebugName(Name);
	TextureRHI->SetName(DebugName);

	FD3D12Resource* Resource = GetD3D12TextureFromRHITexture(TextureRHI)->GetResource();
	SetName(Resource, Name);
#endif
}

void FD3D12DynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
}

void FD3D12DynamicRHI::RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
}

FTextureReferenceRHIRef FD3D12DynamicRHI::RHICreateTextureReference(FLastRenderTimeContainer* LastRenderTime)
{
	return new FD3D12TextureReference(GetRHIDevice(), LastRenderTime);
}

void FD3D12CommandContext::RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRefRHI, FTextureRHIParamRef NewTextureRHI)
{
#if 0
	// Updating texture references is disallowed while the RHI could be caching them in referenced resource tables.
	check(ResourceTableFrameCounter == INDEX_NONE);
#endif

	FD3D12TextureReference* TextureRef = (FD3D12TextureReference*)TextureRefRHI;
	if (TextureRef)
	{
		FD3D12TextureBase* NewTexture = NULL;
		FD3D12ShaderResourceView* NewSRV = NULL;
		if (NewTextureRHI)
		{
			NewTexture = GetD3D12TextureFromRHITexture(NewTextureRHI);
			if (NewTexture)
			{
				NewSRV = NewTexture->GetShaderResourceView();
			}
		}
		TextureRef->SetReferencedTexture(NewTextureRHI, NewTexture, NewSRV);
	}
}

ID3D12CommandQueue* FD3D12DynamicRHI::RHIGetD3DCommandQueue()
{
	return GetAdapter().GetDevice()->GetCommandListManager().GetD3DCommandQueue();
}

FTexture2DRHIRef FD3D12DynamicRHI::RHICreateTexture2DFromResource(EPixelFormat Format, uint32 TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D12Resource* Resource)
{
	check(Resource);
	FD3D12Adapter* Adapter = &GetAdapter();

	D3D12_RESOURCE_DESC TextureDesc = Resource->GetDesc();
	TextureDesc.Alignment = 0;

	uint32 SizeX = TextureDesc.Width;
	uint32 SizeY = TextureDesc.Height;
	uint32 SizeZ = 1;
	uint32 ArraySize = TextureDesc.DepthOrArraySize;
	uint32 NumMips = TextureDesc.MipLevels;
	uint32 NumSamples = TextureDesc.SampleDesc.Count;

	check(TextureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);
	check(ArraySize == 1);

	//TODO: Somehow Oculus is creating a Render Target with 4k alignment with ovr_GetTextureSwapChainBufferDX
	//      This is invalid and causes our size calculation to fail. Oculus SDK bug?
	if (TextureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		TextureDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	}

	SCOPE_CYCLE_COUNTER(STAT_D3D12CreateTextureTime);

	if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES2)
	{
		// Remove sRGB read flag when not supported
		TexCreateFlags &= ~TexCreate_SRGB;
	}

	const bool bSRGB = (TexCreateFlags & TexCreate_SRGB) != 0;

	const DXGI_FORMAT PlatformResourceFormat = TextureDesc.Format;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);

	// Set up the texture bind flags.
	bool bCreateRTV = (TextureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0;
	bool bCreateDSV = (TextureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0;

	const D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_COMMON;

	FD3D12Device* Device = Adapter->GetDevice();
	FD3D12Resource* TextureResource = new FD3D12Resource(Device, Device->GetNodeMask(), Resource, State, TextureDesc);
	TextureResource->AddRef();

	FD3D12Texture2D* Texture2D = new FD3D12Texture2D(Device, SizeX, SizeY, SizeZ, NumMips, NumSamples, Format, false, TexCreateFlags, ClearValueBinding);

	FD3D12ResourceLocation& Location = Texture2D->ResourceLocation;
	Location.AsStandAlone(TextureResource);
	Location.SetType(FD3D12ResourceLocation::ResourceLocationType::eAliased);
	TextureResource->AddRef();

	if (bCreateRTV)
	{
		Texture2D->SetCreatedRTVsPerSlice(false, NumMips);
		Texture2D->SetNumRenderTargetViews(NumMips);

		// Create a render target view for each array index and mip index
		for (uint32 ArrayIndex = 0; ArrayIndex < ArraySize; ArrayIndex++)
		{
			for (uint32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
			{
				D3D12_RENDER_TARGET_VIEW_DESC RTVDesc;
				FMemory::Memzero(&RTVDesc, sizeof(RTVDesc));
				RTVDesc.Format = PlatformRenderTargetFormat;

				if (NumSamples == 1)
				{
					RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
					RTVDesc.Texture2D.MipSlice = MipIndex;
					RTVDesc.Texture2D.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, RTVDesc.Format);
				}
				else
				{
					RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
				}

				Texture2D->SetRenderTargetViewIndex(FD3D12RenderTargetView::CreateRenderTargetView(Device, &Location, RTVDesc), ArrayIndex * NumMips + MipIndex);
			}
		}
	}

	if (bCreateDSV)
	{
		// Create a depth-stencil-view for the texture.
		D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc;
		FMemory::Memzero(&DSVDesc, sizeof(DSVDesc));
		DSVDesc.Format = FindDepthStencilDXGIFormat(PlatformResourceFormat);

		if (NumSamples == 1)
		{
			DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			DSVDesc.Texture2D.MipSlice = 0;
		}
		else
		{
			DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		}

		const bool HasStencil = HasStencilBits(DSVDesc.Format);
		for (uint32 AccessType = 0; AccessType < FExclusiveDepthStencil::MaxIndex; ++AccessType)
		{
			// Create a read-only access views for the texture.
			DSVDesc.Flags = D3D12_DSV_FLAG_NONE;

			if (AccessType & FExclusiveDepthStencil::DepthRead_StencilWrite)
			{
				DSVDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
			}

			if ((AccessType & FExclusiveDepthStencil::DepthWrite_StencilRead) && HasStencil)
			{
				DSVDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
			}

			Texture2D->SetDepthStencilView(FD3D12DepthStencilView::CreateDepthStencilView(Device, &Location, DSVDesc, HasStencil), AccessType);
		}
	}

	if (TexCreateFlags & TexCreate_CPUReadback)
	{
		const uint32 BlockBytes = GPixelFormats[Format].BlockBytes;
		const uint32 XBytesAligned = Align((uint32)SizeX * BlockBytes, FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
		D3D12_SUBRESOURCE_FOOTPRINT DestSubresource;
		DestSubresource.Depth = SizeZ;
		DestSubresource.Height = SizeY;
		DestSubresource.Width = SizeX;
		DestSubresource.Format = PlatformResourceFormat;
		DestSubresource.RowPitch = XBytesAligned;

		check(DestSubresource.RowPitch % FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);	// Make sure we align correctly.

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT PlacedTexture2D = { 0 };
		PlacedTexture2D.Offset = 0;
		PlacedTexture2D.Footprint = DestSubresource;

		Texture2D->SetReadBackHeapDesc(PlacedTexture2D);
	}

	// Create a shader resource view for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Format = PlatformShaderResourceFormat;

	if (NumSamples == 1)
	{
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.MipLevels = NumMips;
		SRVDesc.Texture2D.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, SRVDesc.Format);
	}
	else
	{
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
	}

	// Create a wrapper for the SRV and set it on the texture
	FD3D12ShaderResourceView* SRV = FD3D12ShaderResourceView::CreateShaderResourceView(Device, &Location, SRVDesc);
	Texture2D->SetShaderResourceView(SRV);

	FD3D12TextureStats::D3D12TextureAllocated(*Texture2D);

	return Texture2D;
}

FTextureCubeRHIRef FD3D12DynamicRHI::RHICreateTextureCubeFromResource(EPixelFormat Format, uint32 TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D12Resource* Resource)
{
	check(Resource);
	FD3D12Adapter* Adapter = &GetAdapter();

	D3D12_RESOURCE_DESC TextureDesc = Resource->GetDesc();
	TextureDesc.Alignment = 0;

	uint32 SizeX = TextureDesc.Width;
	uint32 SizeY = TextureDesc.Height;
	uint32 SizeZ = 1;
	uint32 ArraySize = TextureDesc.DepthOrArraySize;
	uint32 NumMips = TextureDesc.MipLevels;
	uint32 NumSamples = TextureDesc.SampleDesc.Count;

	check(TextureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);
	check(ArraySize == 6);

	//TODO: Somehow Oculus is creating a Render Target with 4k alignment with ovr_GetTextureSwapChainBufferDX
	//      This is invalid and causes our size calculation to fail. Oculus SDK bug?
	if (TextureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		TextureDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	}

	SCOPE_CYCLE_COUNTER(STAT_D3D12CreateTextureTime);

	if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES2)
	{
		// Remove sRGB read flag when not supported
		TexCreateFlags &= ~TexCreate_SRGB;
	}

	const bool bSRGB = (TexCreateFlags & TexCreate_SRGB) != 0;

	const DXGI_FORMAT PlatformResourceFormat = TextureDesc.Format;
	const DXGI_FORMAT PlatformShaderResourceFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = FindShaderResourceDXGIFormat(PlatformResourceFormat, bSRGB);

	// Set up the texture bind flags.
	bool bCreateRTV = (TextureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0;
	bool bCreateDSV = (TextureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0;

	const D3D12_RESOURCE_STATES State = D3D12_RESOURCE_STATE_COMMON;

	FD3D12Device* Device = Adapter->GetDevice();
	FD3D12Resource* TextureResource = new FD3D12Resource(Device, Device->GetNodeMask(), Resource, State, TextureDesc);
	TextureResource->AddRef();

	FD3D12TextureCube* TextureCube = new FD3D12TextureCube(Device, SizeX, SizeY, SizeZ, NumMips, NumSamples, Format, true, TexCreateFlags, ClearValueBinding);

	FD3D12ResourceLocation& Location = TextureCube->ResourceLocation;
	Location.AsStandAlone(TextureResource);
	Location.SetType(FD3D12ResourceLocation::ResourceLocationType::eAliased);
	TextureResource->AddRef();

	if (bCreateRTV)
	{
		TextureCube->SetCreatedRTVsPerSlice(false, NumMips);
		TextureCube->SetNumRenderTargetViews(NumMips);

		// Create a render target view for each array index and mip index
		for (uint32 ArrayIndex = 0; ArrayIndex < ArraySize; ArrayIndex++)
		{
			for (uint32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
			{
				D3D12_RENDER_TARGET_VIEW_DESC RTVDesc;
				FMemory::Memzero(&RTVDesc, sizeof(RTVDesc));
				RTVDesc.Format = PlatformRenderTargetFormat;

				if (NumSamples == 1)
				{
					RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
					RTVDesc.Texture2D.MipSlice = MipIndex;
					RTVDesc.Texture2D.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, RTVDesc.Format);
				}
				else
				{
					RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
				}

				TextureCube->SetRenderTargetViewIndex(FD3D12RenderTargetView::CreateRenderTargetView(Device, &Location, RTVDesc), ArrayIndex * NumMips + MipIndex);
			}
		}
	}

	if (bCreateDSV)
	{
		// Create a depth-stencil-view for the texture.
		D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc;
		FMemory::Memzero(&DSVDesc, sizeof(DSVDesc));
		DSVDesc.Format = FindDepthStencilDXGIFormat(PlatformResourceFormat);

		if (NumSamples == 1)
		{
			DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			DSVDesc.Texture2D.MipSlice = 0;
		}
		else
		{
			DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		}

		const bool HasStencil = HasStencilBits(DSVDesc.Format);
		for (uint32 AccessType = 0; AccessType < FExclusiveDepthStencil::MaxIndex; ++AccessType)
		{
			// Create a read-only access views for the texture.
			DSVDesc.Flags = D3D12_DSV_FLAG_NONE;

			if (AccessType & FExclusiveDepthStencil::DepthRead_StencilWrite)
			{
				DSVDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
			}

			if ((AccessType & FExclusiveDepthStencil::DepthWrite_StencilRead) && HasStencil)
			{
				DSVDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
			}

			TextureCube->SetDepthStencilView(FD3D12DepthStencilView::CreateDepthStencilView(Device, &Location, DSVDesc, HasStencil), AccessType);
		}
	}

	if (TexCreateFlags & TexCreate_CPUReadback)
	{
		const uint32 BlockBytes = GPixelFormats[Format].BlockBytes;
		const uint32 XBytesAligned = Align((uint32)SizeX * BlockBytes, FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
		D3D12_SUBRESOURCE_FOOTPRINT DestSubresource;
		DestSubresource.Depth = SizeZ;
		DestSubresource.Height = SizeY;
		DestSubresource.Width = SizeX;
		DestSubresource.Format = PlatformResourceFormat;
		DestSubresource.RowPitch = XBytesAligned;

		check(DestSubresource.RowPitch % FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);	// Make sure we align correctly.

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT PlacedTexture2D = { 0 };
		PlacedTexture2D.Offset = 0;
		PlacedTexture2D.Footprint = DestSubresource;

		TextureCube->SetReadBackHeapDesc(PlacedTexture2D);
	}

	// Create a shader resource view for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Format = PlatformShaderResourceFormat;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MostDetailedMip = 0;
	SRVDesc.TextureCube.MipLevels = NumMips;
	SRVDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	// Create a wrapper for the SRV and set it on the texture
	FD3D12ShaderResourceView* SRV = FD3D12ShaderResourceView::CreateShaderResourceView(Device, &Location, SRVDesc);
	TextureCube->SetShaderResourceView(SRV);

	FD3D12TextureStats::D3D12TextureAllocated(*TextureCube);

	return TextureCube;
}

void FD3D12DynamicRHI::RHIAliasTextureResources(FTextureRHIParamRef DestTextureRHI, FTextureRHIParamRef SrcTextureRHI)
{	
	FD3D12TextureBase* DestTexture = GetD3D12TextureFromRHITexture(DestTextureRHI);
	FD3D12TextureBase* SrcTexture = GetD3D12TextureFromRHITexture(SrcTextureRHI);

	while (DestTexture && SrcTexture)
	{
		DestTexture->AliasResources(SrcTexture);

		DestTexture = DestTexture->GetNextObject();
		SrcTexture = SrcTexture->GetNextObject();
	}
}
