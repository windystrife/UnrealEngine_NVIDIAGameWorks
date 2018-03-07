// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightMap.cpp: Light-map implementation.
=============================================================================*/

#include "LightMap.h"
#include "UnrealEngine.h"
#include "Interfaces/ITargetPlatform.h"
#include "StaticLighting.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "InstancedStaticMesh.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Misc/FeedbackContext.h"
#include "UObject/Package.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/MapBuildDataRegistry.h"

#define VISUALIZE_PACKING 0

DEFINE_LOG_CATEGORY_STATIC(LogLightMap, Log, All);

FLightmassDebugOptions GLightmassDebugOptions;

/** Whether to use bilinear filtering on lightmaps */
bool GUseBilinearLightmaps = true;

/** Whether to allow padding around mappings. */
bool GAllowLightmapPadding = true;

/** Counts the number of lightmap textures generated each lighting build. */
ENGINE_API int32 GLightmapCounter = 0;
/** Whether to compress lightmaps. Reloaded from ini each lighting build. */
ENGINE_API bool GCompressLightmaps = true;

/** Whether to allow lighting builds to generate streaming lightmaps. */
ENGINE_API bool GAllowStreamingLightmaps = false;

/** Largest boundingsphere radius to use when packing lightmaps into a texture atlas. */
ENGINE_API float GMaxLightmapRadius = 5000.0f;	//10000.0;	//2000.0f;

/** The quality level of the current lighting build */
ELightingBuildQuality GLightingBuildQuality = Quality_Preview;

#if WITH_EDITOR
	/** Information about the lightmap sample that is selected */
	UNREALED_API extern FSelectedLightmapSample GCurrentSelectedLightmapSample;
#endif

/** The color to set selected texels to */
ENGINE_API FColor GTexelSelectionColor(255, 50, 0);

#if WITH_EDITOR
	// NOTE: We're only counting the top-level mip-map for the following variables.
	/** Total number of texels allocated for all lightmap textures. */
	ENGINE_API uint64 GNumLightmapTotalTexels = 0;
	/** Total number of texels used if the texture was non-power-of-two. */
	ENGINE_API uint64 GNumLightmapTotalTexelsNonPow2 = 0;
	/** Number of lightmap textures generated. */
	ENGINE_API int32 GNumLightmapTextures = 0;
	/** Total number of mapped texels. */
	ENGINE_API uint64 GNumLightmapMappedTexels = 0;
	/** Total number of unmapped texels. */
	ENGINE_API uint64 GNumLightmapUnmappedTexels = 0;
	/** Whether to allow cropping of unmapped borders in lightmaps and shadowmaps. Controlled by BaseEngine.ini setting. */
	ENGINE_API bool GAllowLightmapCropping = false;
	/** Total lightmap texture memory size (in bytes), including GLightmapTotalStreamingSize. */
	ENGINE_API uint64 GLightmapTotalSize = 0;
	/** Total memory size for streaming lightmaps (in bytes). */
	ENGINE_API uint64 GLightmapTotalStreamingSize = 0;
#endif

static TAutoConsoleVariable<int32> CVarTexelDebugging(
	TEXT("r.TexelDebugging"),
	0,	
	TEXT("Whether T + Left mouse click in the editor selects lightmap texels for debugging Lightmass.  Lightmass must be recompiled with ALLOW_LIGHTMAP_SAMPLE_DEBUGGING enabled for this to work."),
	ECVF_Default);

bool IsTexelDebuggingEnabled()
{
	return CVarTexelDebugging.GetValueOnGameThread() != 0;
}

#include "TextureLayout.h"

FLightMap::FLightMap()
	: bAllowHighQualityLightMaps(true)
	, NumRefs(0)
{
	bAllowHighQualityLightMaps = AllowHighQualityLightmaps(GMaxRHIFeatureLevel);
#if !PLATFORM_DESKTOP 
	checkf(bAllowHighQualityLightMaps || IsMobilePlatform(GMaxRHIShaderPlatform), TEXT("Low quality lightmaps are not currently supported on consoles. Make sure console variable r.HighQualityLightMaps is true for this platform"));
#endif
}

void FLightMap::Serialize(FArchive& Ar)
{
	Ar << LightGuids;
}

void FLightMap::Cleanup()
{
	BeginCleanup(this);
}

void FLightMap::FinishCleanup()
{
	delete this;
}

ULightMapTexture2D::ULightMapTexture2D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LODGroup = TEXTUREGROUP_Lightmap;
}

void ULightMapTexture2D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	uint32 Flags = LightmapFlags;
	Ar << Flags;
	LightmapFlags = ELightMapFlags( Flags );
}

/** 
 * Returns a one line description of an object for viewing in the generic browser
 */
FString ULightMapTexture2D::GetDesc()
{
	return FString::Printf( TEXT("Lightmap: %dx%d [%s]"), GetSizeX(), GetSizeY(), GPixelFormats[GetPixelFormat()].Name );
}

#if WITH_EDITOR
static void DumpLightmapSizeOnDisk()
{
	UE_LOG(LogLightMap,Log,TEXT("Lightmap size on disk"));
	UE_LOG(LogLightMap,Log,TEXT("Source (KB),Source is PNG,Platform Data (KB),Lightmap"));
	for (TObjectIterator<ULightMapTexture2D> It; It; ++It)
	{
		ULightMapTexture2D* Lightmap = *It;
		UE_LOG(LogLightMap,Log,TEXT("%f,%d,%f,%s"),
			Lightmap->Source.GetSizeOnDisk() / 1024.0f,
			Lightmap->Source.IsPNGCompressed(),
			Lightmap->CalcTextureMemorySizeEnum(TMC_AllMips) / 1024.0f,
			*Lightmap->GetPathName()
			);
	}
}

static FAutoConsoleCommand CmdDumpLightmapSizeOnDisk(
	TEXT("DumpLightmapSizeOnDisk"),
	TEXT("Dumps the size of all loaded lightmaps on disk (source and platform data)"),
	FConsoleCommandDelegate::CreateStatic(DumpLightmapSizeOnDisk)
	);
#endif // #if WITH_EDITOR

/** Lightmap resolution scaling factors for debugging.  The defaults are to use the original resolution unchanged. */
float TextureMappingDownsampleFactor0 = 1.0f;
int32 TextureMappingMinDownsampleSize0 = 16;
float TextureMappingDownsampleFactor1 = 1.0f;
int32 TextureMappingMinDownsampleSize1 = 128;
float TextureMappingDownsampleFactor2 = 1.0f;
int32 TextureMappingMinDownsampleSize2 = 256;

static int32 AdjustTextureMappingSize(int32 InSize)
{
	int32 NewSize = InSize;
	if (InSize > TextureMappingMinDownsampleSize0 && InSize <= TextureMappingMinDownsampleSize1)
	{
		NewSize = FMath::TruncToInt(InSize * TextureMappingDownsampleFactor0);
	}
	else if (InSize > TextureMappingMinDownsampleSize1 && InSize <= TextureMappingMinDownsampleSize2)
	{
		NewSize = FMath::TruncToInt(InSize * TextureMappingDownsampleFactor1);
	}
	else if (InSize > TextureMappingMinDownsampleSize2)
	{
		NewSize = FMath::TruncToInt(InSize * TextureMappingDownsampleFactor2);
	}
	return NewSize;
}

FStaticLightingMesh::FStaticLightingMesh(
	int32 InNumTriangles,
	int32 InNumShadingTriangles,
	int32 InNumVertices,
	int32 InNumShadingVertices,
	int32 InTextureCoordinateIndex,
	bool bInCastShadow,
	bool bInTwoSidedMaterial,
	const TArray<ULightComponent*>& InRelevantLights,
	const UPrimitiveComponent* const InComponent,
	const FBox& InBoundingBox,
	const FGuid& InGuid
	):
	NumTriangles(InNumTriangles),
	NumShadingTriangles(InNumShadingTriangles),
	NumVertices(InNumVertices),
	NumShadingVertices(InNumShadingVertices),
	TextureCoordinateIndex(InTextureCoordinateIndex),
	bCastShadow(bInCastShadow && InComponent->bCastStaticShadow),
	bTwoSidedMaterial(bInTwoSidedMaterial),
	RelevantLights(InRelevantLights),
	Component(InComponent),
	BoundingBox(InBoundingBox),
	Guid(FGuid::NewGuid()),
	SourceMeshGuid(InGuid),
	HLODTreeIndex(0),
	HLODChildStartIndex(0),
	HLODChildEndIndex(0)
{}

FStaticLightingTextureMapping::FStaticLightingTextureMapping(FStaticLightingMesh* InMesh,UObject* InOwner,int32 InSizeX,int32 InSizeY,int32 InLightmapTextureCoordinateIndex,bool bInBilinearFilter):
	FStaticLightingMapping(InMesh,InOwner),
	SizeX(AdjustTextureMappingSize(InSizeX)),
	SizeY(AdjustTextureMappingSize(InSizeY)),
	LightmapTextureCoordinateIndex(InLightmapTextureCoordinateIndex),
	bBilinearFilter(bInBilinearFilter)
{}

#if WITH_EDITOR

/**
 * An allocation of a region of light-map texture to a specific light-map.
 */
struct FLightMapAllocation
{
	/** 
	 * Basic constructor
	 */
	FLightMapAllocation()
	{
		MappedRect.Min.X = 0;
		MappedRect.Min.Y = 0;
		MappedRect.Max.X = 0;
		MappedRect.Max.Y = 0;
		Primitive = nullptr;
		Registry = NULL;
		InstanceIndex = INDEX_NONE;
		bSkipEncoding = false;
	}

	/**
	 * Copy construct from FQuantizedLightmapData
	 */
	FLightMapAllocation(FQuantizedLightmapData&& QuantizedData)
		: TotalSizeX(QuantizedData.SizeX)
		, TotalSizeY(QuantizedData.SizeY)
		, bHasSkyShadowing(QuantizedData.bHasSkyShadowing)
		, RawData(MoveTemp(QuantizedData.Data))
	{
		FMemory::Memcpy(Scale, QuantizedData.Scale, sizeof(Scale));
		FMemory::Memcpy(Add, QuantizedData.Add, sizeof(Add));
		PaddingType = GAllowLightmapPadding ? LMPT_NormalPadding : LMPT_NoPadding;
		MappedRect.Min.X = 0;
		MappedRect.Min.Y = 0;
		MappedRect.Max.X = TotalSizeX;
		MappedRect.Max.Y = TotalSizeY;
		Primitive = nullptr;
		InstanceIndex = INDEX_NONE;
		bSkipEncoding = false;
	}

	// Called after the lightmap is encoded
	void PostEncode()
	{
		if (InstanceIndex >= 0 && Registry)
		{
			FMeshMapBuildData* MeshBuildData = Registry->GetMeshBuildData(MapBuildDataId);
			check(MeshBuildData);

			UInstancedStaticMeshComponent* Component = CastChecked<UInstancedStaticMeshComponent>(Primitive);

			// Instances may have been removed since LM allocation.
			// Instances may have also been shuffled from removes. We do not handle this case.
			if( InstanceIndex < MeshBuildData->PerInstanceLightmapData.Num() )
			{
				// TODO: We currently only support one LOD of static lighting in foliage
				// Need to create per-LOD instance data to fix that
				MeshBuildData->PerInstanceLightmapData[InstanceIndex].LightmapUVBias = LightMap->GetCoordinateBias();

				Component->PerInstanceRenderData->UpdateInstanceData(Component, InstanceIndex);
				Component->MarkRenderStateDirty();
			}
		}
	}

	TRefCountPtr<FLightMap2D> LightMap;

	UPrimitiveComponent* Primitive;
	UMapBuildDataRegistry* Registry;
	FGuid MapBuildDataId;
	int32			InstanceIndex;

	/** Upper-left X-coordinate in the texture atlas. */
	int32				OffsetX;
	/** Upper-left Y-coordinate in the texture atlas. */
	int32				OffsetY;
	/** Total number of texels along the X-axis. */
	int32				TotalSizeX;
	/** Total number of texels along the Y-axis. */
	int32				TotalSizeY;
	/** The rectangle of mapped texels within this mapping that is placed in the texture atlas. */
	FIntRect		MappedRect;
	bool			bDebug;
	bool			bHasSkyShadowing;
	ELightMapPaddingType			PaddingType;
	TArray<FLightMapCoefficients>	RawData;
	float							Scale[NUM_STORED_LIGHTMAP_COEF][4];
	float							Add[NUM_STORED_LIGHTMAP_COEF][4];
	/** True if we can skip encoding this allocation because it's similar enough to an existing
	    allocation at the same offset */
	bool bSkipEncoding;
};

