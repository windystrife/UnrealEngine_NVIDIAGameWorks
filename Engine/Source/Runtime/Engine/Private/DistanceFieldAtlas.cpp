// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldAtlas.cpp
=============================================================================*/

#include "DistanceFieldAtlas.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Misc/App.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Modules/ModuleManager.h"
#include "StaticMeshResources.h"
#include "ProfilingDebugging/CookStats.h"
#include "UniquePtr.h"
#include "Engine/StaticMesh.h"
#include "AutomationTest.h"

#if WITH_EDITOR
#include "DerivedDataCacheInterface.h"
#include "MeshUtilities.h"
#endif

#if ENABLE_COOK_STATS
namespace DistanceFieldCookStats
{
	FCookStats::FDDCResourceUsageStats UsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("DistanceField.Usage"), TEXT(""));
	});
}
#endif

static TAutoConsoleVariable<int32> CVarDistField(
	TEXT("r.GenerateMeshDistanceFields"),
	0,	
	TEXT("Whether to build distance fields of static meshes, needed for distance field AO, which is used to implement Movable SkyLight shadows.\n")
	TEXT("Enabling will increase mesh build times and memory usage.  Changing this value will cause a rebuild of all static meshes."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarCompressDistField(
	TEXT("r.DistanceFieldBuild.Compress"),
	0,	
	TEXT("Whether to store mesh distance fields compressed in memory, which reduces how much memory they take, but also causes serious hitches when making new levels visible.  Only enable if your project does not stream levels in-game.\n")
	TEXT("Changing this regenerates all mesh distance fields."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarEightBitDistField(
	TEXT("r.DistanceFieldBuild.EightBit"),
	0,	
	TEXT("Whether to store mesh distance fields in an 8 bit fixed point format instead of 16 bit floating point.  \n")
	TEXT("8 bit uses half the memory, but introduces artifacts for large meshes or thin meshes."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarUseEmbreeForMeshDistanceFieldGeneration(
	TEXT("r.DistanceFieldBuild.UseEmbree"),
	1,
	TEXT("Whether to use embree ray tracer for mesh distance field generation."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarDistFieldRes(
	TEXT("r.DistanceFields.MaxPerMeshResolution"),
	128,	
	TEXT("Highest resolution (in one dimension) allowed for a single static mesh asset, used to cap the memory usage of meshes with a large scale.\n")
	TEXT("Changing this will cause all distance fields to be rebuilt.  Large values such as 512 can consume memory very quickly! (128Mb for one asset at 512)"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<float> CVarDistFieldResScale(
	TEXT("r.DistanceFields.DefaultVoxelDensity"),
	.1f,	
	TEXT("Determines how the default scale of a mesh converts into distance field voxel dimensions.\n")
	TEXT("Changing this will cause all distance fields to be rebuilt.  Large values can consume memory very quickly!"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarDistFieldAtlasResXY(
	TEXT("r.DistanceFields.AtlasSizeXY"),
	512,	
	TEXT("Max size of the global mesh distance field atlas volume texture in X and Y."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarDistFieldAtlasResZ(
	TEXT("r.DistanceFields.AtlasSizeZ"),
	1024,	
	TEXT("Max size of the global mesh distance field atlas volume texture in Z."),
	ECVF_ReadOnly);

int32 GDistanceFieldForceAtlasRealloc = 0;
FAutoConsoleVariableRef CVarDistFieldForceAtlasRealloc(
	TEXT("r.DistanceFields.ForceAtlasRealloc"),
	GDistanceFieldForceAtlasRealloc,
	TEXT("Force a full realloc."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLandscapeGI(
	TEXT("r.GenerateLandscapeGIData"),
	0,
	TEXT("Whether to generate a low-resolution base color texture for landscapes for rendering real-time global illumination.\n")
	TEXT("This feature requires GenerateMeshDistanceFields is also enabled, and will increase mesh build times and memory usage.\n"),
	ECVF_Default);

TGlobalResource<FDistanceFieldVolumeTextureAtlas> GDistanceFieldVolumeTextureAtlas = TGlobalResource<FDistanceFieldVolumeTextureAtlas>();

FDistanceFieldVolumeTextureAtlas::FDistanceFieldVolumeTextureAtlas() :
	BlockAllocator(0, 0, 0, 0, 0, 0, false, false),
	bInitialized(false)
{
	// Warning: can't access cvars here, this is called during global init
	Generation = 0;
}

void FDistanceFieldVolumeTextureAtlas::InitializeIfNeeded()
{
	if (!bInitialized)
	{
		bInitialized = true;

		static const auto CVarEightBit = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFieldBuild.EightBit"));
		const bool bEightBitFixedPoint = CVarEightBit->GetValueOnAnyThread() != 0;

		Format = bEightBitFixedPoint ? PF_G8 : PF_R16F;

		static const auto CVarXY = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFields.AtlasSizeXY"));
		const int32 AtlasXY = CVarXY->GetValueOnAnyThread();

		static const auto CVarZ = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFields.AtlasSizeZ"));
		const int32 AtlasZ = CVarZ->GetValueOnAnyThread();

		BlockAllocator = FTextureLayout3d(0, 0, 0, AtlasXY, AtlasXY, AtlasZ, false, false);
	}
}

FString FDistanceFieldVolumeTextureAtlas::GetSizeString() const
{
	if (VolumeTextureRHI)
	{
		const int32 FormatSize = GPixelFormats[Format].BlockBytes;

		size_t BackingDataBytes = 0;

		for (int32 AllocationIndex = 0; AllocationIndex < CurrentAllocations.Num(); AllocationIndex++)
		{
			FDistanceFieldVolumeTexture* Texture = CurrentAllocations[AllocationIndex];
			BackingDataBytes += Texture->VolumeData.CompressedDistanceFieldVolume.Num() * Texture->VolumeData.CompressedDistanceFieldVolume.GetTypeSize();
		}

		for (int32 AllocationIndex = 0; AllocationIndex < PendingAllocations.Num(); AllocationIndex++)
		{
			FDistanceFieldVolumeTexture* Texture = PendingAllocations[AllocationIndex];
			BackingDataBytes += Texture->VolumeData.CompressedDistanceFieldVolume.Num() * Texture->VolumeData.CompressedDistanceFieldVolume.GetTypeSize();
		}

		float AtlasMemorySize = VolumeTextureRHI->GetSizeX() * VolumeTextureRHI->GetSizeY() * VolumeTextureRHI->GetSizeZ() * FormatSize / 1024.0f / 1024.0f;
		return FString::Printf(TEXT("Allocated %ux%ux%u distance field atlas = %.1fMb, with %u objects containing %.1fMb backing data"), 
			VolumeTextureRHI->GetSizeX(), 
			VolumeTextureRHI->GetSizeY(), 
			VolumeTextureRHI->GetSizeZ(), 
			AtlasMemorySize,
			CurrentAllocations.Num() + PendingAllocations.Num(),
			BackingDataBytes / 1024.0f / 1024.0f);
	}
	else
	{
		return TEXT("");
	}
}

struct FMeshDistanceFieldStats
{
	size_t MemoryBytes;
	float ResolutionScale;
	UStaticMesh* Mesh;
};

void FDistanceFieldVolumeTextureAtlas::ListMeshDistanceFields() const
{
	TArray<FMeshDistanceFieldStats> GatheredStats;

	const int32 FormatSize = GPixelFormats[Format].BlockBytes;

	for (int32 AllocationIndex = 0; AllocationIndex < CurrentAllocations.Num(); AllocationIndex++)
	{
		FDistanceFieldVolumeTexture* Texture = CurrentAllocations[AllocationIndex];
		FMeshDistanceFieldStats Stats;
		size_t AtlasMemory = Texture->VolumeData.Size.X * Texture->VolumeData.Size.Y * Texture->VolumeData.Size.Z * FormatSize;
		size_t BackingMemory = Texture->VolumeData.CompressedDistanceFieldVolume.Num() * Texture->VolumeData.CompressedDistanceFieldVolume.GetTypeSize();
		Stats.MemoryBytes = AtlasMemory + BackingMemory;
		Stats.Mesh = Texture->GetStaticMesh();
#if WITH_EDITORONLY_DATA
		Stats.ResolutionScale = Stats.Mesh->SourceModels[0].BuildSettings.DistanceFieldResolutionScale;
#else
		Stats.ResolutionScale = -1;
#endif
		GatheredStats.Add(Stats);
	}

	struct FMeshDistanceFieldStatsSorter
	{
		bool operator()( const FMeshDistanceFieldStats& A, const FMeshDistanceFieldStats& B ) const
		{
			return A.MemoryBytes > B.MemoryBytes;
		}
	};

	GatheredStats.Sort(FMeshDistanceFieldStatsSorter());

	size_t TotalMemory = 0;

	for (int32 EntryIndex = 0; EntryIndex < GatheredStats.Num(); EntryIndex++)
	{
		const FMeshDistanceFieldStats& MeshStats = GatheredStats[EntryIndex];
		TotalMemory += MeshStats.MemoryBytes;
	}

	UE_LOG(LogStaticMesh, Log, TEXT("Dumping mesh distance fields for %u meshes, total %.1fMb"), GatheredStats.Num(), TotalMemory / 1024.0f / 1024.0f);
	UE_LOG(LogStaticMesh, Log, TEXT("   Memory Mb, Scale, Name, Path"));

	for (int32 EntryIndex = 0; EntryIndex < GatheredStats.Num(); EntryIndex++)
	{
		const FMeshDistanceFieldStats& MeshStats = GatheredStats[EntryIndex];
		UE_LOG(LogStaticMesh, Log, TEXT("   %.2f, %.1f, %s, %s"), MeshStats.MemoryBytes / 1024.0f / 1024.0f, MeshStats.ResolutionScale, *MeshStats.Mesh->GetName(), *MeshStats.Mesh->GetPathName());
	}
}

void FDistanceFieldVolumeTextureAtlas::AddAllocation(FDistanceFieldVolumeTexture* Texture)
{
	InitializeIfNeeded();
	PendingAllocations.AddUnique(Texture);
}

void FDistanceFieldVolumeTextureAtlas::RemoveAllocation(FDistanceFieldVolumeTexture* Texture)
{
	InitializeIfNeeded();
	PendingAllocations.Remove(Texture);

	if (CurrentAllocations.Contains(Texture))
	{
		const FIntVector Min = Texture->GetAllocationMin();
		const FIntVector Size = Texture->VolumeData.Size;
		verify(BlockAllocator.RemoveElement(Min.X, Min.Y, Min.Z, Size.X, Size.Y, Size.Z));
		CurrentAllocations.Remove(Texture);
	}
}

struct FCompareVolumeAllocation
{
	FORCEINLINE bool operator()(const FDistanceFieldVolumeTexture& A, const FDistanceFieldVolumeTexture& B) const
	{
		return A.GetAllocationVolume() > B.GetAllocationVolume();
	}
};

void FDistanceFieldVolumeTextureAtlas::UpdateAllocations()
{
	if (PendingAllocations.Num() > 0 || GDistanceFieldForceAtlasRealloc != 0)
	{
		const double StartTime = FPlatformTime::Seconds();

		// Sort largest to smallest for best packing
		PendingAllocations.Sort(FCompareVolumeAllocation());

		for (int32 AllocationIndex = 0; AllocationIndex < PendingAllocations.Num(); AllocationIndex++)
		{
			FDistanceFieldVolumeTexture* Texture = PendingAllocations[AllocationIndex];
			const FIntVector Size = Texture->VolumeData.Size;

			if (!BlockAllocator.AddElement((uint32&)Texture->AtlasAllocationMin.X, (uint32&)Texture->AtlasAllocationMin.Y, (uint32&)Texture->AtlasAllocationMin.Z, Size.X, Size.Y, Size.Z))
			{
				UE_LOG(LogStaticMesh,Error,TEXT("Failed to allocate %ux%ux%u in distance field atlas"), Size.X, Size.Y, Size.Z);
				PendingAllocations.RemoveAt(AllocationIndex);
				AllocationIndex--;
			}
		}

		static const auto CVarCompress = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFieldBuild.Compress"));
		const bool bDataIsCompressed = CVarCompress->GetValueOnAnyThread() != 0;

		const int32 FormatSize = GPixelFormats[Format].BlockBytes;

		if (!VolumeTextureRHI
			|| BlockAllocator.GetSizeX() > VolumeTextureRHI->GetSizeX()
			|| BlockAllocator.GetSizeY() > VolumeTextureRHI->GetSizeY()
			|| BlockAllocator.GetSizeZ() > VolumeTextureRHI->GetSizeZ()
			|| GDistanceFieldForceAtlasRealloc)
		{
			if (CurrentAllocations.Num() > 0)
			{
				static const auto CVarXY = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFields.AtlasSizeXY"));
				const int32 AtlasXY = CVarXY->GetValueOnAnyThread();

				static const auto CVarZ = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFields.AtlasSizeZ"));
				const int32 AtlasZ = CVarZ->GetValueOnAnyThread();

				// Remove all allocations from the layout so we have a clean slate
				BlockAllocator = FTextureLayout3d(0, 0, 0, AtlasXY, AtlasXY, AtlasZ, false, false);
				
				Generation++;

				// Re-upload all textures since we had to reallocate
				PendingAllocations.Append(CurrentAllocations);
				CurrentAllocations.Empty();

				// Sort largest to smallest for best packing
				PendingAllocations.Sort(FCompareVolumeAllocation());

				// Add all allocations back to the layout
				for (int32 AllocationIndex = 0; AllocationIndex < PendingAllocations.Num(); AllocationIndex++)
				{
					FDistanceFieldVolumeTexture* Texture = PendingAllocations[AllocationIndex];
					const FIntVector Size = Texture->VolumeData.Size;

					if (!BlockAllocator.AddElement((uint32&)Texture->AtlasAllocationMin.X, (uint32&)Texture->AtlasAllocationMin.Y, (uint32&)Texture->AtlasAllocationMin.Z, Size.X, Size.Y, Size.Z))
					{
						UE_LOG(LogStaticMesh,Error,TEXT("Failed to allocate %ux%ux%u in distance field atlas"), Size.X, Size.Y, Size.Z);
						PendingAllocations.RemoveAt(AllocationIndex);
						AllocationIndex--;
					}
				}
			}

			// Fully free the previous atlas memory before allocating a new one
			{
				// Remove last ref, add to deferred delete list
				VolumeTextureRHI = NULL;

				// Flush commandlist, flush RHI thread, delete deferred resources (GNM Memblock defers further)
				FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);

				// Flush GPU, flush GNM Memblock free
				RHIFlushResources();
			}

			FRHIResourceCreateInfo CreateInfo;

			VolumeTextureRHI = RHICreateTexture3D(
				BlockAllocator.GetSizeX(), 
				BlockAllocator.GetSizeY(), 
				BlockAllocator.GetSizeZ(), 
				Format,
				1,
				TexCreate_ShaderResource,
				CreateInfo);

			UE_LOG(LogStaticMesh,Log,TEXT("%s"),*GetSizeString());

			// Full update, coalesce the thousands of small allocations into a single array for RHIUpdateTexture3D
			// D3D12 has a huge alignment requirement which results in 6Gb of staging textures being needed to update a 112Mb atlas in small chunks otherwise (FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT)
			{
				const int32 Pitch = BlockAllocator.GetSizeX() * FormatSize;
				const int32 DepthPitch = BlockAllocator.GetSizeX() * BlockAllocator.GetSizeY() * FormatSize;

				const FUpdateTextureRegion3D UpdateRegion(FIntVector::ZeroValue, FIntVector::ZeroValue, BlockAllocator.GetSize());
				FUpdateTexture3DData TextureUpdateData = RHIBeginUpdateTexture3D(VolumeTextureRHI, 0, UpdateRegion);

				TArray<uint8> UncompressedData;

				for (int32 AllocationIndex = 0; AllocationIndex < PendingAllocations.Num(); AllocationIndex++)
				{
					FDistanceFieldVolumeTexture* Texture = PendingAllocations[AllocationIndex];
					const FIntVector Size = Texture->VolumeData.Size;
					const int32 UncompressedSize = Size.X * Size.Y * Size.Z * FormatSize;

					const TArray<uint8>* SourceDataPtr = NULL;

					if (bDataIsCompressed)
					{
						UncompressedData.Reset(UncompressedSize);
						UncompressedData.AddUninitialized(UncompressedSize);

						verify(FCompression::UncompressMemory((ECompressionFlags)COMPRESS_ZLIB, UncompressedData.GetData(), UncompressedSize, Texture->VolumeData.CompressedDistanceFieldVolume.GetData(), Texture->VolumeData.CompressedDistanceFieldVolume.Num()));

						SourceDataPtr = &UncompressedData;
					}
					else
					{
						// Update the volume texture atlas
						check(Texture->VolumeData.CompressedDistanceFieldVolume.Num() == Size.X * Size.Y * Size.Z * FormatSize);
						SourceDataPtr = &Texture->VolumeData.CompressedDistanceFieldVolume;
					}

					const int32 SourcePitch = Size.X * FormatSize;
					check(SourcePitch <= (int32)TextureUpdateData.RowPitch);

					// Copy each row into the correct position in TextureUpdateData.Data
					for (int32 ZIndex = 0; ZIndex < Size.Z; ZIndex++)
					{
						const int32 DestZIndex = (Texture->AtlasAllocationMin.Z + ZIndex) * TextureUpdateData.DepthPitch + Texture->AtlasAllocationMin.X * FormatSize;
						const int32 SourceZIndex = ZIndex * Size.Y * SourcePitch;

						for (int32 YIndex = 0; YIndex < Size.Y; YIndex++)
						{
							const int32 DestIndex = DestZIndex + (Texture->AtlasAllocationMin.Y + YIndex) * TextureUpdateData.RowPitch;
							const int32 SourceIndex = SourceZIndex + YIndex * SourcePitch;
							check( uint32(DestIndex) + SourcePitch <= TextureUpdateData.DataSizeBytes );
							FMemory::Memcpy((uint8*)&TextureUpdateData.Data[DestIndex], (const uint8*)&(*SourceDataPtr)[SourceIndex], SourcePitch );
						}
					}
				}

				UncompressedData.Empty();
				RHIEndUpdateTexture3D(TextureUpdateData);
			}
		}
		else
		{
			for (int32 AllocationIndex = 0; AllocationIndex < PendingAllocations.Num(); AllocationIndex++)
			{
				FDistanceFieldVolumeTexture* Texture = PendingAllocations[AllocationIndex];
				const FIntVector Size = Texture->VolumeData.Size;

				const FUpdateTextureRegion3D UpdateRegion(Texture->AtlasAllocationMin, FIntVector::ZeroValue, Size);
				const int32 UncompressedSize = Size.X * Size.Y * Size.Z * FormatSize;

				if (bDataIsCompressed)
				{
					TArray<uint8> UncompressedData;
					UncompressedData.Empty(UncompressedSize);
					UncompressedData.AddUninitialized(UncompressedSize);

					verify(FCompression::UncompressMemory((ECompressionFlags)COMPRESS_ZLIB, UncompressedData.GetData(), UncompressedSize, Texture->VolumeData.CompressedDistanceFieldVolume.GetData(), Texture->VolumeData.CompressedDistanceFieldVolume.Num()));

					// Update the volume texture atlas
					RHIUpdateTexture3D(VolumeTextureRHI, 0, UpdateRegion, Size.X * FormatSize, Size.X * Size.Y * FormatSize, (const uint8*)UncompressedData.GetData());
				}
				else
				{
					// Update the volume texture atlas
					check(Texture->VolumeData.CompressedDistanceFieldVolume.Num() == Size.X * Size.Y * Size.Z * FormatSize);
					RHIUpdateTexture3D(VolumeTextureRHI, 0, UpdateRegion, Size.X * FormatSize, Size.X * Size.Y * FormatSize, (const uint8*)Texture->VolumeData.CompressedDistanceFieldVolume.GetData());
				}
			}
		}

		CurrentAllocations.Append(PendingAllocations);
		PendingAllocations.Empty();

		const double EndTime = FPlatformTime::Seconds();
		const float UpdateDurationMs = (float)(EndTime - StartTime) * 1000.0f;

		if (UpdateDurationMs > 10.0f)
		{
			UE_LOG(LogStaticMesh,Verbose,TEXT("FDistanceFieldVolumeTextureAtlas::UpdateAllocations took %.1fms"), UpdateDurationMs);
		}
		GDistanceFieldForceAtlasRealloc = 0;
	}	
}

FDistanceFieldVolumeTexture::~FDistanceFieldVolumeTexture()
{
	if (FApp::CanEverRender())
	{
		// Make sure we have been properly removed from the atlas before deleting
		check(!bReferencedByAtlas);
	}
}

void FDistanceFieldVolumeTexture::Initialize(UStaticMesh* InStaticMesh)
{
	if (IsValidDistanceFieldVolume())
	{
		StaticMesh = InStaticMesh;

		bReferencedByAtlas = true;

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			AddAllocation,
			FDistanceFieldVolumeTexture*, DistanceFieldVolumeTexture, this,
			{
				GDistanceFieldVolumeTextureAtlas.AddAllocation(DistanceFieldVolumeTexture);
			}
		);
	}
}

void FDistanceFieldVolumeTexture::Release()
{
	if (bReferencedByAtlas)
	{
		StaticMesh = NULL;

		bReferencedByAtlas = false;

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ReleaseAllocation,
			FDistanceFieldVolumeTexture*, DistanceFieldVolumeTexture, this,
			{
				GDistanceFieldVolumeTextureAtlas.RemoveAllocation(DistanceFieldVolumeTexture);
			}
		);
	}
}

FIntVector FDistanceFieldVolumeTexture::GetAllocationSize() const
{
	return VolumeData.Size;
}

bool FDistanceFieldVolumeTexture::IsValidDistanceFieldVolume() const
{
	return VolumeData.Size.GetMax() > 0;
}

FDistanceFieldAsyncQueue* GDistanceFieldAsyncQueue = NULL;

#if WITH_EDITOR

// DDC key for distance field data, must be changed when modifying the generation code or data format
#define DISTANCEFIELD_DERIVEDDATA_VER TEXT("E1AE9CB64EF64BA9A5EA17E72C88F9D")

FString BuildDistanceFieldDerivedDataKey(const FString& InMeshKey)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFields.MaxPerMeshResolution"));
	const int32 PerMeshMax = CVar->GetValueOnAnyThread();
	const FString PerMeshMaxString = PerMeshMax == 128 ? TEXT("") : FString(TEXT("_%u"), PerMeshMax);

	static const auto CVarDensity = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.DistanceFields.DefaultVoxelDensity"));
	const float VoxelDensity = CVarDensity->GetValueOnAnyThread();
	const FString VoxelDensityString = VoxelDensity == .1f ? TEXT("") : FString(TEXT("_%.3f"), VoxelDensity);

	static const auto CVarCompress = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFieldBuild.Compress"));
	const bool bCompress = CVarCompress->GetValueOnAnyThread() != 0;
	const FString CompressString = bCompress ? TEXT("") : TEXT("_uc");

	static const auto CVarEightBit = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFieldBuild.EightBit"));
	const bool bEightBitFixedPoint = CVarEightBit->GetValueOnAnyThread() != 0;
	const FString FormatString = bEightBitFixedPoint ? TEXT("_8u") : TEXT("");

	static const auto CVarEmbree = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFieldBuild.UseEmbree"));
	const bool bUseEmbree = CVarEmbree->GetValueOnAnyThread() != 0;
	const FString EmbreeString = bUseEmbree ? TEXT("_e") : TEXT("");

	return FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("DIST"),
		*FString::Printf(TEXT("%s_%s%s%s%s%s%s"), *InMeshKey, DISTANCEFIELD_DERIVEDDATA_VER, *PerMeshMaxString, *VoxelDensityString, *CompressString, *FormatString, *EmbreeString),
		TEXT(""));
}

#endif

#if WITH_EDITORONLY_DATA

void FDistanceFieldVolumeData::CacheDerivedData(const FString& InDDCKey, UStaticMesh* Mesh, UStaticMesh* GenerateSource, float DistanceFieldResolutionScale, bool bGenerateDistanceFieldAsIfTwoSided)
{
	TArray<uint8> DerivedData;

	COOK_STAT(auto Timer = DistanceFieldCookStats::UsageStats.TimeSyncWork());
	if (GetDerivedDataCacheRef().GetSynchronous(*InDDCKey, DerivedData))
	{
		COOK_STAT(Timer.AddHit(DerivedData.Num()));
		FMemoryReader Ar(DerivedData, /*bIsPersistent=*/ true);
		Ar << *this;
	}
	else
	{
		// We don't actually build the resource until later, so only track the cycles used here.
		COOK_STAT(Timer.TrackCyclesOnly());
		FAsyncDistanceFieldTask* NewTask = new FAsyncDistanceFieldTask;
		NewTask->DDCKey = InDDCKey;
		check(Mesh && GenerateSource);
		NewTask->StaticMesh = Mesh;
		NewTask->GenerateSource = GenerateSource;
		NewTask->DistanceFieldResolutionScale = DistanceFieldResolutionScale;
		NewTask->bGenerateDistanceFieldAsIfTwoSided = bGenerateDistanceFieldAsIfTwoSided;
		NewTask->GeneratedVolumeData = new FDistanceFieldVolumeData();

		for (int32 MaterialIndex = 0; MaterialIndex < Mesh->StaticMaterials.Num(); MaterialIndex++)
		{
			// Default material blend mode
			EBlendMode BlendMode = BLEND_Opaque;

			if (Mesh->StaticMaterials[MaterialIndex].MaterialInterface)
			{
				BlendMode = Mesh->StaticMaterials[MaterialIndex].MaterialInterface->GetBlendMode();
			}

			NewTask->MaterialBlendModes.Add(BlendMode);
		}

		GDistanceFieldAsyncQueue->AddTask(NewTask);
	}
}

#endif

int32 GUseAsyncDistanceFieldBuildQueue = 1;
static FAutoConsoleVariableRef CVarAOAsyncBuildQueue(
	TEXT("r.AOAsyncBuildQueue"),
	GUseAsyncDistanceFieldBuildQueue,
	TEXT("Whether to asynchronously build distance field volume data from meshes."),
	ECVF_Default | ECVF_ReadOnly
	);

class FBuildDistanceFieldThreadRunnable : public FRunnable
{
public:
	/** Initialization constructor. */
	FBuildDistanceFieldThreadRunnable(
		FDistanceFieldAsyncQueue* InAsyncQueue
		)
		: NextThreadIndex(0)
		, AsyncQueue(*InAsyncQueue)
		, Thread(nullptr)
		, bIsRunning(false)
		, bForceFinish(false)
	{}

	virtual ~FBuildDistanceFieldThreadRunnable()
	{
		check(!bIsRunning);
	}

	// FRunnable interface.
	virtual bool Init() { bIsRunning = true; return true; }
	virtual void Exit() { bIsRunning = false; }
	virtual void Stop() { bForceFinish = true; }
	virtual uint32 Run();

	void Launch()
	{
		check(!bIsRunning);

		bForceFinish = false;
		Thread.Reset(FRunnableThread::Create(this, *FString::Printf(TEXT("BuildDistanceFieldThread%u"), NextThreadIndex), 0, TPri_Normal, FPlatformAffinity::GetPoolThreadMask()));
		NextThreadIndex++;
	}

	inline bool IsRunning() { return bIsRunning; }

private:

	int32 NextThreadIndex;

	FDistanceFieldAsyncQueue& AsyncQueue;

	/** The runnable thread */
	TUniquePtr<FRunnableThread> Thread;

	TUniquePtr<FQueuedThreadPool> WorkerThreadPool;

	volatile bool bIsRunning;
	volatile bool bForceFinish;
};

FQueuedThreadPool* CreateWorkerThreadPool()
{
	const int32 NumThreads = FMath::Max<int32>(FPlatformMisc::NumberOfCoresIncludingHyperthreads() - 2, 1);
	FQueuedThreadPool* WorkerThreadPool = FQueuedThreadPool::Allocate();
	WorkerThreadPool->Create(NumThreads, 32 * 1024, TPri_BelowNormal);
	return WorkerThreadPool;
}

uint32 FBuildDistanceFieldThreadRunnable::Run()
{
	bool bHasWork = true;

	while (!bForceFinish && bHasWork)
	{
		// LIFO build order, since meshes actually visible in a map are typically loaded last
		FAsyncDistanceFieldTask* Task = AsyncQueue.TaskQueue.Pop();

		if (Task)
		{
			if (!WorkerThreadPool)
			{
				WorkerThreadPool.Reset(CreateWorkerThreadPool());
			}

			AsyncQueue.Build(Task, *WorkerThreadPool);
			bHasWork = true;
		}
		else
		{
			bHasWork = false;
		}
	}

	WorkerThreadPool = nullptr;

	return 0;
}

FAsyncDistanceFieldTask::FAsyncDistanceFieldTask()
	: StaticMesh(nullptr)
	, GenerateSource(nullptr)
	, DistanceFieldResolutionScale(0.0f)
	, bGenerateDistanceFieldAsIfTwoSided(false)
	, GeneratedVolumeData(nullptr)
{
}


FDistanceFieldAsyncQueue::FDistanceFieldAsyncQueue() 
{
#if WITH_EDITOR
	MeshUtilities = NULL;
#endif

	ThreadRunnable = MakeUnique<FBuildDistanceFieldThreadRunnable>(this);
}

FDistanceFieldAsyncQueue::~FDistanceFieldAsyncQueue()
{
}

void FDistanceFieldAsyncQueue::AddTask(FAsyncDistanceFieldTask* Task)
{
#if WITH_EDITOR
	if (!MeshUtilities)
	{
		MeshUtilities = &FModuleManager::Get().LoadModuleChecked<IMeshUtilities>(TEXT("MeshUtilities"));
	}
	
	ReferencedTasks.Add(Task);

	if (GUseAsyncDistanceFieldBuildQueue)
	{
		TaskQueue.Push(Task);

		if (!ThreadRunnable->IsRunning())
		{
			ThreadRunnable->Launch();
		}
	}
	else
	{
		TUniquePtr<FQueuedThreadPool> WorkerThreadPool(CreateWorkerThreadPool());
		Build(Task, *WorkerThreadPool);
	}
#else
	UE_LOG(LogStaticMesh,Fatal,TEXT("Tried to build a distance field without editor support (this should have been done during cooking)"));
#endif
}

void FDistanceFieldAsyncQueue::BlockUntilBuildComplete(UStaticMesh* StaticMesh, bool bWarnIfBlocked)
{
	// We will track the wait time here, but only the cycles used.
	// This function is called whether or not an async task is pending, 
	// so we have to look elsewhere to properly count how many resources have actually finished building.
	COOK_STAT(auto Timer = DistanceFieldCookStats::UsageStats.TimeAsyncWait());
	COOK_STAT(Timer.TrackCyclesOnly());
	bool bReferenced = false;
	bool bHadToBlock = false;
	double StartTime = 0;

	do 
	{
		ProcessAsyncTasks();

		bReferenced = false;

		for (int TaskIndex = 0; TaskIndex < ReferencedTasks.Num(); TaskIndex++)
		{
			bReferenced = bReferenced || ReferencedTasks[TaskIndex]->StaticMesh == StaticMesh;
			bReferenced = bReferenced || ReferencedTasks[TaskIndex]->GenerateSource == StaticMesh;
		}

		if (bReferenced)
		{
			if (!bHadToBlock)
			{
				StartTime = FPlatformTime::Seconds();
			}

			bHadToBlock = true;
			FPlatformProcess::Sleep(.01f);
		}
	} 
	while (bReferenced);

	if (bHadToBlock &&
		bWarnIfBlocked
#if WITH_EDITOR
		&& !FAutomationTestFramework::Get().GetCurrentTest() // HACK - Don't output this warning during automation test
#endif
		)
	{
		UE_LOG(LogStaticMesh, Warning, TEXT("Main thread blocked for %.3fs for async distance field build of %s to complete!  This can happen if the mesh is rebuilt excessively."),
			(float)(FPlatformTime::Seconds() - StartTime), 
			*StaticMesh->GetName());
	}
}

void FDistanceFieldAsyncQueue::BlockUntilAllBuildsComplete()
{
	do 
	{
		ProcessAsyncTasks();
		FPlatformProcess::Sleep(.01f);
	} 
	while (GetNumOutstandingTasks() > 0);
}

void FDistanceFieldAsyncQueue::Build(FAsyncDistanceFieldTask* Task, FQueuedThreadPool& ThreadPool)
{
#if WITH_EDITOR

	// Editor 'force delete' can null any UObject pointers which are seen by reference collecting (eg UProperty or serialized)
	if (Task->StaticMesh && Task->GenerateSource)
	{
		const FStaticMeshLODResources& LODModel = Task->GenerateSource->RenderData->LODResources[0];

		MeshUtilities->GenerateSignedDistanceFieldVolumeData(
			Task->StaticMesh->GetName(),
			LODModel,
			ThreadPool,
			Task->MaterialBlendModes,
			Task->GenerateSource->RenderData->Bounds,
			Task->DistanceFieldResolutionScale,
			Task->bGenerateDistanceFieldAsIfTwoSided,
			*Task->GeneratedVolumeData);
	}

    CompletedTasks.Push(Task);

#endif
}

void FDistanceFieldAsyncQueue::AddReferencedObjects(FReferenceCollector& Collector)
{	
	for (int TaskIndex = 0; TaskIndex < ReferencedTasks.Num(); TaskIndex++)
	{
		// Make sure none of the UObjects referenced by the async tasks are GC'ed during the task
		Collector.AddReferencedObject(ReferencedTasks[TaskIndex]->StaticMesh);
		Collector.AddReferencedObject(ReferencedTasks[TaskIndex]->GenerateSource);
	}
}

void FDistanceFieldAsyncQueue::ProcessAsyncTasks()
{
#if WITH_EDITOR
	TArray<FAsyncDistanceFieldTask*> LocalCompletedTasks;
	CompletedTasks.PopAll(LocalCompletedTasks);

	for (int TaskIndex = 0; TaskIndex < LocalCompletedTasks.Num(); TaskIndex++)
	{
		// We want to count each resource built from a DDC miss, so count each iteration of the loop separately.
		COOK_STAT(auto Timer = DistanceFieldCookStats::UsageStats.TimeSyncWork());
		FAsyncDistanceFieldTask* Task = LocalCompletedTasks[TaskIndex];

		ReferencedTasks.Remove(Task);

		// Editor 'force delete' can null any UObject pointers which are seen by reference collecting (eg UProperty or serialized)
		if (Task->StaticMesh)
		{
			Task->GeneratedVolumeData->VolumeTexture.Initialize(Task->StaticMesh);
			FDistanceFieldVolumeData* OldVolumeData = Task->StaticMesh->RenderData->LODResources[0].DistanceFieldData;

			{
				// Cause all components using this static mesh to get re-registered, which will recreate their proxies and primitive uniform buffers
				FStaticMeshComponentRecreateRenderStateContext RecreateRenderStateContext(Task->StaticMesh, false);

				// Assign the new volume data
				Task->StaticMesh->RenderData->LODResources[0].DistanceFieldData = Task->GeneratedVolumeData;
			}

			OldVolumeData->VolumeTexture.Release();

			// Rendering thread may still be referencing the old one, use the deferred cleanup interface to delete it next frame when it is safe
			BeginCleanup(OldVolumeData);

			{
				TArray<uint8> DerivedData;
				// Save built distance field volume to DDC
				FMemoryWriter Ar(DerivedData, /*bIsPersistent=*/ true);
				Ar << *(Task->StaticMesh->RenderData->LODResources[0].DistanceFieldData);
				GetDerivedDataCacheRef().Put(*Task->DDCKey, DerivedData);
				COOK_STAT(Timer.AddMiss(DerivedData.Num()));
			}
		}

		delete Task;
	}

	if (ReferencedTasks.Num() > 0 && !ThreadRunnable->IsRunning())
	{
		ThreadRunnable->Launch();
	}
#endif
}

void FDistanceFieldAsyncQueue::Shutdown()
{
	ThreadRunnable->Stop();
	bool bLogged = false;

	while (ThreadRunnable->IsRunning())
	{
		if (!bLogged)
		{
			bLogged = true;
			UE_LOG(LogStaticMesh,Log,TEXT("Abandoning remaining async distance field tasks for shutdown"));
		}
		FPlatformProcess::Sleep(0.01f);
	}
}