struct FLightMapAllocationGroup
{
	FLightMapAllocationGroup()
		: Outer(nullptr)
		, LightmapFlags(LMF_None)
		, Bounds(ForceInit)
		, TotalTexels(0)
	{
	}

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FLightMapAllocationGroup(FLightMapAllocationGroup&&) = default;
	FLightMapAllocationGroup& operator=(FLightMapAllocationGroup&&) = default;

	FLightMapAllocationGroup(const FLightMapAllocationGroup&) = delete; // non-copy-able
	FLightMapAllocationGroup& operator=(const FLightMapAllocationGroup&) = delete;
#else
	FLightMapAllocationGroup(FLightMapAllocationGroup&& other)
		: Allocations(MoveTemp(other.Allocations))
		, Outer(other.Outer)
		, LightmapFlags(other.LightmapFlags)
		, Bounds(other.Bounds)
		, TotalTexels(other.TotalTexels)
	{
	}

	FLightMapAllocationGroup& operator=(FLightMapAllocationGroup&& other)
	{
		Allocations = MoveTemp(other.Allocations);
		Outer = other.Outer;
		LightmapFlags = other.LightmapFlags;
		Bounds = other.Bounds;
		TotalTexels = other.TotalTexels;

		return *this;
	}

private:
	FLightMapAllocationGroup(const FLightMapAllocationGroup&); // non-copy-able
	FLightMapAllocationGroup& operator=(const FLightMapAllocationGroup&);
public:
#endif

	TArray<TUniquePtr<FLightMapAllocation>, TInlineAllocator<1>> Allocations;

	UObject*		Outer;
	ELightMapFlags	LightmapFlags;

	// Bounds of the primitive that the mapping is applied to
	// Used to group nearby allocations into the same lightmap texture
	FBoxSphereBounds Bounds;

	int32			TotalTexels;
};

/**
 * A light-map texture which has been partially allocated, but not yet encoded.
 */
struct FLightMapPendingTexture : public FTextureLayout
{

	/** Helper data to keep track of the asynchronous tasks for the 4 lightmap textures. */
	ULightMapTexture2D*				Textures[NUM_STORED_LIGHTMAP_COEF];
	ULightMapTexture2D*				SkyOcclusionTexture;
	ULightMapTexture2D*				AOMaterialMaskTexture;

	TArray<TUniquePtr<FLightMapAllocation>> Allocations;
	UObject*						Outer;
	TWeakObjectPtr<UWorld>			OwningWorld;
	/** Bounding volume for all mappings within this texture.							*/
	FBoxSphereBounds				Bounds;

	/** Lightmap streaming flags that must match in order to be stored in this texture.	*/
	ELightMapFlags					LightmapFlags;
	// Optimization to quickly test if a new allocation won't fit
	// Primarily of benefit to instanced mesh lightmaps
	int32							UnallocatedTexels;
	int32							NumOutstandingAsyncTasks;
	bool							bUObjectsCreated;
	int32							NumNonPower2Texels;
	uint64							NumLightmapMappedTexels;
	uint64							NumLightmapUnmappedTexels;
	volatile bool					bIsFinishedEncoding; // has the encoding thread finished encoding (not the AsyncCache)
	bool							bHasRunPostEncode;
	bool							bTexelDebuggingEnabled;

	FLightMapPendingTexture(UWorld* InWorld, uint32 InSizeX,uint32 InSizeY)
		: FTextureLayout(4, 4, InSizeX, InSizeY, /* PowerOfTwo */ true, /* Force2To1Aspect */ true, /* AlignByFour */ true) // Min size is 4x4 in case of block compression.
		, SkyOcclusionTexture(nullptr)
		, AOMaterialMaskTexture(nullptr)
		, OwningWorld(InWorld)
		, Bounds(FBox(ForceInit))
		, LightmapFlags(LMF_None)
		, UnallocatedTexels(InSizeX * InSizeY)
		, NumOutstandingAsyncTasks(0)
		, bUObjectsCreated(false)
		, NumNonPower2Texels(0)
		, NumLightmapMappedTexels(0)
		, NumLightmapUnmappedTexels(0)
		, bIsFinishedEncoding(false)
		, bHasRunPostEncode(false)
		, bTexelDebuggingEnabled(IsTexelDebuggingEnabled())
	{}

	~FLightMapPendingTexture()
	{
	}

	void CreateUObjects();

	/**
	 * Processes the textures and starts asynchronous compression tasks for all mip-levels.
	 */
	void StartEncoding(ULevel* Unused, ITextureCompressorModule* UnusedCompressor);

	/**
	 * Call this function after the IsFinishedEncoding function returns true
	 */
	void PostEncode();

	/**
	 * IsFinishedCoding
	 * Are we ready to call PostEncode
	 * encode is run in a separate thread
	 * @return are we finished with the StartEncoding function yet
	 */
	bool IsFinishedEncoding() const;

	/**
	 * IsAsyncCacheComplete
	 * checks if any of our texture async caches are still running
	 */
	bool IsAsyncCacheComplete() const;

	/**
	 * Call this function after IsAscynCacheComplete returns true
	 */
	void FinishCachingTextures();

	
	
	/**
	 * Finds a free area in the texture large enough to contain a surface with the given size.
	 * If a large enough area is found, it is marked as in use, the output parameters OutBaseX and OutBaseY are
	 * set to the coordinates of the upper left corner of the free area and the function return true.
	 * Otherwise, the function returns false and OutBaseX and OutBaseY remain uninitialized.
	 *
	 * If the allocation succeeded, Allocation.OffsetX and Allocation.OffsetY will be set to the upper-left corner
	 * of the area allocated.
	 *
	 * @param Allocation	Lightmap allocation to try to fit
	 * @param bForceIntoThisTexture	True if we should ignore distance and other factors when considering whether the mapping should be packed into this texture
	 *
	 * @return	True if succeeded, false otherwise.
	 */
	bool AddElement(FLightMapAllocationGroup& AllocationGroup, const bool bForceIntoThisTexture = false);

private:
	/**
	* Finish caching the texture
	*/
	void FinishCacheTexture(UTexture2D* Texture);

	void PostEncode(UTexture2D* Texture);

	

	FName GetLightmapName(int32 TextureIndex, int32 CoefficientIndex);
	FName GetSkyOcclusionTextureName(int32 TextureIndex);
	FName GetAOMaterialMaskTextureName(int32 TextureIndex);
	bool NeedsSkyOcclusionTexture() const;
	bool NeedsAOMaterialMaskTexture() const;
};

/**
 * IsAsyncCacheComplete
 * checks if any of our texture async caches are still running
 */
bool FLightMapPendingTexture::IsAsyncCacheComplete() const
{
	check(IsInGameThread()); //updates global variables and accesses shared UObjects
	if (SkyOcclusionTexture && !SkyOcclusionTexture->IsAsyncCacheComplete())
	{
		return false;
	}

	if (AOMaterialMaskTexture && !AOMaterialMaskTexture->IsAsyncCacheComplete())
	{
		return false;
	}

	// Encode and compress the coefficient textures.
	for (uint32 CoefficientIndex = 0; CoefficientIndex < NUM_STORED_LIGHTMAP_COEF; CoefficientIndex += 2)
	{
		auto Texture = Textures[CoefficientIndex];
		if (Texture == nullptr)
		{
			continue;
		}
		if (!Texture->IsAsyncCacheComplete())
		{
			return false;
		}
	}
	return true;
}

/**
 * Called once the compression tasks for all mip-levels of a texture has finished.
 * Copies the compressed data into each of the mip-levels of the texture and deletes the tasks.
 *
 * @param CoefficientIndex	Texture coefficient index, identifying the specific texture with this FLightMapPendingTexture.
 */


void FLightMapPendingTexture::FinishCacheTexture(UTexture2D* Texture)
{
	check(IsInGameThread()); // updating global variables needs to be done in main thread
	check(Texture != nullptr);

	Texture->FinishCachePlatformData();
	Texture->UpdateResource();

	int32 TextureSize = Texture->CalcTextureMemorySizeEnum(TMC_AllMips);
	GLightmapTotalSize += TextureSize;
	GLightmapTotalStreamingSize += (LightmapFlags & LMF_Streamed) ? TextureSize : 0;
}

void FLightMapPendingTexture::PostEncode(UTexture2D* Texture)
{
	check(IsInGameThread());
	check(Texture != nullptr);
	Texture->CachePlatformData(true, true);
}


bool FLightMapPendingTexture::IsFinishedEncoding() const
{
	return bIsFinishedEncoding;
}

void FLightMapPendingTexture::PostEncode()
{
	check(IsInGameThread()); 
	check(bIsFinishedEncoding);

	if (bHasRunPostEncode)
	{
		return;
	}
	bHasRunPostEncode = true;

	for (int32 AllocationIndex = 0; AllocationIndex < Allocations.Num(); AllocationIndex++)
	{
		auto& Allocation = Allocations[AllocationIndex];

		int32 PaddedSizeX = Allocation->TotalSizeX;
		int32 PaddedSizeY = Allocation->TotalSizeY;
		int32 BaseX = Allocation->OffsetX - Allocation->MappedRect.Min.X;
		int32 BaseY = Allocation->OffsetY - Allocation->MappedRect.Min.Y;
		if (FPlatformProperties::HasEditorOnlyData() && GLightmassDebugOptions.bPadMappings && (Allocation->PaddingType == LMPT_NormalPadding))
		{
			if ((PaddedSizeX - 2 > 0) && ((PaddedSizeY - 2) > 0))
			{
				PaddedSizeX -= 2;
				PaddedSizeY -= 2;
				BaseX += 1;
				BaseY += 1;
			}
		}

		// Calculate the coordinate scale/biases this light-map.
		FVector2D Scale((float)PaddedSizeX / (float)GetSizeX(), (float)PaddedSizeY / (float)GetSizeY());
		FVector2D Bias((float)BaseX / (float)GetSizeX(), (float)BaseY / (float)GetSizeY());

		// Set the scale/bias of the lightmap
		check(Allocation->LightMap);
		Allocation->LightMap->CoordinateScale = Scale;
		Allocation->LightMap->CoordinateBias = Bias;
		Allocation->PostEncode();

		// Free the light-map's raw data.
		Allocation->RawData.Empty();
	}


	if (SkyOcclusionTexture!=nullptr)
	{
		PostEncode(SkyOcclusionTexture);
	}

	if (AOMaterialMaskTexture != nullptr)
	{
		PostEncode(AOMaterialMaskTexture);
	}


	// update all the global stats
	GNumLightmapMappedTexels += NumLightmapMappedTexels;
	GNumLightmapUnmappedTexels += NumLightmapUnmappedTexels;
	GNumLightmapTotalTexelsNonPow2 += NumNonPower2Texels;

	// Encode and compress the coefficient textures.
	for (uint32 CoefficientIndex = 0; CoefficientIndex < NUM_STORED_LIGHTMAP_COEF; CoefficientIndex += 2)
	{
		auto Texture = Textures[CoefficientIndex];
		if (Texture == nullptr)
		{
			continue;
		}
		
		PostEncode(Texture);

		GNumLightmapTotalTexels += Texture->Source.GetSizeX() * Texture->Source.GetSizeY();
		GNumLightmapTextures++;


		UPackage* TexturePackage = Texture->GetOutermost();
		if (OwningWorld.IsValid())
		{
			for (int32 LevelIndex = 0; TexturePackage && LevelIndex < OwningWorld->GetNumLevels(); LevelIndex++)
			{
				ULevel* Level = OwningWorld->GetLevel(LevelIndex);
				UPackage* LevelPackage = Level->GetOutermost();
				if (TexturePackage == LevelPackage)
				{
					Level->LightmapTotalSize += float(Texture->CalcTextureMemorySizeEnum(TMC_AllMips)) / 1024.0f;
					break;
				}
			}
		}
	}

}


void FLightMapPendingTexture::FinishCachingTextures()
{
	check(IsInGameThread()); //updates global variables and accesses shared UObjects
	if (SkyOcclusionTexture)
	{
		FinishCacheTexture(SkyOcclusionTexture);
	}

	if (AOMaterialMaskTexture)
	{
		FinishCacheTexture(AOMaterialMaskTexture);
	}

	// Encode and compress the coefficient textures.
	for (uint32 CoefficientIndex = 0; CoefficientIndex < NUM_STORED_LIGHTMAP_COEF; CoefficientIndex += 2)
	{
		auto& Texture = Textures[CoefficientIndex];
		if (Texture)
		{
			FinishCacheTexture(Texture);
		}
	}

}

/**
 * Finds a free area in the texture large enough to contain a surface with the given size.
 * If a large enough area is found, it is marked as in use, the output parameters OutBaseX and OutBaseY are
 * set to the coordinates of the upper left corner of the free area and the function return true.
 * Otherwise, the function returns false and OutBaseX and OutBaseY remain uninitialized.
 *
 * If the allocation succeeded, Allocation.OffsetX and Allocation.OffsetY will be set to the upper-left corner
 * of the allocated area.
 *
 * @param Allocation	Lightmap allocation group to try to fit
 * @param bForceIntoThisTexture	True if we should ignore distance and other factors when considering whether the mapping should be packed into this texture
 *
 * @return	True if succeeded, false otherwise.
 */
bool FLightMapPendingTexture::AddElement(FLightMapAllocationGroup& AllocationGroup, bool bForceIntoThisTexture)
{
	if (!bForceIntoThisTexture)
	{
		// Don't pack lightmaps from different packages into the same texture.
		if (Outer != AllocationGroup.Outer)
		{
			return false;
		}
	}

	// This is a rough test, passing it doesn't guarantee it'll fit
	// But failing it does guarantee that it _won't_ fit
	if (UnallocatedTexels < AllocationGroup.TotalTexels)
	{
		return false;
	}

	const bool bEmptyTexture = Allocations.Num() == 0;
	const FBoxSphereBounds NewBounds = bEmptyTexture ? AllocationGroup.Bounds : Bounds + AllocationGroup.Bounds;

	if (!bEmptyTexture && !bForceIntoThisTexture)
	{
		// Don't mix streaming lightmaps with non-streaming lightmaps.
		if ((LightmapFlags & LMF_Streamed) != (AllocationGroup.LightmapFlags & LMF_Streamed))
		{
			return false;
		}

		// Is this a streaming lightmap?
		if (LightmapFlags & LMF_Streamed)
		{
			bool bPerformDistanceCheck = true;

			// Don't pack together lightmaps that are too far apart
			if (bPerformDistanceCheck && NewBounds.SphereRadius > GMaxLightmapRadius && NewBounds.SphereRadius > (Bounds.SphereRadius + SMALL_NUMBER))
			{
				return false;
			}
		}
	}

	int32 NewUnallocatedTexels = UnallocatedTexels;

	int32 iAllocation = 0;
	for (; iAllocation < AllocationGroup.Allocations.Num(); ++iAllocation)
	{
		auto& Allocation = AllocationGroup.Allocations[iAllocation];
		uint32 BaseX, BaseY;
		const uint32 MappedRectWidth = Allocation->MappedRect.Width();
		const uint32 MappedRectHeight = Allocation->MappedRect.Height();
		if (FTextureLayout::AddElement(BaseX, BaseY, MappedRectWidth, MappedRectHeight))
		{
			Allocation->OffsetX = BaseX;
			Allocation->OffsetY = BaseY;

			// Assumes bAlignByFour
			NewUnallocatedTexels -= ((MappedRectWidth + 3) & ~3) * ((MappedRectHeight + 3) & ~3);
		}
		else
		{
			// failed to add all elements to the texture
			break;
		}
	}
	if (iAllocation < AllocationGroup.Allocations.Num())
	{
		// failed to add all elements to the texture
		// remove the ones added so far to restore our original state
		for (--iAllocation; iAllocation >= 0; --iAllocation)
		{
			auto& Allocation = AllocationGroup.Allocations[iAllocation];
			const uint32 MappedRectWidth = Allocation->MappedRect.Width();
			const uint32 MappedRectHeight = Allocation->MappedRect.Height();
			verify(FTextureLayout::RemoveElement(Allocation->OffsetX, Allocation->OffsetY, MappedRectWidth, MappedRectHeight));
		}
		return false;
	}

	Bounds = NewBounds;
	UnallocatedTexels = NewUnallocatedTexels;

	return true;
}

/** Whether to color each lightmap texture with a different (random) color. */
bool GVisualizeLightmapTextures = false;

static void GenerateLightmapMipsAndDilateColor(int32 NumMips, int32 TextureSizeX, int32 TextureSizeY, FColor TextureColor, FColor** MipData, int8** MipCoverageData)
{
	for(int32 MipIndex = 1; MipIndex < NumMips; MipIndex++)
	{
		const int32 SourceMipSizeX = FMath::Max(1, TextureSizeX >> (MipIndex - 1));
		const int32 SourceMipSizeY = FMath::Max(1, TextureSizeY >> (MipIndex - 1));
		const int32 DestMipSizeX = FMath::Max(1, TextureSizeX >> MipIndex);
		const int32 DestMipSizeY = FMath::Max(1, TextureSizeY >> MipIndex);

		// Downsample the previous mip-level, taking into account which texels are mapped.
		FColor* NextMipData = MipData[MipIndex];
		FColor* LastMipData = MipData[MipIndex - 1];

		int8* NextMipCoverageData = MipCoverageData[ MipIndex ];
		int8* LastMipCoverageData = MipCoverageData[ MipIndex - 1 ];

		const int32 MipFactorX = SourceMipSizeX / DestMipSizeX;
		const int32 MipFactorY = SourceMipSizeY / DestMipSizeY;

		//@todo - generate mips before encoding lightmaps!  
		// Currently we are filtering in the encoded space, similar to generating mips of sRGB textures in sRGB space
		for(int32 Y = 0; Y < DestMipSizeY; Y++)
		{
			for(int32 X = 0; X < DestMipSizeX; X++)
			{
				FLinearColor AccumulatedColor = FLinearColor::Black;
				uint32 Coverage = 0;

				const uint32 MinSourceY = (Y + 0) * MipFactorY;
				const uint32 MaxSourceY = (Y + 1) * MipFactorY;
				for(uint32 SourceY = MinSourceY; SourceY < MaxSourceY; SourceY++)
				{
					const uint32 MinSourceX = (X + 0) * MipFactorX;
					const uint32 MaxSourceX = (X + 1) * MipFactorX;
					for(uint32 SourceX = MinSourceX; SourceX < MaxSourceX; SourceX++)
					{
						const FColor& SourceColor = LastMipData[SourceY * SourceMipSizeX + SourceX];
						int8 SourceCoverage = LastMipCoverageData[ SourceY * SourceMipSizeX + SourceX ];
						if( SourceCoverage )
						{
							AccumulatedColor += SourceColor.ReinterpretAsLinear() * SourceCoverage;
							Coverage += SourceCoverage;
						}
					}
				}
				FColor& DestColor = NextMipData[Y * DestMipSizeX + X];
				int8& DestCoverage = NextMipCoverageData[Y * DestMipSizeX + X];
				if ( GVisualizeLightmapTextures )
				{
					DestColor = TextureColor;
					DestCoverage = 127;
				}
				else if(Coverage)
				{
					DestColor = ( AccumulatedColor / Coverage ).Quantize();
					DestCoverage = Coverage / (MipFactorX * MipFactorY);
				}
				else
				{
					DestColor = FColor(0,0,0);
					DestCoverage = 0;
				}
			}
		}
	}

	// Expand texels which are mapped into adjacent texels which are not mapped to avoid artifacts when using texture filtering.
	for(int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		FColor* MipLevelData = MipData[MipIndex];
		int8* MipLevelCoverageData = MipCoverageData[MipIndex];

		uint32 MipSizeX = FMath::Max(1,TextureSizeX >> MipIndex);
		uint32 MipSizeY = FMath::Max(1,TextureSizeY >> MipIndex);
		for(uint32 DestY = 0;DestY < MipSizeY;DestY++)
		{
			for(uint32 DestX = 0; DestX < MipSizeX; DestX++)
			{
				FColor& DestColor = MipLevelData[DestY * MipSizeX + DestX];
				int8& DestCoverage = MipLevelCoverageData[DestY * MipSizeX + DestX];
				if(DestCoverage == 0)
				{
					FLinearColor AccumulatedColor = FLinearColor::Black;
					uint32 Coverage = 0;

					const int32 MinSourceY = FMath::Max((int32)DestY - 1, (int32)0);
					const int32 MaxSourceY = FMath::Min((int32)DestY + 1, (int32)MipSizeY - 1);
					for(int32 SourceY = MinSourceY; SourceY <= MaxSourceY; SourceY++)
					{
						const int32 MinSourceX = FMath::Max((int32)DestX - 1, (int32)0);
						const int32 MaxSourceX = FMath::Min((int32)DestX + 1, (int32)MipSizeX - 1);
						for(int32 SourceX = MinSourceX; SourceX <= MaxSourceX; SourceX++)
						{
							FColor& SourceColor = MipLevelData[SourceY * MipSizeX + SourceX];
							int8 SourceCoverage = MipLevelCoverageData[SourceY * MipSizeX + SourceX];
							if( SourceCoverage > 0 )
							{
								static const uint32 Weights[3][3] =
								{
									{ 1, 255, 1 },
									{ 255, 0, 255 },
									{ 1, 255, 1 },
								};
								AccumulatedColor += SourceColor.ReinterpretAsLinear() * SourceCoverage * Weights[SourceX - DestX + 1][SourceY - DestY + 1];
								Coverage += SourceCoverage * Weights[SourceX - DestX + 1][SourceY - DestY + 1];
							}
						}
					}

					if(Coverage)
					{
						DestColor = (AccumulatedColor / Coverage).Quantize();
						DestCoverage = -1;
					}
				}
			}
		}
	}

	// Fill zero coverage texels with closest colors using mips
	for(int32 MipIndex = NumMips - 2; MipIndex >= 0; MipIndex--)
	{
		const int32 DstMipSizeX = FMath::Max(1, TextureSizeX >> MipIndex);
		const int32 DstMipSizeY = FMath::Max(1, TextureSizeY >> MipIndex);
		const int32 SrcMipSizeX = FMath::Max(1, TextureSizeX >> (MipIndex + 1));
		const int32 SrcMipSizeY = FMath::Max(1, TextureSizeY >> (MipIndex + 1));

		// Source from higher mip, taking into account which texels are mapped.
		FColor* DstMipData = MipData[MipIndex];
		FColor* SrcMipData = MipData[MipIndex + 1];

		int8* DstMipCoverageData = MipCoverageData[ MipIndex ];
		int8* SrcMipCoverageData = MipCoverageData[ MipIndex + 1 ];

		for(int32 DstY = 0; DstY < DstMipSizeY; DstY++)
		{
			for(int32 DstX = 0; DstX < DstMipSizeX; DstX++)
			{
				const uint32 SrcX = DstX / 2;
				const uint32 SrcY = DstY / 2;

				const FColor& SrcColor = SrcMipData[ SrcY * SrcMipSizeX + SrcX ];
				int8 SrcCoverage = SrcMipCoverageData[ SrcY * SrcMipSizeX + SrcX ];

				FColor& DstColor = DstMipData[ DstY * DstMipSizeX + DstX ];
				int8& DstCoverage = DstMipCoverageData[ DstY * DstMipSizeX + DstX ];

				// Point upsample mip data for zero coverage texels
				// TODO bilinear upsample
				if( SrcCoverage != 0 && DstCoverage == 0 )
				{
					DstColor = SrcColor;
					DstCoverage = SrcCoverage;
				}
			}
		}
	}
}

static void GenerateLightmapMipsAndDilateByte(int32 NumMips, int32 TextureSizeX, int32 TextureSizeY, uint8 TextureColor, uint8** MipData, int8** MipCoverageData)
{
	for(int32 MipIndex = 1; MipIndex < NumMips; MipIndex++)
	{
		const int32 SourceMipSizeX = FMath::Max(1, TextureSizeX >> (MipIndex - 1));
		const int32 SourceMipSizeY = FMath::Max(1, TextureSizeY >> (MipIndex - 1));
		const int32 DestMipSizeX = FMath::Max(1, TextureSizeX >> MipIndex);
		const int32 DestMipSizeY = FMath::Max(1, TextureSizeY >> MipIndex);

		// Downsample the previous mip-level, taking into account which texels are mapped.
		uint8* NextMipData = MipData[MipIndex];
		uint8* LastMipData = MipData[MipIndex - 1];

		int8* NextMipCoverageData = MipCoverageData[ MipIndex ];
		int8* LastMipCoverageData = MipCoverageData[ MipIndex - 1 ];

		const int32 MipFactorX = SourceMipSizeX / DestMipSizeX;
		const int32 MipFactorY = SourceMipSizeY / DestMipSizeY;

		//@todo - generate mips before encoding lightmaps!  
		// Currently we are filtering in the encoded space, similar to generating mips of sRGB textures in sRGB space
		for(int32 Y = 0; Y < DestMipSizeY; Y++)
		{
			for(int32 X = 0; X < DestMipSizeX; X++)
			{
				float AccumulatedColor = 0;
				uint32 Coverage = 0;

				const uint32 MinSourceY = (Y + 0) * MipFactorY;
				const uint32 MaxSourceY = (Y + 1) * MipFactorY;
				for(uint32 SourceY = MinSourceY; SourceY < MaxSourceY; SourceY++)
				{
					const uint32 MinSourceX = (X + 0) * MipFactorX;
					const uint32 MaxSourceX = (X + 1) * MipFactorX;
					for(uint32 SourceX = MinSourceX; SourceX < MaxSourceX; SourceX++)
					{
						const uint8& SourceColor = LastMipData[SourceY * SourceMipSizeX + SourceX];
						int8 SourceCoverage = LastMipCoverageData[ SourceY * SourceMipSizeX + SourceX ];
						if( SourceCoverage )
						{
							AccumulatedColor += SourceColor / 255.0f * SourceCoverage;
							Coverage += SourceCoverage;
						}
					}
				}
				uint8& DestColor = NextMipData[Y * DestMipSizeX + X];
				int8& DestCoverage = NextMipCoverageData[Y * DestMipSizeX + X];
				if ( GVisualizeLightmapTextures )
				{
					DestColor = TextureColor;
					DestCoverage = 127;
				}
				else if(Coverage)
				{
					DestColor = (uint8)FMath::Clamp<int32>(FMath::TruncToInt(AccumulatedColor / Coverage * 255.f), 0, 255);
					DestCoverage = Coverage / (MipFactorX * MipFactorY);
				}
				else
				{
					DestColor = 0;
					DestCoverage = 0;
				}
			}
		}
	}

	// Expand texels which are mapped into adjacent texels which are not mapped to avoid artifacts when using texture filtering.
	for(int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		uint8* MipLevelData = MipData[MipIndex];
		int8* MipLevelCoverageData = MipCoverageData[MipIndex];

		uint32 MipSizeX = FMath::Max(1,TextureSizeX >> MipIndex);
		uint32 MipSizeY = FMath::Max(1,TextureSizeY >> MipIndex);
		for(uint32 DestY = 0;DestY < MipSizeY;DestY++)
		{
			for(uint32 DestX = 0; DestX < MipSizeX; DestX++)
			{
				uint8& DestColor = MipLevelData[DestY * MipSizeX + DestX];
				int8& DestCoverage = MipLevelCoverageData[DestY * MipSizeX + DestX];
				if(DestCoverage == 0)
				{
					float AccumulatedColor = 0;
					uint32 Coverage = 0;

					const int32 MinSourceY = FMath::Max((int32)DestY - 1, (int32)0);
					const int32 MaxSourceY = FMath::Min((int32)DestY + 1, (int32)MipSizeY - 1);
					for(int32 SourceY = MinSourceY; SourceY <= MaxSourceY; SourceY++)
					{
						const int32 MinSourceX = FMath::Max((int32)DestX - 1, (int32)0);
						const int32 MaxSourceX = FMath::Min((int32)DestX + 1, (int32)MipSizeX - 1);
						for(int32 SourceX = MinSourceX; SourceX <= MaxSourceX; SourceX++)
						{
							uint8& SourceColor = MipLevelData[SourceY * MipSizeX + SourceX];
							int8 SourceCoverage = MipLevelCoverageData[SourceY * MipSizeX + SourceX];
							if( SourceCoverage > 0 )
							{
								static const uint32 Weights[3][3] =
								{
									{ 1, 255, 1 },
									{ 255, 0, 255 },
									{ 1, 255, 1 },
								};
								AccumulatedColor += SourceColor / 255.0f * SourceCoverage * Weights[SourceX - DestX + 1][SourceY - DestY + 1];
								Coverage += SourceCoverage * Weights[SourceX - DestX + 1][SourceY - DestY + 1];
							}
						}
					}

					if(Coverage)
					{
						DestColor = (uint8)FMath::Clamp<int32>(FMath::TruncToInt(AccumulatedColor / Coverage * 255.f), 0, 255);
						DestCoverage = -1;
					}
				}
			}
		}
	}

	// Fill zero coverage texels with closest colors using mips
	for(int32 MipIndex = NumMips - 2; MipIndex >= 0; MipIndex--)
	{
		const int32 DstMipSizeX = FMath::Max(1, TextureSizeX >> MipIndex);
		const int32 DstMipSizeY = FMath::Max(1, TextureSizeY >> MipIndex);
		const int32 SrcMipSizeX = FMath::Max(1, TextureSizeX >> (MipIndex + 1));
		const int32 SrcMipSizeY = FMath::Max(1, TextureSizeY >> (MipIndex + 1));

		// Source from higher mip, taking into account which texels are mapped.
		uint8* DstMipData = MipData[MipIndex];
		uint8* SrcMipData = MipData[MipIndex + 1];

		int8* DstMipCoverageData = MipCoverageData[ MipIndex ];
		int8* SrcMipCoverageData = MipCoverageData[ MipIndex + 1 ];

		for(int32 DstY = 0; DstY < DstMipSizeY; DstY++)
		{
			for(int32 DstX = 0; DstX < DstMipSizeX; DstX++)
			{
				const uint32 SrcX = DstX / 2;
				const uint32 SrcY = DstY / 2;

				const uint8& SrcColor = SrcMipData[ SrcY * SrcMipSizeX + SrcX ];
				int8 SrcCoverage = SrcMipCoverageData[ SrcY * SrcMipSizeX + SrcX ];

				uint8& DstColor = DstMipData[ DstY * DstMipSizeX + DstX ];
				int8& DstCoverage = DstMipCoverageData[ DstY * DstMipSizeX + DstX ];

				// Point upsample mip data for zero coverage texels
				// TODO bilinear upsample
				if( SrcCoverage != 0 && DstCoverage == 0 )
				{
					DstColor = SrcColor;
					DstCoverage = SrcCoverage;
				}
			}
		}
	}
}

void FLightMapPendingTexture::CreateUObjects()
{
	check(IsInGameThread());
	++GLightmapCounter;
	if (NeedsSkyOcclusionTexture())
	{
		SkyOcclusionTexture = NewObject<ULightMapTexture2D>(Outer, GetSkyOcclusionTextureName(GLightmapCounter));
	}

	if (NeedsAOMaterialMaskTexture())
	{
		AOMaterialMaskTexture = NewObject<ULightMapTexture2D>(Outer, GetAOMaterialMaskTextureName(GLightmapCounter));
	}

	// Encode and compress the coefficient textures.
	for (uint32 CoefficientIndex = 0; CoefficientIndex < NUM_STORED_LIGHTMAP_COEF; CoefficientIndex += 2)
	{
		Textures[CoefficientIndex] = nullptr;
		// Skip generating simple lightmaps if wanted.
		static const auto CVarSupportLowQualityLightmaps = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportLowQualityLightmaps"));
		const bool bAllowLowQualityLightMaps = (!CVarSupportLowQualityLightmaps) || (CVarSupportLowQualityLightmaps->GetValueOnAnyThread() != 0);

		if ((!bAllowLowQualityLightMaps) && CoefficientIndex >= LQ_LIGHTMAP_COEF_INDEX)
		{
			continue;
		}

		// Create the light-map texture for this coefficient.
		auto Texture = NewObject<ULightMapTexture2D>(Outer, GetLightmapName(GLightmapCounter, CoefficientIndex));
		Textures[CoefficientIndex] = Texture;
	}

	check(bUObjectsCreated == false);
	bUObjectsCreated = true;
}

bool FLightMapPendingTexture::NeedsSkyOcclusionTexture() const
{
	if (bUObjectsCreated)
	{
		return SkyOcclusionTexture != nullptr;
	}

	bool bNeedsSkyOcclusionTexture = false;

	for (int32 AllocationIndex = 0; AllocationIndex < Allocations.Num(); AllocationIndex++)
	{
		auto& Allocation = Allocations[AllocationIndex];

		if (Allocation->bHasSkyShadowing)
		{
			bNeedsSkyOcclusionTexture = true;
			break;
		}
	}
	return bNeedsSkyOcclusionTexture;
}

bool FLightMapPendingTexture::NeedsAOMaterialMaskTexture() const
{
	if (bUObjectsCreated)
	{
		return AOMaterialMaskTexture != nullptr;
	}

	const FLightmassWorldInfoSettings* LightmassWorldSettings = OwningWorld.IsValid() ? &(OwningWorld->GetWorldSettings()->LightmassSettings) : NULL;
	if (LightmassWorldSettings && LightmassWorldSettings->bUseAmbientOcclusion && LightmassWorldSettings->bGenerateAmbientOcclusionMaterialMask)
	{
		return true;
	}

	return false;
}

void FLightMapPendingTexture::StartEncoding(ULevel* Unused, ITextureCompressorModule* UnusedCompressor)
{
	if (!bUObjectsCreated)
	{
		check(IsInGameThread());
		CreateUObjects();
	}

	FColor TextureColor;
	if ( GVisualizeLightmapTextures )
	{
		TextureColor = FColor::MakeRandomColor();
	}

	if (SkyOcclusionTexture != nullptr)
	{
		auto Texture = SkyOcclusionTexture;
		
		Texture->Source.Init2DWithMipChain(GetSizeX(), GetSizeY(), TSF_BGRA8);
		Texture->MipGenSettings = TMGS_LeaveExistingMips;
		int32 NumMips = Texture->Source.GetNumMips();
		Texture->SRGB = false;
		Texture->Filter	= GUseBilinearLightmaps ? TF_Default : TF_Nearest;
		Texture->LODGroup = TEXTUREGROUP_Lightmap;
		Texture->LightmapFlags = ELightMapFlags( LightmapFlags );
		Texture->CompressionNoAlpha = false;
		Texture->CompressionNone = !GCompressLightmaps;

		int32 TextureSizeX = Texture->Source.GetSizeX();
		int32 TextureSizeY = Texture->Source.GetSizeY();

		int32 StartBottom = GetSizeX() * GetSizeY();

		// Lock all mip levels.
		FColor* MipData[MAX_TEXTURE_MIP_COUNT] = {0};
		int8* MipCoverageData[MAX_TEXTURE_MIP_COUNT] = {0};
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			MipData[MipIndex] = (FColor*)Texture->Source.LockMip(MipIndex);

			const int32 MipSizeX = FMath::Max( 1, TextureSizeX >> MipIndex );
			const int32 MipSizeY = FMath::Max( 1, TextureSizeY >> MipIndex );
			MipCoverageData[ MipIndex ] = (int8*)FMemory::Malloc( MipSizeX * MipSizeY );
		}

		// Create the uncompressed top mip-level.
		FColor* TopMipData = MipData[0];
		FMemory::Memzero( TopMipData, TextureSizeX * TextureSizeY * sizeof(FColor) );
		FMemory::Memzero( MipCoverageData[0], TextureSizeX * TextureSizeY );

		FIntRect TextureRect( MAX_int32, MAX_int32, MIN_int32, MIN_int32 );
		for(int32 AllocationIndex = 0;AllocationIndex < Allocations.Num();AllocationIndex++)
		{
			auto& Allocation = Allocations[AllocationIndex];
			// Link the light-map to the texture.
			Allocation->LightMap->SkyOcclusionTexture = Texture;
			
			// Skip encoding of this texture if we were asked not to bother
			if( !Allocation->bSkipEncoding )
			{
				TextureRect.Min.X = FMath::Min<int32>( TextureRect.Min.X, Allocation->OffsetX );
				TextureRect.Min.Y = FMath::Min<int32>( TextureRect.Min.Y, Allocation->OffsetY );
				TextureRect.Max.X = FMath::Max<int32>( TextureRect.Max.X, Allocation->OffsetX + Allocation->MappedRect.Width() );
				TextureRect.Max.Y = FMath::Max<int32>( TextureRect.Max.Y, Allocation->OffsetY + Allocation->MappedRect.Height() );

				// Copy the raw data for this light-map into the raw texture data array.
				for(int32 Y = Allocation->MappedRect.Min.Y; Y < Allocation->MappedRect.Max.Y; ++Y)
				{
					for(int32 X = Allocation->MappedRect.Min.X; X < Allocation->MappedRect.Max.X; ++X)
					{
						const FLightMapCoefficients& SourceCoefficients = Allocation->RawData[Y * Allocation->TotalSizeX + X];

						int32 DestY = Y - Allocation->MappedRect.Min.Y + Allocation->OffsetY;
						int32 DestX = X - Allocation->MappedRect.Min.X + Allocation->OffsetX;

						FColor&	DestColor = TopMipData[DestY * TextureSizeX + DestX];

						DestColor.R = SourceCoefficients.SkyOcclusion[0];
						DestColor.G = SourceCoefficients.SkyOcclusion[1];
						DestColor.B = SourceCoefficients.SkyOcclusion[2];
						DestColor.A = SourceCoefficients.SkyOcclusion[3];

						int8& DestCoverage = MipCoverageData[0][ DestY * TextureSizeX + DestX ];
						DestCoverage = SourceCoefficients.Coverage / 2;
					}
				}
			}
		}

		GenerateLightmapMipsAndDilateColor(NumMips, TextureSizeX, TextureSizeY, TextureColor, MipData, MipCoverageData);

		// Unlock all mip levels.
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			Texture->Source.UnlockMip(MipIndex);
			FMemory::Free( MipCoverageData[ MipIndex ] );
		}

	}
	
	if (AOMaterialMaskTexture != nullptr)
	{
		auto Texture = AOMaterialMaskTexture;
		
		Texture->Source.Init2DWithMipChain(GetSizeX(), GetSizeY(), TSF_G8);
		Texture->MipGenSettings = TMGS_LeaveExistingMips;
		int32 NumMips = Texture->Source.GetNumMips();
		Texture->SRGB = false;
		Texture->Filter	= GUseBilinearLightmaps ? TF_Default : TF_Nearest;
		Texture->LODGroup = TEXTUREGROUP_Lightmap;
		Texture->LightmapFlags = ELightMapFlags( LightmapFlags );
		Texture->CompressionNoAlpha = false;
		Texture->CompressionNone = !GCompressLightmaps;
		// BC4
		Texture->CompressionSettings = TC_Alpha;

		int32 TextureSizeX = Texture->Source.GetSizeX();
		int32 TextureSizeY = Texture->Source.GetSizeY();

		int32 StartBottom = GetSizeX() * GetSizeY();

		// Lock all mip levels.
		uint8* MipData[MAX_TEXTURE_MIP_COUNT] = {0};
		int8* MipCoverageData[MAX_TEXTURE_MIP_COUNT] = {0};
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			MipData[MipIndex] = (uint8*)Texture->Source.LockMip(MipIndex);

			const int32 MipSizeX = FMath::Max( 1, TextureSizeX >> MipIndex );
			const int32 MipSizeY = FMath::Max( 1, TextureSizeY >> MipIndex );
			MipCoverageData[ MipIndex ] = (int8*)FMemory::Malloc( MipSizeX * MipSizeY );
		}

		// Create the uncompressed top mip-level.
		uint8* TopMipData = MipData[0];
		FMemory::Memzero( TopMipData, TextureSizeX * TextureSizeY * sizeof(uint8) );
		FMemory::Memzero( MipCoverageData[0], TextureSizeX * TextureSizeY );

		FIntRect TextureRect( MAX_int32, MAX_int32, MIN_int32, MIN_int32 );
		for(int32 AllocationIndex = 0;AllocationIndex < Allocations.Num();AllocationIndex++)
		{
			auto& Allocation = Allocations[AllocationIndex];
			// Link the light-map to the texture.
			Allocation->LightMap->AOMaterialMaskTexture = Texture;
			
			// Skip encoding of this texture if we were asked not to bother
			if( !Allocation->bSkipEncoding )
			{
				TextureRect.Min.X = FMath::Min<int32>( TextureRect.Min.X, Allocation->OffsetX );
				TextureRect.Min.Y = FMath::Min<int32>( TextureRect.Min.Y, Allocation->OffsetY );
				TextureRect.Max.X = FMath::Max<int32>( TextureRect.Max.X, Allocation->OffsetX + Allocation->MappedRect.Width() );
				TextureRect.Max.Y = FMath::Max<int32>( TextureRect.Max.Y, Allocation->OffsetY + Allocation->MappedRect.Height() );

				// Copy the raw data for this light-map into the raw texture data array.
				for(int32 Y = Allocation->MappedRect.Min.Y; Y < Allocation->MappedRect.Max.Y; ++Y)
				{
					for(int32 X = Allocation->MappedRect.Min.X; X < Allocation->MappedRect.Max.X; ++X)
					{
						const FLightMapCoefficients& SourceCoefficients = Allocation->RawData[Y * Allocation->TotalSizeX + X];

						int32 DestY = Y - Allocation->MappedRect.Min.Y + Allocation->OffsetY;
						int32 DestX = X - Allocation->MappedRect.Min.X + Allocation->OffsetX;

						uint8& DestValue = TopMipData[DestY * TextureSizeX + DestX];

						DestValue = SourceCoefficients.AOMaterialMask;

						int8& DestCoverage = MipCoverageData[0][ DestY * TextureSizeX + DestX ];
						DestCoverage = SourceCoefficients.Coverage / 2;
					}
				}
			}
		}

		GenerateLightmapMipsAndDilateByte(NumMips, TextureSizeX, TextureSizeY, TextureColor.R, MipData, MipCoverageData);

		// Unlock all mip levels.
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			Texture->Source.UnlockMip(MipIndex);
			FMemory::Free( MipCoverageData[ MipIndex ] );
		}

	}

	// Encode and compress the coefficient textures.
	for(uint32 CoefficientIndex = 0; CoefficientIndex < NUM_STORED_LIGHTMAP_COEF; CoefficientIndex += 2)
	{
		auto Texture = Textures[CoefficientIndex];
		if (Texture == nullptr)
		{
			continue;
		}

		Texture->Source.Init2DWithMipChain(GetSizeX(), GetSizeY() * 2, TSF_BGRA8);	// Top/bottom atlased
		Texture->MipGenSettings = TMGS_LeaveExistingMips;
		int32 NumMips = Texture->Source.GetNumMips();
		check(NumMips > 0);
		Texture->SRGB = 0;
		Texture->Filter	= GUseBilinearLightmaps ? TF_Default : TF_Nearest;
		Texture->LODGroup = TEXTUREGROUP_Lightmap;
		Texture->LightmapFlags = ELightMapFlags( LightmapFlags );
		Texture->CompressionNoAlpha = CoefficientIndex >= LQ_LIGHTMAP_COEF_INDEX;
		Texture->CompressionNone = !GCompressLightmaps;
		Texture->bForcePVRTC4 = true;

		int32 TextureSizeX = Texture->Source.GetSizeX();
		int32 TextureSizeY = Texture->Source.GetSizeY();
		
		int32 StartBottom = GetSizeX() * GetSizeY();
		
		// Lock all mip levels.
		FColor* MipData[MAX_TEXTURE_MIP_COUNT] = {0};
		int8* MipCoverageData[MAX_TEXTURE_MIP_COUNT] = {0};
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			MipData[MipIndex] = (FColor*)Texture->Source.LockMip(MipIndex);

			const int32 MipSizeX = FMath::Max( 1, TextureSizeX >> MipIndex );
			const int32 MipSizeY = FMath::Max( 1, TextureSizeY >> MipIndex );
			MipCoverageData[ MipIndex ] = (int8*)FMemory::Malloc( MipSizeX * MipSizeY );
		}

		// Create the uncompressed top mip-level.
		FColor* TopMipData = MipData[0];
		FMemory::Memzero( TopMipData, TextureSizeX * TextureSizeY * sizeof(FColor) );
		FMemory::Memzero( MipCoverageData[0], TextureSizeX * TextureSizeY );

		
		for(int32 AllocationIndex = 0;AllocationIndex < Allocations.Num();AllocationIndex++)
		{
			auto& Allocation = Allocations[AllocationIndex];
			// Link the light-map to the texture.
			Allocation->LightMap->Textures[ CoefficientIndex / 2 ] = Texture;
			for( int k = 0; k < 2; k++ )
			{
				Allocation->LightMap->ScaleVectors[ CoefficientIndex + k ] = FVector4(
					Allocation->Scale[ CoefficientIndex + k ][0],
					Allocation->Scale[ CoefficientIndex + k ][1],
					Allocation->Scale[ CoefficientIndex + k ][2],
					Allocation->Scale[ CoefficientIndex + k ][3]
					);

				Allocation->LightMap->AddVectors[ CoefficientIndex + k ] = FVector4(
					Allocation->Add[ CoefficientIndex + k ][0],
					Allocation->Add[ CoefficientIndex + k ][1],
					Allocation->Add[ CoefficientIndex + k ][2],
					Allocation->Add[ CoefficientIndex + k ][3]
					);
			}

			// Skip encoding of this texture if we were asked not to bother
			if( !Allocation->bSkipEncoding )
			{
				FIntRect TextureRect(MAX_int32, MAX_int32, MIN_int32, MIN_int32);
				TextureRect.Min.X = FMath::Min<int32>( TextureRect.Min.X, Allocation->OffsetX );
				TextureRect.Min.Y = FMath::Min<int32>( TextureRect.Min.Y, Allocation->OffsetY );
				TextureRect.Max.X = FMath::Max<int32>( TextureRect.Max.X, Allocation->OffsetX + Allocation->MappedRect.Width() );
				TextureRect.Max.Y = FMath::Max<int32>( TextureRect.Max.Y, Allocation->OffsetY + Allocation->MappedRect.Height() );
	
				NumNonPower2Texels += TextureRect.Width() * TextureRect.Height();


				// Copy the raw data for this light-map into the raw texture data array.
				for(int32 Y = Allocation->MappedRect.Min.Y; Y < Allocation->MappedRect.Max.Y; ++Y)
				{
					for(int32 X = Allocation->MappedRect.Min.X; X < Allocation->MappedRect.Max.X; ++X)
					{
						const FLightMapCoefficients& SourceCoefficients = Allocation->RawData[Y * Allocation->TotalSizeX + X];
						
						int32 DestY = Y - Allocation->MappedRect.Min.Y + Allocation->OffsetY;
						int32 DestX = X - Allocation->MappedRect.Min.X + Allocation->OffsetX;

						FColor&	DestColor = TopMipData[DestY * TextureSizeX + DestX];
						int8&	DestCoverage = MipCoverageData[0][ DestY * TextureSizeX + DestX ];

						FColor&	DestBottomColor = TopMipData[ StartBottom + DestX + DestY * TextureSizeX ];
						int8&	DestBottomCoverage = MipCoverageData[0][ StartBottom + DestX + DestY * TextureSizeX ];
						
#if VISUALIZE_PACKING
						if( X == Allocation->MappedRect.Min.X || Y == Allocation->MappedRect.Min.Y ||
							X == Allocation->MappedRect.Max.X-1 || Y == Allocation->MappedRect.Max.Y-1 ||
							X == Allocation->MappedRect.Min.X+1 || Y == Allocation->MappedRect.Min.Y+1 ||
							X == Allocation->MappedRect.Max.X-2 || Y == Allocation->MappedRect.Max.Y-2 )
						{
							DestColor = FColor::Red;
						}
						else
						{
							DestColor = FColor::Green;
						}
#else
						DestColor.R = SourceCoefficients.Coefficients[CoefficientIndex][0];
						DestColor.G = SourceCoefficients.Coefficients[CoefficientIndex][1];
						DestColor.B = SourceCoefficients.Coefficients[CoefficientIndex][2];
						DestColor.A = SourceCoefficients.Coefficients[CoefficientIndex][3];

						DestBottomColor.R = SourceCoefficients.Coefficients[ CoefficientIndex + 1 ][0];
						DestBottomColor.G = SourceCoefficients.Coefficients[ CoefficientIndex + 1 ][1];
						DestBottomColor.B = SourceCoefficients.Coefficients[ CoefficientIndex + 1 ][2];
						DestBottomColor.A = SourceCoefficients.Coefficients[ CoefficientIndex + 1 ][3];

						if ( GVisualizeLightmapTextures )
						{
							DestColor = TextureColor;
						}

						// uint8 -> int8
						DestCoverage = DestBottomCoverage = SourceCoefficients.Coverage / 2;
						if ( SourceCoefficients.Coverage > 0 )
						{
							NumLightmapMappedTexels++;
						}
						else
						{
							NumLightmapUnmappedTexels++;
						}

#if WITH_EDITOR
						if (bTexelDebuggingEnabled)
						{
							int32 PaddedX = X;
							int32 PaddedY = Y;
							if (GLightmassDebugOptions.bPadMappings && (Allocation->PaddingType == LMPT_NormalPadding))
							{
								if (Allocation->TotalSizeX - 2 > 0 && Allocation->TotalSizeY - 2 > 0)
								{
									PaddedX -= 1;
									PaddedY -= 1;
								}
							}

							if (Allocation->bDebug
								&& PaddedX == GCurrentSelectedLightmapSample.LocalX
								&& PaddedY == GCurrentSelectedLightmapSample.LocalY)
							{
								extern FColor GTexelSelectionColor;
								DestColor = GTexelSelectionColor;
							}
						}
#endif
#endif
					}
				}


			}
		}


		GenerateLightmapMipsAndDilateColor(NumMips, TextureSizeX, TextureSizeY, TextureColor, MipData, MipCoverageData);

		// Unlock all mip levels.
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			Texture->Source.UnlockMip(MipIndex);
			FMemory::Free( MipCoverageData[ MipIndex ] );
		}
	}

	bIsFinishedEncoding = true;
}

FName FLightMapPendingTexture::GetLightmapName(int32 TextureIndex, int32 CoefficientIndex)
{
	check(CoefficientIndex >= 0 && CoefficientIndex < NUM_STORED_LIGHTMAP_COEF);
	FString PotentialName = TEXT("");
	UObject* ExistingObject = NULL;
	int32 LightmapIndex = 0;
	// Search for an unused name
	do
	{
		if (CoefficientIndex < NUM_HQ_LIGHTMAP_COEF)
		{
			PotentialName = FString(TEXT("HQ_Lightmap")) + FString::FromInt(LightmapIndex) + TEXT("_") + FString::FromInt(TextureIndex);
		}
		else
		{
			PotentialName = FString(TEXT("LQ_Lightmap")) + TEXT("_") + FString::FromInt(LightmapIndex) + TEXT("_") + FString::FromInt(TextureIndex);
		}
		ExistingObject = FindObject<UObject>(Outer, *PotentialName);
		LightmapIndex++;
	}
	while (ExistingObject != NULL);
	return FName(*PotentialName);
}

FName FLightMapPendingTexture::GetSkyOcclusionTextureName(int32 TextureIndex)
{
	FString PotentialName = TEXT("");
	UObject* ExistingObject = NULL;
	int32 LightmapIndex = 0;
	// Search for an unused name
	do
	{
		PotentialName = FString(TEXT("SkyOcclusion")) + FString::FromInt(LightmapIndex) + TEXT("_") + FString::FromInt(TextureIndex);

		ExistingObject = FindObject<UObject>(Outer, *PotentialName);
		LightmapIndex++;
	}
	while (ExistingObject != NULL);
	return FName(*PotentialName);
}

FName FLightMapPendingTexture::GetAOMaterialMaskTextureName(int32 TextureIndex)
{
	FString PotentialName = TEXT("");
	UObject* ExistingObject = NULL;
	int32 LightmapIndex = 0;
	// Search for an unused name
	do
	{
		PotentialName = FString(TEXT("AOMaterialMask")) + FString::FromInt(LightmapIndex) + TEXT("_") + FString::FromInt(TextureIndex);

		ExistingObject = FindObject<UObject>(Outer, *PotentialName);
		LightmapIndex++;
	}
	while (ExistingObject != NULL);
	return FName(*PotentialName);
}

/** The light-maps which have not yet been encoded into textures. */
static TArray<FLightMapAllocationGroup> PendingLightMaps;
static uint32 PendingLightMapSize = 0;

#endif // WITH_EDITOR

/** If true, update the status when encoding light maps */
bool FLightMap2D::bUpdateStatus = true;

TRefCountPtr<FLightMap2D> FLightMap2D::AllocateLightMap(UObject* LightMapOuter, FQuantizedLightmapData*& SourceQuantizedData, const FBoxSphereBounds& Bounds, ELightMapPaddingType InPaddingType, ELightMapFlags InLightmapFlags)
{
	// If the light-map has no lights in it, return NULL.
	if (!SourceQuantizedData)
	{
		return NULL;
	}

#if WITH_EDITOR
	FLightMapAllocationGroup AllocationGroup;
	AllocationGroup.Outer = LightMapOuter;
	AllocationGroup.LightmapFlags = InLightmapFlags;
	AllocationGroup.Bounds = Bounds;
	if (!GAllowStreamingLightmaps)
	{
		AllocationGroup.LightmapFlags = ELightMapFlags(AllocationGroup.LightmapFlags & ~LMF_Streamed);
	}

	// Create a new light-map.
	TRefCountPtr<FLightMap2D> LightMap = TRefCountPtr<FLightMap2D>(new FLightMap2D(SourceQuantizedData->LightGuids));

	// Create allocation and add it to the group
	{
		TUniquePtr<FLightMapAllocation> Allocation = MakeUnique<FLightMapAllocation>(MoveTemp(*SourceQuantizedData));
		Allocation->PaddingType = InPaddingType;
		Allocation->LightMap = LightMap;

#if WITH_EDITOR
		if (IsTexelDebuggingEnabled())
		{
			// Detect if this allocation belongs to the texture mapping that was being debugged
			//@todo - this only works for mappings that can be uniquely identified by a single component, BSP for example does not work.
			if (GCurrentSelectedLightmapSample.Component && GCurrentSelectedLightmapSample.Component == LightMapOuter)
			{
				GCurrentSelectedLightmapSample.Lightmap = LightMap;
				Allocation->bDebug = true;
			}
			else
			{
				Allocation->bDebug = false;
			}
		}
#endif

		// SourceQuantizedData is no longer needed now that FLightMapAllocation has what it needs
		delete SourceQuantizedData;
		SourceQuantizedData = NULL;

		// Track the size of pending light-maps.
		PendingLightMapSize += ((Allocation->TotalSizeX + 3) & ~3) * ((Allocation->TotalSizeY + 3) & ~3);

		AllocationGroup.Allocations.Add(MoveTemp(Allocation));
	}

	PendingLightMaps.Add(MoveTemp(AllocationGroup));

	return LightMap;
#else
	return NULL;
#endif // WITH_EDITOR
}

TRefCountPtr<FLightMap2D> FLightMap2D::AllocateInstancedLightMap(UObject* LightMapOuter, UInstancedStaticMeshComponent* Component, TArray<TUniquePtr<FQuantizedLightmapData>> InstancedSourceQuantizedData,
	UMapBuildDataRegistry* Registry, FGuid MapBuildDataId, const FBoxSphereBounds& Bounds, ELightMapPaddingType InPaddingType, ELightMapFlags InLightmapFlags)
{
#if WITH_EDITOR
	check(InstancedSourceQuantizedData.Num() > 0);

	// Verify all instance lightmaps are the same size
	for (int32 InstanceIndex = 1; InstanceIndex < InstancedSourceQuantizedData.Num(); ++InstanceIndex)
	{
		auto& SourceQuantizedData = InstancedSourceQuantizedData[InstanceIndex];
		check(SourceQuantizedData->SizeX == InstancedSourceQuantizedData[0]->SizeX);
		check(SourceQuantizedData->SizeY == InstancedSourceQuantizedData[0]->SizeY);
	}

	// Requantize source data to the same quantization
	// Most of the following code is cloned from UModel::ApplyStaticLighting(), possibly it can be shared in future?
	// or removed, if instanced mesh components can be given per-instance lightmap unpack coefficients
	float MinCoefficient[NUM_STORED_LIGHTMAP_COEF][4];
	float MaxCoefficient[NUM_STORED_LIGHTMAP_COEF][4];
	for (int32 CoefficientIndex = 0; CoefficientIndex < NUM_STORED_LIGHTMAP_COEF; CoefficientIndex += 2)
	{
		for (int32 ColorIndex = 0; ColorIndex < 4; ColorIndex++)
		{
			// Color
			MinCoefficient[CoefficientIndex][ColorIndex] = FLT_MAX;
			MaxCoefficient[CoefficientIndex][ColorIndex] = 0.0f;

			// Direction
			MinCoefficient[CoefficientIndex + 1][ColorIndex] = FLT_MAX;
			MaxCoefficient[CoefficientIndex + 1][ColorIndex] = -FLT_MAX;
		}
	}

	// first, we need to find the max scale for all mappings, and that will be the scale across all instances of this component
	for (auto& SourceQuantizedData : InstancedSourceQuantizedData)
	{
		for (int32 CoefficientIndex = 0; CoefficientIndex < NUM_STORED_LIGHTMAP_COEF; CoefficientIndex++)
		{
			for (int32 ColorIndex = 0; ColorIndex < 4; ColorIndex++)
			{
				// The lightmap data for directional coefficients was packed in lightmass with
				// Pack: y = (x - Min) / (Max - Min)
				// We need to solve for Max and Min in order to combine BSP mappings into a lighting group.
				// QuantizedData->Scale and QuantizedData->Add were calculated in lightmass in order to unpack the lightmap data like so
				// Unpack: x = y * UnpackScale + UnpackAdd
				// Which means
				// Scale = Max - Min
				// Add = Min
				// Therefore we can solve for min and max using substitution

				float Scale = SourceQuantizedData->Scale[CoefficientIndex][ColorIndex];
				float Add = SourceQuantizedData->Add[CoefficientIndex][ColorIndex];
				float Min = Add;
				float Max = Scale + Add;

				MinCoefficient[CoefficientIndex][ColorIndex] = FMath::Min(MinCoefficient[CoefficientIndex][ColorIndex], Min);
				MaxCoefficient[CoefficientIndex][ColorIndex] = FMath::Max(MaxCoefficient[CoefficientIndex][ColorIndex], Max);
			}
		}
	}

	// Now calculate the new unpack scale and add based on the composite min and max
	float Scale[NUM_STORED_LIGHTMAP_COEF][4];
	float Add[NUM_STORED_LIGHTMAP_COEF][4];
	for (int32 CoefficientIndex = 0; CoefficientIndex < NUM_STORED_LIGHTMAP_COEF; CoefficientIndex++)
	{
		for (int32 ColorIndex = 0; ColorIndex < 4; ColorIndex++)
		{
			Scale[CoefficientIndex][ColorIndex] = FMath::Max(MaxCoefficient[CoefficientIndex][ColorIndex] - MinCoefficient[CoefficientIndex][ColorIndex], DELTA);
			Add[CoefficientIndex][ColorIndex] = MinCoefficient[CoefficientIndex][ColorIndex];
		}
	}

	// perform requantization
	for (auto& SourceQuantizedData : InstancedSourceQuantizedData)
	{
		for (uint32 Y = 0; Y < SourceQuantizedData->SizeY; Y++)
		{
			for (uint32 X = 0; X < SourceQuantizedData->SizeX; X++)
			{
				// get source from input, dest from the rectangular offset in the group
				FLightMapCoefficients& LightmapSample = SourceQuantizedData->Data[Y * SourceQuantizedData->SizeX + X];

				// Treat alpha special because of residual
				{
					// Decode LogL
					float LogL = (float)LightmapSample.Coefficients[0][3] / 255.0f;
					float Residual = (float)LightmapSample.Coefficients[1][3] / 255.0f;
					LogL += (Residual - 0.5f) / 255.0f;
					LogL = LogL * SourceQuantizedData->Scale[0][3] + SourceQuantizedData->Add[0][3];

					// Encode LogL
					LogL = (LogL - Add[0][3]) / Scale[0][3];
					Residual = LogL * 255.0f - FMath::RoundToFloat(LogL * 255.0f) + 0.5f;

					LightmapSample.Coefficients[0][3] = (uint8)FMath::Clamp<int32>(FMath::RoundToInt(LogL * 255.0f), 0, 255);
					LightmapSample.Coefficients[1][3] = (uint8)FMath::Clamp<int32>(FMath::RoundToInt(Residual * 255.0f), 0, 255);
				}

				// go over each color coefficient and dequantize and requantize with new Scale/Add
				for (int32 CoefficientIndex = 0; CoefficientIndex < NUM_STORED_LIGHTMAP_COEF; CoefficientIndex++)
				{
					// Don't touch alpha here
					for (int32 ColorIndex = 0; ColorIndex < 3; ColorIndex++)
					{
						// dequantize it
						float Dequantized = (float)LightmapSample.Coefficients[CoefficientIndex][ColorIndex] / 255.0f;
						const float Exponent = CoefficientIndex == 0 ? 2.0f : 1.0f;
						Dequantized = FMath::Pow(Dequantized, Exponent);

						const float Unpacked = Dequantized * SourceQuantizedData->Scale[CoefficientIndex][ColorIndex] + SourceQuantizedData->Add[CoefficientIndex][ColorIndex];
						const float Repacked = (Unpacked - Add[CoefficientIndex][ColorIndex]) / Scale[CoefficientIndex][ColorIndex];

						// requantize it
						LightmapSample.Coefficients[CoefficientIndex][ColorIndex] = (uint8)FMath::Clamp<int32>(FMath::RoundToInt(FMath::Pow(Repacked, 1.0f / Exponent) * 255.0f), 0, 255);
					}
				}
			}
		}

		// Save new requantized Scale/Add
		FMemory::Memcpy(SourceQuantizedData->Scale, Scale, sizeof(Scale));
		FMemory::Memcpy(SourceQuantizedData->Add, Add, sizeof(Add));
	}

	FLightMapAllocationGroup AllocationGroup = FLightMapAllocationGroup();
	AllocationGroup.Outer = LightMapOuter;
	AllocationGroup.LightmapFlags = InLightmapFlags;
	AllocationGroup.Bounds = Bounds;
	if (!GAllowStreamingLightmaps)
	{
		AllocationGroup.LightmapFlags = ELightMapFlags(AllocationGroup.LightmapFlags & ~LMF_Streamed);
	}

	TRefCountPtr<FLightMap2D> BaseLightmap = nullptr;

	for (int32 InstanceIndex = 0; InstanceIndex < InstancedSourceQuantizedData.Num(); ++InstanceIndex)
	{
		auto& SourceQuantizedData = InstancedSourceQuantizedData[InstanceIndex];

		// Create a new light-map.
		TRefCountPtr<FLightMap2D> LightMap = TRefCountPtr<FLightMap2D>(new FLightMap2D(SourceQuantizedData->LightGuids));

		if (InstanceIndex == 0)
		{
			BaseLightmap = LightMap;
		}
		else
		{
			// we need the base lightmap to contain all of the lights used by all lightmaps in the group
			for (auto& LightGuid : SourceQuantizedData->LightGuids)
			{
				BaseLightmap->LightGuids.AddUnique(LightGuid);
			}
		}

		TUniquePtr<FLightMapAllocation> Allocation = MakeUnique<FLightMapAllocation>(MoveTemp(*SourceQuantizedData));
		Allocation->PaddingType = InPaddingType;
		Allocation->LightMap = MoveTemp(LightMap);
		Allocation->Primitive = Component;
		Allocation->Registry = Registry;
		Allocation->MapBuildDataId = MapBuildDataId;
		Allocation->InstanceIndex = InstanceIndex;

		// SourceQuantizedData is no longer needed now that FLightMapAllocation has what it needs
		SourceQuantizedData.Reset();

		// Track the size of pending light-maps.
		PendingLightMapSize += ((Allocation->TotalSizeX + 3) & ~3) * ((Allocation->TotalSizeY + 3) & ~3);

		AllocationGroup.Allocations.Add(MoveTemp(Allocation));
	}

	PendingLightMaps.Add(MoveTemp(AllocationGroup));

	return BaseLightmap;
#else
	return nullptr;
#endif //WITH_EDITOR
}

#if WITH_EDITOR

struct FCompareLightmaps
{
	FORCEINLINE bool operator()(const FLightMapAllocationGroup& A, const FLightMapAllocationGroup& B) const
	{
		// Order descending by total size of allocation
		return A.TotalTexels > B.TotalTexels;
	}
};

#endif //WITH_EDITOR

/**
 * Executes all pending light-map encoding requests.
 * @param	bLightingSuccessful	Whether the lighting build was successful or not.
 * @param	bMultithreadedEncode encode textures on different threads ;)
 */
void FLightMap2D::EncodeTextures( UWorld* InWorld, bool bLightingSuccessful, bool bMultithreadedEncode)
{
#if WITH_EDITOR
	if (bLightingSuccessful)
	{
		GWarn->BeginSlowTask( NSLOCTEXT("LightMap2D", "BeginEncodingLightMapsTask", "Encoding light-maps"), false );
		int32 PackedLightAndShadowMapTextureSize = InWorld->GetWorldSettings()->PackedLightAndShadowMapTextureSize;

		// Reset the pending light-map size.
		PendingLightMapSize = 0;

		// Crop lightmaps if allowed
		if (GAllowLightmapCropping)
		{
			for (FLightMapAllocationGroup& PendingGroup : PendingLightMaps)
			{
				if (!ensure(PendingGroup.Allocations.Num() >= 1))
				{
					continue;
				}

				// TODO: Crop all allocations in a group to the same size for instanced meshes
				if (PendingGroup.Allocations.Num() == 1)
				{
					for (auto& Allocation : PendingGroup.Allocations)
					{
						CropUnmappedTexels(Allocation->RawData, Allocation->TotalSizeX, Allocation->TotalSizeY, Allocation->MappedRect);
					}
				}
			}
		}

		// Calculate size of pending allocations for sorting
		for (FLightMapAllocationGroup& PendingGroup : PendingLightMaps)
		{
			PendingGroup.TotalTexels = 0;
			for (auto& Allocation : PendingGroup.Allocations)
			{
				// Assumes bAlignByFour
				PendingGroup.TotalTexels += ((Allocation->MappedRect.Width() + 3) & ~3) * ((Allocation->MappedRect.Height() + 3) & ~3);
			}
		}

		// Sort the light-maps in descending order by size.
		Sort(PendingLightMaps.GetData(), PendingLightMaps.Num(), FCompareLightmaps());

		// Allocate texture space for each light-map.
		TArray<FLightMapPendingTexture*> PendingTextures;

		for (FLightMapAllocationGroup& PendingGroup : PendingLightMaps)
		{
			if (!ensure(PendingGroup.Allocations.Num() >= 1))
			{
				continue;
			}

			int32 MaxWidth = 0;
			int32 MaxHeight = 0;
			for (auto& Allocation : PendingGroup.Allocations)
			{
				MaxWidth = FMath::Max(MaxWidth, Allocation->MappedRect.Width());
				MaxHeight = FMath::Max(MaxHeight, Allocation->MappedRect.Height());
			}

			FLightMapPendingTexture* Texture = nullptr;

			// Find an existing texture which the light-map can be stored in.
			// Lightmaps will always be 4-pixel aligned...
			for (FLightMapPendingTexture* ExistingTexture : PendingTextures)
			{
				if (ExistingTexture->AddElement(PendingGroup))
				{
					Texture = ExistingTexture;
					break;
				}
			}

			if (!Texture)
			{
				int32 NewTextureSizeX = PackedLightAndShadowMapTextureSize;
				int32 NewTextureSizeY = PackedLightAndShadowMapTextureSize / 2;

				// Assumes identically-sized allocations, fit into the smallest 2x1 rectangle
				const int32 AllocationCountX = FMath::CeilToInt(FMath::Sqrt(FMath::DivideAndRoundUp(PendingGroup.Allocations.Num() * 2 * MaxHeight, MaxWidth)));
				const int32 AllocationCountY = FMath::DivideAndRoundUp(PendingGroup.Allocations.Num(), AllocationCountX);
				const int32 AllocationSizeX = AllocationCountX * MaxWidth;
				const int32 AllocationSizeY = AllocationCountY * MaxHeight;

				if (AllocationSizeX > NewTextureSizeX || AllocationSizeY > NewTextureSizeY)
				{
					NewTextureSizeX = FMath::RoundUpToPowerOfTwo(AllocationSizeX);
					NewTextureSizeY = FMath::RoundUpToPowerOfTwo(AllocationSizeY);

					// Force 2:1 aspect
					NewTextureSizeX = FMath::Max(NewTextureSizeX, NewTextureSizeY * 2);
					NewTextureSizeY = FMath::Max(NewTextureSizeY, NewTextureSizeX / 2);
				}

				// If there is no existing appropriate texture, create a new one.
				Texture = new FLightMapPendingTexture(InWorld, NewTextureSizeX, NewTextureSizeY);
				PendingTextures.Add(Texture);
				Texture->Outer = PendingGroup.Outer;
				Texture->Bounds = PendingGroup.Bounds;
				Texture->LightmapFlags = PendingGroup.LightmapFlags;
				verify(Texture->AddElement(PendingGroup));
			}

			// Give the texture ownership of the allocations
			for (auto& Allocation : PendingGroup.Allocations)
			{
				Texture->Allocations.Add(MoveTemp(Allocation));
			}
		}
		PendingLightMaps.Empty();
		if (bMultithreadedEncode)
		{
			FThreadSafeCounter Counter(PendingTextures.Num());
			// allocate memory for all the async encode tasks
			TArray<FAsyncEncode<FLightMapPendingTexture>> AsyncEncodeTasks;
			AsyncEncodeTasks.Empty(PendingTextures.Num());
			for (auto& Texture :PendingTextures)
			{
				// precreate the UObjects then give them to some threads to process
				// need to precreate Uobjects 
				Texture->CreateUObjects();
				auto AsyncEncodeTask = new (AsyncEncodeTasks)FAsyncEncode<FLightMapPendingTexture>(Texture,nullptr,Counter,nullptr);
				GLargeThreadPool->AddQueuedWork(AsyncEncodeTask);
			}

			while (Counter.GetValue() > 0)
			{
				GWarn->UpdateProgress(Counter.GetValue(), PendingTextures.Num());
				FPlatformProcess::Sleep(0.0001f);
			}
		}
		else
		{
			// Encode all the pending textures.
			for (int32 TextureIndex = 0; TextureIndex < PendingTextures.Num(); TextureIndex++)
			{
				if (bUpdateStatus && (TextureIndex % 20) == 0)
				{
					GWarn->UpdateProgress(TextureIndex, PendingTextures.Num());
				}
				FLightMapPendingTexture* PendingTexture = PendingTextures[TextureIndex];
				PendingTexture->StartEncoding(nullptr,nullptr);
			}
		}

		// finish the encode (separate from waiting for the cache to complete)
		while (true)
		{
			bool bIsFinishedPostEncode = true;
			for (auto& PendingTexture : PendingTextures)
			{
				if (PendingTexture->IsFinishedEncoding())
				{
					PendingTexture->PostEncode();
				}
				else
				{
					// call post encode in order
					bIsFinishedPostEncode = false;
					break;
				}
			}
			if (bIsFinishedPostEncode)
				break;
		}

		for (auto& PendingTexture : PendingTextures)
		{
			PendingTexture->FinishCachingTextures();
			delete PendingTexture;
		}

		PendingTextures.Empty();


		// End the encoding lighmaps slow task
		GWarn->EndSlowTask();
		
	}
	else
	{
		PendingLightMaps.Empty();
	}
#endif //WITH_EDITOR
}

FLightMap2D::FLightMap2D()
{
	Textures[0] = NULL;
	Textures[1] = NULL;
	SkyOcclusionTexture = NULL;
	AOMaterialMaskTexture = NULL;
}

FLightMap2D::FLightMap2D(const TArray<FGuid>& InLightGuids)
{
	LightGuids = InLightGuids;
	Textures[0] = NULL;
	Textures[1] = NULL;
	SkyOcclusionTexture = NULL;
	AOMaterialMaskTexture = NULL;
}

const UTexture2D* FLightMap2D::GetTexture(uint32 BasisIndex) const
{
	check(IsValid(BasisIndex));
	return Textures[BasisIndex];
}

UTexture2D* FLightMap2D::GetTexture(uint32 BasisIndex)
{
	check(IsValid(BasisIndex));
	return Textures[BasisIndex];
}

UTexture2D* FLightMap2D::GetSkyOcclusionTexture() const
{
	return SkyOcclusionTexture;
}

UTexture2D* FLightMap2D::GetAOMaterialMaskTexture() const
{
	return AOMaterialMaskTexture;
}

/**
 * Returns whether the specified basis has a valid lightmap texture or not.
 * @param	BasisIndex - The basis index.
 * @return	true if the specified basis has a valid lightmap texture, otherwise false
 */
bool FLightMap2D::IsValid(uint32 BasisIndex) const
{
	return Textures[BasisIndex] != nullptr;
}

struct FLegacyLightMapTextureInfo
{
	ULightMapTexture2D* Texture;
	FLinearColor Scale;
	FLinearColor Bias;

	friend FArchive& operator<<(FArchive& Ar,FLegacyLightMapTextureInfo& I)
	{
		return Ar << I.Texture << I.Scale << I.Bias;
	}
};

void FLightMap2D::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(Textures[0]);
	Collector.AddReferencedObject(Textures[1]);
	Collector.AddReferencedObject(SkyOcclusionTexture);
	Collector.AddReferencedObject(AOMaterialMaskTexture);
}

void FLightMap2D::Serialize(FArchive& Ar)
{
	FLightMap::Serialize(Ar);

	if( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_LOW_QUALITY_DIRECTIONAL_LIGHTMAPS )
	{
		for(uint32 CoefficientIndex = 0;CoefficientIndex < 3;CoefficientIndex++)
		{
			ULightMapTexture2D* Dummy = NULL;
			Ar << Dummy;
			FVector4 Dummy2;
			Ar << Dummy2;
			Ar << Dummy2;
		}
	}
	else if( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_COMBINED_LIGHTMAP_TEXTURES )
	{
		for( uint32 CoefficientIndex = 0; CoefficientIndex < 4; CoefficientIndex++ )
		{
			ULightMapTexture2D* Dummy = NULL;
			Ar << Dummy;
			FVector4 Dummy2;
			Ar << Dummy2;
			Ar << Dummy2;
		}
	}
	else
	{
		if (Ar.IsCooking())
		{
			bool bStripLQLightmaps = !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::LowQualityLightmaps);
			bool bStripHQLightmaps = !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::HighQualityLightmaps);

			ULightMapTexture2D* Dummy = NULL;
			ULightMapTexture2D*& Texture1 = bStripHQLightmaps ? Dummy : Textures[0];
			ULightMapTexture2D*& Texture2 = bStripLQLightmaps ? Dummy : Textures[1];
			Ar << Texture1;
			Ar << Texture2;
		}
		else
		{
			Ar << Textures[0];
			Ar << Textures[1];
		}

		if (Ar.UE4Ver() >= VER_UE4_SKY_LIGHT_COMPONENT)
		{
			if (Ar.IsCooking())
			{
				bool bStripHQLightmaps = !Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::HighQualityLightmaps);

				ULightMapTexture2D* Dummy = NULL;
				ULightMapTexture2D*& SkyTexture = bStripHQLightmaps ? Dummy : SkyOcclusionTexture;
				Ar << SkyTexture;

				if (Ar.UE4Ver() >= VER_UE4_AO_MATERIAL_MASK)
				{
					ULightMapTexture2D*& MaskTexture = bStripHQLightmaps ? Dummy : AOMaterialMaskTexture;
					Ar << MaskTexture;
				}
			}
			else
			{
				Ar << SkyOcclusionTexture;

				if (Ar.UE4Ver() >= VER_UE4_AO_MATERIAL_MASK)
				{
					Ar << AOMaterialMaskTexture;
				}
			}
		}
		
		for(uint32 CoefficientIndex = 0;CoefficientIndex < NUM_STORED_LIGHTMAP_COEF;CoefficientIndex++)
		{
			Ar << ScaleVectors[CoefficientIndex];
			Ar << AddVectors[CoefficientIndex];
		}
	}

	Ar << CoordinateScale << CoordinateBias;
	
	// Force no divide by zeros even with low precision. This should be fixed during build but for some reason isn't.
	if( Ar.IsLoading() )
	{
		for( int k = 0; k < 3; k++ )
		{
			ScaleVectors[2][k] = FMath::Max( 0.0f, ScaleVectors[2][k] );
			AddVectors[2][k] = FMath::Max( 0.01f, AddVectors[2][k] );
		}
	}

	//Release unneeded texture references on load so they will be garbage collected.
	//In the editor we need to keep these references since they will need to be saved.
	if (Ar.IsLoading() && !GIsEditor)
	{
		Textures[ bAllowHighQualityLightMaps ? 1 : 0 ] = NULL;

		if (!bAllowHighQualityLightMaps)
		{
			SkyOcclusionTexture = NULL;
			AOMaterialMaskTexture = NULL;
		}
	}
}

FLightMapInteraction FLightMap2D::GetInteraction(ERHIFeatureLevel::Type InFeatureLevel) const
{
	bool bHighQuality = AllowHighQualityLightmaps(InFeatureLevel);

	int32 LightmapIndex = bHighQuality ? 0 : 1;

	bool bValidTextures = Textures[ LightmapIndex ] && Textures[ LightmapIndex ]->Resource;

	// When the FLightMap2D is first created, the textures aren't set, so that case needs to be handled.
	if(bValidTextures)
	{
		return FLightMapInteraction::Texture(Textures, SkyOcclusionTexture, AOMaterialMaskTexture, ScaleVectors, AddVectors, CoordinateScale, CoordinateBias, bHighQuality);
	}

	return FLightMapInteraction::None();
}

void FLegacyLightMap1D::Serialize(FArchive& Ar)
{
	FLightMap::Serialize(Ar);

	check(Ar.IsLoading());

	UObject* Owner;
	TQuantizedLightSampleBulkData<FQuantizedDirectionalLightSample> DirectionalSamples;
	TQuantizedLightSampleBulkData<FQuantizedSimpleLightSample> SimpleSamples;

	Ar << Owner;

	DirectionalSamples.Serialize( Ar, Owner );

	for (int32 ElementIndex = 0; ElementIndex < 5; ElementIndex++)
	{
		FVector Dummy;
		Ar << Dummy;
	}

	SimpleSamples.Serialize( Ar, Owner );
}

/*-----------------------------------------------------------------------------
	FQuantizedLightSample version of bulk data.
-----------------------------------------------------------------------------*/

/**
 * Returns whether single element serialization is required given an archive. This e.g.
 * can be the case if the serialization for an element changes and the single element
 * serialization code handles backward compatibility.
 */
template<class QuantizedLightSampleType>
bool TQuantizedLightSampleBulkData<QuantizedLightSampleType>::RequiresSingleElementSerialization( FArchive& Ar )
{
	return false;
}

/**
 * Returns size in bytes of single element.
 *
 * @return Size in bytes of single element
 */
template<class QuantizedLightSampleType>
int32 TQuantizedLightSampleBulkData<QuantizedLightSampleType>::GetElementSize() const
{
	return sizeof(QuantizedLightSampleType);
}

/**
 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
 * 
 * @param Ar			Archive to serialize with
 * @param Data			Base pointer to data
 * @param ElementIndex	Element index to serialize
 */
template<class QuantizedLightSampleType>
void TQuantizedLightSampleBulkData<QuantizedLightSampleType>::SerializeElement( FArchive& Ar, void* Data, int32 ElementIndex )
{
	QuantizedLightSampleType* QuantizedLightSample = (QuantizedLightSampleType*)Data + ElementIndex;
	// serialize as colors
	const uint32 NumCoefficients = sizeof(QuantizedLightSampleType) / sizeof(FColor);
	for(int32 CoefficientIndex = 0; CoefficientIndex < NumCoefficients; CoefficientIndex++)
	{
		uint32 ColorDWORD = QuantizedLightSample->Coefficients[CoefficientIndex].DWColor();
		Ar << ColorDWORD;
		QuantizedLightSample->Coefficients[CoefficientIndex] = FColor(ColorDWORD);
	} 
};

FArchive& operator<<(FArchive& Ar, FLightMap*& R)
{
	uint32 LightMapType = FLightMap::LMT_None;

	if (Ar.IsSaving())
	{
		if (R != nullptr)
		{
			if (R->GetLightMap2D())
			{
				LightMapType = FLightMap::LMT_2D;
			}
		}
	}

	Ar << LightMapType;

	if (Ar.IsLoading())
	{
		// explicitly don't call "delete R;",
		// we expect the calling code to handle that
		switch (LightMapType)
		{
		case FLightMap::LMT_None:
			R = nullptr;
			break;
		case FLightMap::LMT_1D:
			R = new FLegacyLightMap1D();
			break;
		case FLightMap::LMT_2D:
			R = new FLightMap2D();
			break;
		default:
			check(0);
		}
	}

	if (R != nullptr)
	{
		R->Serialize(Ar);

		if (Ar.IsLoading())
		{
			// Toss legacy vertex lightmaps
			if (LightMapType == FLightMap::LMT_1D)
			{
				delete R;
				R = nullptr;
			}

			// Dump old lightmaps
			if (Ar.UE4Ver() < VER_UE4_COMBINED_LIGHTMAP_TEXTURES)
			{
				delete R; // safe because if we're loading we new'd this above
				R = nullptr;
			}
		}
	}

	return Ar;
}

bool FQuantizedLightmapData::HasNonZeroData() const
{
	// 1D lightmaps don't have a valid coverage amount, so they shouldn't be discarded if the coverage is 0
	uint8 MinCoverageThreshold = (SizeY == 1) ? 0 : 1;

	// Check all of the samples for a non-zero coverage (if valid) and at least one non-zero coefficient
	for (int32 SampleIndex = 0; SampleIndex < Data.Num(); SampleIndex++)
	{
		const FLightMapCoefficients& LightmapSample = Data[SampleIndex];

		if (LightmapSample.Coverage >= MinCoverageThreshold)
		{
			// Don't look at simple lightmap coefficients if we're not building them.
			static const auto CVarSupportLowQualityLightmaps = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportLowQualityLightmaps"));
			const bool bAllowLowQualityLightMaps = (!CVarSupportLowQualityLightmaps) || (CVarSupportLowQualityLightmaps->GetValueOnAnyThread() != 0);
			const int32 NumCoefficients = bAllowLowQualityLightMaps ? NUM_STORED_LIGHTMAP_COEF : NUM_HQ_LIGHTMAP_COEF;

			for (int32 CoefficentIndex = 0; CoefficentIndex < NumCoefficients; CoefficentIndex++)
			{
				if ((LightmapSample.Coefficients[CoefficentIndex][0] != 0) || (LightmapSample.Coefficients[CoefficentIndex][1] != 0) || (LightmapSample.Coefficients[CoefficentIndex][2] != 0))
				{
					return true;
				}
			}

			if (bHasSkyShadowing)
			{
				for (int32 Index = 0; Index < ARRAY_COUNT(LightmapSample.SkyOcclusion); Index++)
				{
					if (LightmapSample.SkyOcclusion[Index] != 0)
					{
						return true;
					}
				}
			}

			if (LightmapSample.AOMaterialMask != 0)
			{
				return true;
			}
		}
	}

	return false;
}
