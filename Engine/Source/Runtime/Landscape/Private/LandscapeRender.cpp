// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LandscapeRender.cpp: New terrain rendering
=============================================================================*/

#include "LandscapeRender.h"
#include "LightMap.h"
#include "ShadowMap.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapePrivate.h"
#include "LandscapeMeshProxyComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionLandscapeLayerCoords.h"
#include "ShaderParameterUtils.h"
#include "TessellationRendering.h"
#include "LandscapeEdit.h"
#include "Engine/LevelStreaming.h"
#include "LevelUtils.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "LandscapeMaterialInstanceConstant.h"
#include "Engine/ShadowMapTexture2D.h"
#include "EngineGlobals.h"
#include "UnrealEngine.h"
#include "LandscapeLight.h"
#include "Containers/Algo/Find.h"
#include "Engine/StaticMesh.h"


IMPLEMENT_UNIFORM_BUFFER_STRUCT(FLandscapeUniformShaderParameters, TEXT("LandscapeParameters"));

#define LANDSCAPE_LOD_DISTANCE_FACTOR 2.f
#define LANDSCAPE_MAX_COMPONENT_SIZE 255
#define LANDSCAPE_LOD_SQUARE_ROOT_FACTOR 1.5f

int32 GLandscapeMeshLODBias = 0;
FAutoConsoleVariableRef CVarLandscapeMeshLODBias(
	TEXT("r.LandscapeLODBias"),
	GLandscapeMeshLODBias,
	TEXT("LOD bias for landscape/terrain meshes."),
	ECVF_Scalability
	);

/*------------------------------------------------------------------------------
	Forsyth algorithm for cache optimizing index buffers.
------------------------------------------------------------------------------*/

// Forsyth algorithm to optimize post-transformed vertex cache
namespace
{
	// code for computing vertex score was taken, as much as possible
	// directly from the original publication.
	float ComputeVertexCacheScore(int32 CachePosition, uint32 VertexCacheSize)
	{
		const float FindVertexScoreCacheDecayPower = 1.5f;
		const float FindVertexScoreLastTriScore = 0.75f;

		float Score = 0.0f;
		if (CachePosition < 0)
		{
			// Vertex is not in FIFO cache - no score.
		}
		else
		{
			if (CachePosition < 3)
			{
				// This vertex was used in the last triangle,
				// so it has a fixed score, whichever of the three
				// it's in. Otherwise, you can get very different
				// answers depending on whether you add
				// the triangle 1,2,3 or 3,1,2 - which is silly.
				Score = FindVertexScoreLastTriScore;
			}
			else
			{
				check(CachePosition < (int32)VertexCacheSize);
				// Points for being high in the cache.
				const float Scaler = 1.0f / (VertexCacheSize - 3);
				Score = 1.0f - (CachePosition - 3) * Scaler;
				Score = FMath::Pow(Score, FindVertexScoreCacheDecayPower);
			}
		}

		return Score;
	}

	float ComputeVertexValenceScore(uint32 numActiveFaces)
	{
		const float FindVertexScoreValenceBoostScale = 2.0f;
		const float FindVertexScoreValenceBoostPower = 0.5f;

		float Score = 0.f;

		// Bonus points for having a low number of tris still to
		// use the vert, so we get rid of lone verts quickly.
		float ValenceBoost = FMath::Pow(float(numActiveFaces), -FindVertexScoreValenceBoostPower);
		Score += FindVertexScoreValenceBoostScale * ValenceBoost;

		return Score;
	}

	const uint32 MaxVertexCacheSize = 64;
	const uint32 MaxPrecomputedVertexValenceScores = 64;
	float VertexCacheScores[MaxVertexCacheSize + 1][MaxVertexCacheSize];
	float VertexValenceScores[MaxPrecomputedVertexValenceScores];
	bool bVertexScoresComputed = false; //ComputeVertexScores();

	bool ComputeVertexScores()
	{
		for (uint32 CacheSize = 0; CacheSize <= MaxVertexCacheSize; ++CacheSize)
		{
			for (uint32 CachePos = 0; CachePos < CacheSize; ++CachePos)
			{
				VertexCacheScores[CacheSize][CachePos] = ComputeVertexCacheScore(CachePos, CacheSize);
			}
		}

		for (uint32 Valence = 0; Valence < MaxPrecomputedVertexValenceScores; ++Valence)
		{
			VertexValenceScores[Valence] = ComputeVertexValenceScore(Valence);
		}

		return true;
	}

	inline float FindVertexCacheScore(uint32 CachePosition, uint32 MaxSizeVertexCache)
	{
		return VertexCacheScores[MaxSizeVertexCache][CachePosition];
	}

	inline float FindVertexValenceScore(uint32 NumActiveTris)
	{
		return VertexValenceScores[NumActiveTris];
	}

	float FindVertexScore(uint32 NumActiveFaces, uint32 CachePosition, uint32 VertexCacheSize)
	{
		check(bVertexScoresComputed);

		if (NumActiveFaces == 0)
		{
			// No tri needs this vertex!
			return -1.0f;
		}

		float Score = 0.f;
		if (CachePosition < VertexCacheSize)
		{
			Score += VertexCacheScores[VertexCacheSize][CachePosition];
		}

		if (NumActiveFaces < MaxPrecomputedVertexValenceScores)
		{
			Score += VertexValenceScores[NumActiveFaces];
		}
		else
		{
			Score += ComputeVertexValenceScore(NumActiveFaces);
		}

		return Score;
	}

	struct OptimizeVertexData
	{
		float  Score;
		uint32  ActiveFaceListStart;
		uint32  ActiveFaceListSize;
		uint32  CachePos0;
		uint32  CachePos1;
		OptimizeVertexData() : Score(0.f), ActiveFaceListStart(0), ActiveFaceListSize(0), CachePos0(0), CachePos1(0) { }
	};

	//-----------------------------------------------------------------------------
	//  OptimizeFaces
	//-----------------------------------------------------------------------------
	//  Parameters:
	//      InIndexList
	//          input index list
	//      OutIndexList
	//          a pointer to a preallocated buffer the same size as indexList to
	//          hold the optimized index list
	//      LRUCacheSize
	//          the size of the simulated post-transform cache (max:64)
	//-----------------------------------------------------------------------------

	template <typename INDEX_TYPE>
	void OptimizeFaces(const TArray<INDEX_TYPE>& InIndexList, TArray<INDEX_TYPE>& OutIndexList, uint16 LRUCacheSize)
	{
		uint32 VertexCount = 0;
		const uint32 IndexCount = InIndexList.Num();

		// compute face count per vertex
		for (uint32 i = 0; i < IndexCount; ++i)
		{
			uint32 Index = InIndexList[i];
			VertexCount = FMath::Max(Index, VertexCount);
		}
		VertexCount++;

		TArray<OptimizeVertexData> VertexDataList;
		VertexDataList.Empty(VertexCount);
		for (uint32 i = 0; i < VertexCount; i++)
		{
			VertexDataList.Add(OptimizeVertexData());
		}

		OutIndexList.Empty(IndexCount);
		OutIndexList.AddZeroed(IndexCount);

		// compute face count per vertex
		for (uint32 i = 0; i < IndexCount; ++i)
		{
			uint32 Index = InIndexList[i];
			OptimizeVertexData& VertexData = VertexDataList[Index];
			VertexData.ActiveFaceListSize++;
		}

		TArray<uint32> ActiveFaceList;

		const uint32 EvictedCacheIndex = TNumericLimits<uint32>::Max();

		{
			// allocate face list per vertex
			uint32 CurActiveFaceListPos = 0;
			for (uint32 i = 0; i < VertexCount; ++i)
			{
				OptimizeVertexData& VertexData = VertexDataList[i];
				VertexData.CachePos0 = EvictedCacheIndex;
				VertexData.CachePos1 = EvictedCacheIndex;
				VertexData.ActiveFaceListStart = CurActiveFaceListPos;
				CurActiveFaceListPos += VertexData.ActiveFaceListSize;
				VertexData.Score = FindVertexScore(VertexData.ActiveFaceListSize, VertexData.CachePos0, LRUCacheSize);
				VertexData.ActiveFaceListSize = 0;
			}
			ActiveFaceList.Empty(CurActiveFaceListPos);
			ActiveFaceList.AddZeroed(CurActiveFaceListPos);
		}

		// fill out face list per vertex
		for (uint32 i = 0; i < IndexCount; i += 3)
		{
			for (uint32 j = 0; j < 3; ++j)
			{
				uint32 Index = InIndexList[i + j];
				OptimizeVertexData& VertexData = VertexDataList[Index];
				ActiveFaceList[VertexData.ActiveFaceListStart + VertexData.ActiveFaceListSize] = i;
				VertexData.ActiveFaceListSize++;
			}
		}

		TArray<uint8> ProcessedFaceList;
		ProcessedFaceList.Empty(IndexCount);
		ProcessedFaceList.AddZeroed(IndexCount);

		uint32 VertexCacheBuffer[(MaxVertexCacheSize + 3) * 2];
		uint32* Cache0 = VertexCacheBuffer;
		uint32* Cache1 = VertexCacheBuffer + (MaxVertexCacheSize + 3);
		uint32 EntriesInCache0 = 0;

		uint32 BestFace = 0;
		float BestScore = -1.f;

		const float MaxValenceScore = FindVertexScore(1, EvictedCacheIndex, LRUCacheSize) * 3.f;

		for (uint32 i = 0; i < IndexCount; i += 3)
		{
			if (BestScore < 0.f)
			{
				// no verts in the cache are used by any unprocessed faces so
				// search all unprocessed faces for a new starting point
				for (uint32 j = 0; j < IndexCount; j += 3)
				{
					if (ProcessedFaceList[j] == 0)
					{
						uint32 Face = j;
						float FaceScore = 0.f;
						for (uint32 k = 0; k < 3; ++k)
						{
							uint32 Index = InIndexList[Face + k];
							OptimizeVertexData& VertexData = VertexDataList[Index];
							check(VertexData.ActiveFaceListSize > 0);
							check(VertexData.CachePos0 >= LRUCacheSize);
							FaceScore += VertexData.Score;
						}

						if (FaceScore > BestScore)
						{
							BestScore = FaceScore;
							BestFace = Face;

							check(BestScore <= MaxValenceScore);
							if (BestScore >= MaxValenceScore)
							{
								break;
							}
						}
					}
				}
				check(BestScore >= 0.f);
			}

			ProcessedFaceList[BestFace] = 1;
			uint32 EntriesInCache1 = 0;

			// add bestFace to LRU cache and to newIndexList
			for (uint32 V = 0; V < 3; ++V)
			{
				INDEX_TYPE Index = InIndexList[BestFace + V];
				OutIndexList[i + V] = Index;

				OptimizeVertexData& VertexData = VertexDataList[Index];

				if (VertexData.CachePos1 >= EntriesInCache1)
				{
					VertexData.CachePos1 = EntriesInCache1;
					Cache1[EntriesInCache1++] = Index;

					if (VertexData.ActiveFaceListSize == 1)
					{
						--VertexData.ActiveFaceListSize;
						continue;
					}
				}

				check(VertexData.ActiveFaceListSize > 0);
				uint32 FindIndex;
				for (FindIndex = VertexData.ActiveFaceListStart; FindIndex < VertexData.ActiveFaceListStart + VertexData.ActiveFaceListSize; FindIndex++)
				{
					if (ActiveFaceList[FindIndex] == BestFace)
					{
						break;
					}
				}
				check(FindIndex != VertexData.ActiveFaceListStart + VertexData.ActiveFaceListSize);

				if (FindIndex != VertexData.ActiveFaceListStart + VertexData.ActiveFaceListSize - 1)
				{
					uint32 SwapTemp = ActiveFaceList[FindIndex];
					ActiveFaceList[FindIndex] = ActiveFaceList[VertexData.ActiveFaceListStart + VertexData.ActiveFaceListSize - 1];
					ActiveFaceList[VertexData.ActiveFaceListStart + VertexData.ActiveFaceListSize - 1] = SwapTemp;
				}

				--VertexData.ActiveFaceListSize;
				VertexData.Score = FindVertexScore(VertexData.ActiveFaceListSize, VertexData.CachePos1, LRUCacheSize);

			}

			// move the rest of the old verts in the cache down and compute their new scores
			for (uint32 C0 = 0; C0 < EntriesInCache0; ++C0)
			{
				uint32 Index = Cache0[C0];
				OptimizeVertexData& VertexData = VertexDataList[Index];

				if (VertexData.CachePos1 >= EntriesInCache1)
				{
					VertexData.CachePos1 = EntriesInCache1;
					Cache1[EntriesInCache1++] = Index;
					VertexData.Score = FindVertexScore(VertexData.ActiveFaceListSize, VertexData.CachePos1, LRUCacheSize);
				}
			}

			// find the best scoring triangle in the current cache (including up to 3 that were just evicted)
			BestScore = -1.f;
			for (uint32 C1 = 0; C1 < EntriesInCache1; ++C1)
			{
				uint32 Index = Cache1[C1];
				OptimizeVertexData& VertexData = VertexDataList[Index];
				VertexData.CachePos0 = VertexData.CachePos1;
				VertexData.CachePos1 = EvictedCacheIndex;
				for (uint32 j = 0; j < VertexData.ActiveFaceListSize; ++j)
				{
					uint32 Face = ActiveFaceList[VertexData.ActiveFaceListStart + j];
					float FaceScore = 0.f;
					for (uint32 V = 0; V < 3; V++)
					{
						uint32 FaceIndex = InIndexList[Face + V];
						OptimizeVertexData& FaceVertexData = VertexDataList[FaceIndex];
						FaceScore += FaceVertexData.Score;
					}
					if (FaceScore > BestScore)
					{
						BestScore = FaceScore;
						BestFace = Face;
					}
				}
			}

			uint32* SwapTemp = Cache0;
			Cache0 = Cache1;
			Cache1 = SwapTemp;

			EntriesInCache0 = FMath::Min(EntriesInCache1, (uint32)LRUCacheSize);
		}
	}

} // namespace 

struct FLandscapeDebugOptions
{
	FLandscapeDebugOptions()
		: bShowPatches(false)
		, bDisableStatic(false)
		, bDisableCombine(false)
		, PatchesConsoleCommand(
		TEXT("Landscape.Patches"),
		TEXT("Show/hide Landscape patches"),
		FConsoleCommandDelegate::CreateRaw(this, &FLandscapeDebugOptions::Patches))
		, StaticConsoleCommand(
		TEXT("Landscape.Static"),
		TEXT("Enable/disable Landscape static drawlists"),
		FConsoleCommandDelegate::CreateRaw(this, &FLandscapeDebugOptions::Static))
		, CombineConsoleCommand(
		TEXT("Landscape.Combine"),
		TEXT("Enable/disable Landscape component combining"),
		FConsoleCommandDelegate::CreateRaw(this, &FLandscapeDebugOptions::Combine))
	{
	}

	bool bShowPatches;
	bool bDisableStatic;
	bool bDisableCombine;

private:
	FAutoConsoleCommand PatchesConsoleCommand;
	FAutoConsoleCommand StaticConsoleCommand;
	FAutoConsoleCommand CombineConsoleCommand;

	void Patches()
	{
		bShowPatches = !bShowPatches;
		UE_LOG(LogLandscape, Display, TEXT("Landscape.Patches: %s"), bShowPatches ? TEXT("Show") : TEXT("Hide"));
	}

	void Static()
	{
		bDisableStatic = !bDisableStatic;
		UE_LOG(LogLandscape, Display, TEXT("Landscape.Static: %s"), bDisableStatic ? TEXT("Disabled") : TEXT("Enabled"));
	}

	void Combine()
	{
		bDisableCombine = !bDisableCombine;
		UE_LOG(LogLandscape, Display, TEXT("Landscape.Combine: %s"), bDisableCombine ? TEXT("Disabled") : TEXT("Enabled"));
	}
};

FLandscapeDebugOptions GLandscapeDebugOptions;


#if WITH_EDITOR
LANDSCAPE_API bool GLandscapeEditModeActive = false;
LANDSCAPE_API ELandscapeViewMode::Type GLandscapeViewMode = ELandscapeViewMode::Normal;
LANDSCAPE_API int32 GLandscapeEditRenderMode = ELandscapeEditRenderMode::None;
UMaterialInterface* GLayerDebugColorMaterial = nullptr;
UMaterialInterface* GSelectionColorMaterial = nullptr;
UMaterialInterface* GSelectionRegionMaterial = nullptr;
UMaterialInterface* GMaskRegionMaterial = nullptr;
UTexture2D* GLandscapeBlackTexture = nullptr;
UMaterialInterface* GLandscapeLayerUsageMaterial = nullptr;
#endif

void ULandscapeComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	// TODO - investigate whether this is correct
	OutMaterials.Append(MaterialInstances.FilterByPredicate([](UMaterialInstance* MaterialInstance) { return MaterialInstance != nullptr; }));

	if (OverrideMaterial)
	{
		OutMaterials.Add(OverrideMaterial);
	}

	if (OverrideHoleMaterial)
	{
		OutMaterials.Add(OverrideHoleMaterial);
	}

	if (MobileMaterialInterface)
	{
		OutMaterials.AddUnique(MobileMaterialInterface);
	}

#if WITH_EDITORONLY_DATA
	if (EditToolRenderData.ToolMaterial)
	{
		OutMaterials.Add(EditToolRenderData.ToolMaterial);
	}

	if (EditToolRenderData.GizmoMaterial)
	{
		OutMaterials.Add(EditToolRenderData.GizmoMaterial);
	}
#endif

#if WITH_EDITOR
	//if (bGetDebugMaterials) // TODO: This should be tested and enabled
	{
		OutMaterials.Add(GLayerDebugColorMaterial);
		OutMaterials.Add(GSelectionColorMaterial);
		OutMaterials.Add(GSelectionRegionMaterial);
		OutMaterials.Add(GMaskRegionMaterial);
		OutMaterials.Add(GLandscapeLayerUsageMaterial);
	}
#endif
}

//
// FLandscapeComponentSceneProxy
//
TMap<uint32, FLandscapeSharedBuffers*>FLandscapeComponentSceneProxy::SharedBuffersMap;
TMap<uint32, FLandscapeSharedAdjacencyIndexBuffer*>FLandscapeComponentSceneProxy::SharedAdjacencyIndexBufferMap;
TMap<FLandscapeNeighborInfo::FLandscapeKey, TMap<FIntPoint, const FLandscapeNeighborInfo*> > FLandscapeNeighborInfo::SharedSceneProxyMap;

const static FName NAME_LandscapeResourceNameForDebugging(TEXT("Landscape"));

FLandscapeComponentSceneProxy::FLandscapeComponentSceneProxy(ULandscapeComponent* InComponent, TArrayView<UMaterialInterface* const> InMaterialInterfacesByLOD)
	: FPrimitiveSceneProxy(InComponent, NAME_LandscapeResourceNameForDebugging)
	, FLandscapeNeighborInfo(InComponent->GetWorld(), InComponent->GetLandscapeProxy()->GetLandscapeGuid(), InComponent->GetSectionBase() / InComponent->ComponentSizeQuads, InComponent->HeightmapTexture, InComponent->ForcedLOD, InComponent->LODBias)
	, MaxLOD(FMath::CeilLogTwo(InComponent->SubsectionSizeQuads + 1) - 1)
	, FirstLOD(0)
	, LastLOD(FMath::CeilLogTwo(InComponent->SubsectionSizeQuads + 1) - 1)
	, NumSubsections(InComponent->NumSubsections)
	, SubsectionSizeQuads(InComponent->SubsectionSizeQuads)
	, SubsectionSizeVerts(InComponent->SubsectionSizeQuads + 1)
	, ComponentSizeQuads(InComponent->ComponentSizeQuads)
	, ComponentSizeVerts(InComponent->ComponentSizeQuads + 1)
	, StaticLightingLOD(InComponent->GetLandscapeProxy()->StaticLightingLOD)
	, SectionBase(InComponent->GetSectionBase())
	, WeightmapScaleBias(InComponent->WeightmapScaleBias)
	, WeightmapSubsectionOffset(InComponent->WeightmapSubsectionOffset)
	, WeightmapTextures(InComponent->WeightmapTextures)
	, NumWeightmapLayerAllocations(InComponent->WeightmapLayerAllocations.Num())
	, NormalmapTexture(InComponent->HeightmapTexture)
	, BaseColorForGITexture(InComponent->GIBakedBaseColorTexture)
	, HeightmapScaleBias(InComponent->HeightmapScaleBias)
	, XYOffsetmapTexture(InComponent->XYOffsetmapTexture)
	, SharedBuffersKey(0)
	, SharedBuffers(nullptr)
	, VertexFactory(nullptr)
#if WITH_EDITORONLY_DATA
	, EditToolRenderData(InComponent->EditToolRenderData)
#endif
	, ComponentLightInfo(nullptr)
	, LandscapeComponent(InComponent)
	, LODFalloff(InComponent->GetLandscapeProxy()->LODFalloff)
#if WITH_EDITOR || !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	, CollisionMipLevel(InComponent->CollisionMipLevel)
	, SimpleCollisionMipLevel(InComponent->SimpleCollisionMipLevel)
	, CollisionResponse(InComponent->GetLandscapeProxy()->BodyInstance.GetResponseToChannels())
#endif
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	, LightMapResolution(InComponent->GetStaticLightMapResolution())
#endif
{
	MaterialInterfacesByLOD.Append(InMaterialInterfacesByLOD.GetData(), InMaterialInterfacesByLOD.Num());

	if (!IsComponentLevelVisible())
	{
		bNeedsLevelAddedToWorldNotification = true;
	}

	LevelColor = FLinearColor(1.f, 1.f, 1.f);

	const auto FeatureLevel = GetScene().GetFeatureLevel();
	if (FeatureLevel <= ERHIFeatureLevel::ES3_1)
	{
		HeightmapTexture = nullptr;
		HeightmapSubsectionOffsetU = 0;
		HeightmapSubsectionOffsetV = 0;
	}
	else
	{
		HeightmapSubsectionOffsetU = ((float)(InComponent->SubsectionSizeQuads + 1) / (float)HeightmapTexture->GetSizeX());
		HeightmapSubsectionOffsetV = ((float)(InComponent->SubsectionSizeQuads + 1) / (float)HeightmapTexture->GetSizeY());
	}

	LODBias = FMath::Clamp<int8>(LODBias, -MaxLOD, MaxLOD);

	if (InComponent->GetLandscapeProxy()->MaxLODLevel >= 0)
	{
		MaxLOD = FMath::Min<int8>(MaxLOD, InComponent->GetLandscapeProxy()->MaxLODLevel);
	}

	FirstLOD = (ForcedLOD >= 0) ? FMath::Min<int32>(ForcedLOD, MaxLOD) : FMath::Max<int32>(LODBias, 0);
	LastLOD = (ForcedLOD >= 0) ? FirstLOD : MaxLOD;	// we always need to go to MaxLOD regardless of LODBias as we could need the lowest LODs due to streaming.

	float LODDistanceFactor;
	switch (LODFalloff)
	{
	case ELandscapeLODFalloff::SquareRoot:
		LODDistanceFactor = FMath::Square(FMath::Min(LANDSCAPE_LOD_SQUARE_ROOT_FACTOR * InComponent->GetLandscapeProxy()->LODDistanceFactor, MAX_LANDSCAPE_LOD_DISTANCE_FACTOR));
		break;
	case ELandscapeLODFalloff::Linear:
	default:
		LODDistanceFactor = InComponent->GetLandscapeProxy()->LODDistanceFactor;
		break;
	}

	LODDistance = FMath::Sqrt(2.f * FMath::Square((float)SubsectionSizeQuads)) * LANDSCAPE_LOD_DISTANCE_FACTOR / LODDistanceFactor; // vary in 0...1
	DistDiff = -FMath::Sqrt(2.f * FMath::Square(0.5f*(float)SubsectionSizeQuads));

	if (InComponent->StaticLightingResolution > 0.f)
	{
		StaticLightingResolution = InComponent->StaticLightingResolution;
	}
	else
	{
		StaticLightingResolution = InComponent->GetLandscapeProxy()->StaticLightingResolution;
	}

	ComponentLightInfo = MakeUnique<FLandscapeLCI>(InComponent);
	check(ComponentLightInfo);

	const bool bHasStaticLighting = ComponentLightInfo->GetLightMap() || ComponentLightInfo->GetShadowMap();

	// Check material usage
	if (ensure(MaterialInterfacesByLOD.Num() > 0))
	{
		for (UMaterialInterface*& MaterialInterface : MaterialInterfacesByLOD)
		{
			if (MaterialInterface == nullptr ||
				(bHasStaticLighting && !MaterialInterface->CheckMaterialUsage(MATUSAGE_StaticLighting)))
			{
				MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
			}
		}
	}
	else
	{
		MaterialInterfacesByLOD.Add(UMaterial::GetDefaultMaterial(MD_Surface));
	}

	// TODO - LOD Materials - Currently all LOD materials are instances of [0] so have the same relevance
	MaterialRelevance = MaterialInterfacesByLOD[0]->GetRelevance(FeatureLevel);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || (UE_BUILD_SHIPPING && WITH_EDITOR)
	if (GIsEditor)
	{
		ALandscapeProxy* Proxy = InComponent->GetLandscapeProxy();
		// Try to find a color for level coloration.
		if (Proxy)
		{
			ULevel* Level = Proxy->GetLevel();
			ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel(Level);
			if (LevelStreaming)
			{
				LevelColor = LevelStreaming->LevelColor;
			}
		}
	}
#endif

	// We dissallow tessellation after LOD 0 so bRequiresAdjacencyInformation can only be true if LOD 0 needs it
	bRequiresAdjacencyInformation = MaterialSettingsRequireAdjacencyInformation_GameThread(MaterialInterfacesByLOD[0], XYOffsetmapTexture == nullptr ? &FLandscapeVertexFactory::StaticType : &FLandscapeXYOffsetVertexFactory::StaticType, InComponent->GetWorld()->FeatureLevel);

	const int8 SubsectionSizeLog2 = FMath::CeilLogTwo(InComponent->SubsectionSizeQuads + 1);
	SharedBuffersKey = (SubsectionSizeLog2 & 0xf) | ((NumSubsections & 0xf) << 4) |
	                   (FeatureLevel <= ERHIFeatureLevel::ES3_1 ? 0 : 1 << 30) | (XYOffsetmapTexture == nullptr ? 0 : 1 << 31);

	bSupportsHeightfieldRepresentation = true;

#if WITH_EDITOR
	for (auto& Allocation : InComponent->WeightmapLayerAllocations)
	{
		if (Allocation.LayerInfo != nullptr && Allocation.LayerInfo != ALandscapeProxy::VisibilityLayer)
		{
			// Use black for hole layer
			LayerColors.Add(Allocation.LayerInfo->LayerUsageDebugColor);
		}
	}
#endif
}

void FLandscapeComponentSceneProxy::CreateRenderThreadResources()
{
	check(HeightmapTexture != nullptr);

	if (IsComponentLevelVisible())
	{
		RegisterNeighbors();
	}

	auto FeatureLevel = GetScene().GetFeatureLevel();

	SharedBuffers = FLandscapeComponentSceneProxy::SharedBuffersMap.FindRef(SharedBuffersKey);
	if (SharedBuffers == nullptr)
	{
		SharedBuffers = new FLandscapeSharedBuffers(
			SharedBuffersKey, SubsectionSizeQuads, NumSubsections,
			FeatureLevel, bRequiresAdjacencyInformation);

		FLandscapeComponentSceneProxy::SharedBuffersMap.Add(SharedBuffersKey, SharedBuffers);

		if (!XYOffsetmapTexture)
		{
			FLandscapeVertexFactory* LandscapeVertexFactory = new FLandscapeVertexFactory();
			LandscapeVertexFactory->Data.PositionComponent = FVertexStreamComponent(SharedBuffers->VertexBuffer, 0, sizeof(FLandscapeVertex), VET_Float4);
			LandscapeVertexFactory->InitResource();
			SharedBuffers->VertexFactory = LandscapeVertexFactory;
		}
		else
		{
			FLandscapeXYOffsetVertexFactory* LandscapeXYOffsetVertexFactory = new FLandscapeXYOffsetVertexFactory();
			LandscapeXYOffsetVertexFactory->Data.PositionComponent = FVertexStreamComponent(SharedBuffers->VertexBuffer, 0, sizeof(FLandscapeVertex), VET_Float4);
			LandscapeXYOffsetVertexFactory->InitResource();
			SharedBuffers->VertexFactory = LandscapeXYOffsetVertexFactory;
		}
	}

	SharedBuffers->AddRef();

	if (bRequiresAdjacencyInformation)
	{
		if (SharedBuffers->AdjacencyIndexBuffers == nullptr)
		{
			ensure(SharedBuffers->NumIndexBuffers > 0);
			if (SharedBuffers->IndexBuffers[0])
			{
				// Recreate Index Buffers, this case happens only there are Landscape Components using different material (one uses tessellation, other don't use it) 
				if (SharedBuffers->bUse32BitIndices && !((FRawStaticIndexBuffer16or32<uint32>*)SharedBuffers->IndexBuffers[0])->Num())
				{
					SharedBuffers->CreateIndexBuffers<uint32>(FeatureLevel, bRequiresAdjacencyInformation);
				}
				else if (!((FRawStaticIndexBuffer16or32<uint16>*)SharedBuffers->IndexBuffers[0])->Num())
				{
					SharedBuffers->CreateIndexBuffers<uint16>(FeatureLevel, bRequiresAdjacencyInformation);
				}
			}

			SharedBuffers->AdjacencyIndexBuffers = new FLandscapeSharedAdjacencyIndexBuffer(SharedBuffers);
			FLandscapeComponentSceneProxy::SharedAdjacencyIndexBufferMap.Add(SharedBuffersKey, SharedBuffers->AdjacencyIndexBuffers);
		}
		SharedBuffers->AdjacencyIndexBuffers->AddRef();

		// Delayed Initialize for IndexBuffers
		for (int32 i = 0; i < SharedBuffers->NumIndexBuffers; i++)
		{
			SharedBuffers->IndexBuffers[i]->InitResource();
		}
	}

	// Assign vertex factory
	VertexFactory = SharedBuffers->VertexFactory;

	// Assign LandscapeUniformShaderParameters
	LandscapeUniformShaderParameters.InitResource();

#if WITH_EDITOR
	// Create MeshBatch for grass rendering
	if(SharedBuffers->GrassIndexBuffer)
	{
		const int32 NumMips = FMath::CeilLogTwo(SubsectionSizeVerts);
		GrassMeshBatch.Elements.Empty(NumMips);
		GrassMeshBatch.Elements.AddDefaulted(NumMips);
		GrassBatchParams.Empty(NumMips);
		GrassBatchParams.AddDefaulted(NumMips);

		FMaterialRenderProxy* RenderProxy = MaterialInterfacesByLOD[0]->GetRenderProxy(false);
		GrassMeshBatch.VertexFactory = VertexFactory;
		GrassMeshBatch.MaterialRenderProxy = RenderProxy;
		GrassMeshBatch.LCI = nullptr;
		GrassMeshBatch.ReverseCulling = false;
		GrassMeshBatch.CastShadow = false;
		GrassMeshBatch.Type = PT_PointList;
		GrassMeshBatch.DepthPriorityGroup = SDPG_World;

		// Combined grass rendering batch element
		FMeshBatchElement* GrassBatchElement = &GrassMeshBatch.Elements[0];
		FLandscapeBatchElementParams* BatchElementParams = &GrassBatchParams[0];
		BatchElementParams->LocalToWorldNoScalingPtr = &LocalToWorldNoScaling;
		BatchElementParams->LandscapeUniformShaderParametersResource = &LandscapeUniformShaderParameters;
		BatchElementParams->SceneProxy = this;
		BatchElementParams->SubX = -1;
		BatchElementParams->SubY = -1;
		BatchElementParams->CurrentLOD = 0;
		GrassBatchElement->UserData = BatchElementParams;
		if (NeedsUniformBufferUpdate())
		{
			UpdateUniformBuffer();
		}
		GrassBatchElement->PrimitiveUniformBufferResource = &GetUniformBuffer();
		GrassBatchElement->IndexBuffer = SharedBuffers->GrassIndexBuffer;
		GrassBatchElement->NumPrimitives = FMath::Square(NumSubsections) * FMath::Square(SubsectionSizeVerts);
		GrassBatchElement->FirstIndex = 0;
		GrassBatchElement->MinVertexIndex = 0;
		GrassBatchElement->MaxVertexIndex = SharedBuffers->NumVertices - 1;

		for (int32 Mip = 1; Mip < NumMips; ++Mip)
		{
			const int32 MipSubsectionSizeVerts = SubsectionSizeVerts >> Mip;

			FMeshBatchElement* CollisionBatchElement = &GrassMeshBatch.Elements[Mip];
			*CollisionBatchElement = *GrassBatchElement;
			FLandscapeBatchElementParams* CollisionBatchElementParams = &GrassBatchParams[Mip];
			*CollisionBatchElementParams = *BatchElementParams;
			CollisionBatchElementParams->CurrentLOD = Mip;
			CollisionBatchElement->UserData = CollisionBatchElementParams;
			CollisionBatchElement->NumPrimitives = FMath::Square(NumSubsections) * FMath::Square(MipSubsectionSizeVerts);
			CollisionBatchElement->FirstIndex = SharedBuffers->GrassIndexMipOffsets[Mip];
		}
	}
#endif
}

void FLandscapeComponentSceneProxy::OnLevelAddedToWorld()
{
	RegisterNeighbors();
}

FLandscapeComponentSceneProxy::~FLandscapeComponentSceneProxy()
{
	UnregisterNeighbors();

	// Free the subsection uniform buffer
	LandscapeUniformShaderParameters.ReleaseResource();

	if (SharedBuffers)
	{
		check(SharedBuffers == FLandscapeComponentSceneProxy::SharedBuffersMap.FindRef(SharedBuffersKey));
		if (SharedBuffers->Release() == 0)
		{
			FLandscapeComponentSceneProxy::SharedBuffersMap.Remove(SharedBuffersKey);
		}
		SharedBuffers = nullptr;
	}
}

int32 GAllowLandscapeShadows = 1;
static FAutoConsoleVariableRef CVarAllowLandscapeShadows(
	TEXT("r.AllowLandscapeShadows"),
	GAllowLandscapeShadows,
	TEXT("Allow Landscape Shadows")
	);

bool FLandscapeComponentSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}

FPrimitiveViewRelevance FLandscapeComponentSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	const bool bCollisionView = (View->Family->EngineShowFlags.CollisionVisibility || View->Family->EngineShowFlags.CollisionPawn);
	Result.bDrawRelevance = (IsShown(View) || bCollisionView) && View->Family->EngineShowFlags.Landscape;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();

	auto FeatureLevel = View->GetFeatureLevel();

#if WITH_EDITOR
	if (!GLandscapeEditModeActive)
	{
		// No tools to render, just use the cached material relevance.
#endif
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
#if WITH_EDITOR
	}
	else
	{
		// Also add the tool material(s)'s relevance to the MaterialRelevance
		FMaterialRelevance ToolRelevance = MaterialRelevance;

		// Tool brushes and Gizmo
		if (EditToolRenderData.ToolMaterial)
		{
			Result.bDynamicRelevance = true;
			ToolRelevance |= EditToolRenderData.ToolMaterial->GetRelevance_Concurrent(FeatureLevel);
		}

		if (EditToolRenderData.GizmoMaterial)
		{
			Result.bDynamicRelevance = true;
			ToolRelevance |= EditToolRenderData.GizmoMaterial->GetRelevance_Concurrent(FeatureLevel);
		}

		// Region selection
		if (EditToolRenderData.SelectedType)
		{
			if ((GLandscapeEditRenderMode & ELandscapeEditRenderMode::SelectRegion) && (EditToolRenderData.SelectedType & FLandscapeEditToolRenderData::ST_REGION)
				&& !(GLandscapeEditRenderMode & ELandscapeEditRenderMode::Mask) && GSelectionRegionMaterial)
			{
				Result.bDynamicRelevance = true;
				ToolRelevance |= GSelectionRegionMaterial->GetRelevance_Concurrent(FeatureLevel);
			}
			if ((GLandscapeEditRenderMode & ELandscapeEditRenderMode::SelectComponent) && (EditToolRenderData.SelectedType & FLandscapeEditToolRenderData::ST_COMPONENT) && GSelectionColorMaterial)
			{
				Result.bDynamicRelevance = true;
				ToolRelevance |= GSelectionColorMaterial->GetRelevance_Concurrent(FeatureLevel);
			}
		}

		// Mask
		if ((GLandscapeEditRenderMode & ELandscapeEditRenderMode::Mask) && GMaskRegionMaterial != nullptr &&
			(((EditToolRenderData.SelectedType & FLandscapeEditToolRenderData::ST_REGION)) || (!(GLandscapeEditRenderMode & ELandscapeEditRenderMode::InvertedMask))))
		{
			Result.bDynamicRelevance = true;
			ToolRelevance |= GMaskRegionMaterial->GetRelevance_Concurrent(FeatureLevel);
		}

		ToolRelevance.SetPrimitiveViewRelevance(Result);
	}

	// Various visualizations need to render using dynamic relevance
	if ((View->Family->EngineShowFlags.Bounds && IsSelected()) ||
		GLandscapeDebugOptions.bShowPatches)
	{
		Result.bDynamicRelevance = true;
	}
#endif

#if WITH_EDITOR || !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const bool bInCollisionView = View->Family->EngineShowFlags.CollisionVisibility || View->Family->EngineShowFlags.CollisionPawn;
#endif

	// Use the dynamic path for rendering landscape components pass only for Rich Views or if the static path is disabled for debug.
	if (IsRichView(*View->Family) ||
#if WITH_EDITOR || !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		bInCollisionView ||
#endif
		GLandscapeDebugOptions.bDisableStatic ||
		View->Family->EngineShowFlags.Wireframe ||
#if WITH_EDITOR
		(IsSelected() && !GLandscapeEditModeActive) ||
		GLandscapeViewMode != ELandscapeViewMode::Normal ||
#else
		IsSelected() ||
#endif
		 !IsStaticPathAvailable())
	{
		Result.bDynamicRelevance = true;
	}
	else
	{
		Result.bStaticRelevance = true;
	}

	Result.bShadowRelevance = (GAllowLandscapeShadows > 0) && IsShadowCast(View);
	return Result;
}

/**
*	Determines the relevance of this primitive's elements to the given light.
*	@param	LightSceneProxy			The light to determine relevance for
*	@param	bDynamic (output)		The light is dynamic for this primitive
*	@param	bRelevant (output)		The light is relevant for this primitive
*	@param	bLightMapped (output)	The light is light mapped for this primitive
*/
void FLandscapeComponentSceneProxy::GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const
{
	// Attach the light to the primitive's static meshes.
	bDynamic = true;
	bRelevant = false;
	bLightMapped = true;
	bShadowMapped = true;

	if (ComponentLightInfo)
	{
		ELightInteractionType InteractionType = ComponentLightInfo->GetInteraction(LightSceneProxy).GetType();

		if (InteractionType != LIT_CachedIrrelevant)
		{
			bRelevant = true;
		}

		if (InteractionType != LIT_CachedLightMap && InteractionType != LIT_CachedIrrelevant)
		{
			bLightMapped = false;
		}

		if (InteractionType != LIT_Dynamic)
		{
			bDynamic = false;
		}

		if (InteractionType != LIT_CachedSignedDistanceFieldShadowMap2D)
		{
			bShadowMapped = false;
		}
	}
	else
	{
		bRelevant = true;
		bLightMapped = false;
	}
}

FLightInteraction FLandscapeComponentSceneProxy::FLandscapeLCI::GetInteraction(const class FLightSceneProxy* LightSceneProxy) const
{
	// ask base class
	ELightInteractionType LightInteraction = GetStaticInteraction(LightSceneProxy, IrrelevantLights);
	
	if(LightInteraction != LIT_MAX)
	{
		return FLightInteraction(LightInteraction);
	}

	// Use dynamic lighting if the light doesn't have static lighting.
	return FLightInteraction::Dynamic();
}

#if WITH_EDITOR
namespace DebugColorMask
{
	const FLinearColor Masks[5] =
	{
		FLinearColor(1.f, 0.f, 0.f, 0.f),
		FLinearColor(0.f, 1.f, 0.f, 0.f),
		FLinearColor(0.f, 0.f, 1.f, 0.f),
		FLinearColor(0.f, 0.f, 0.f, 1.f),
		FLinearColor(0.f, 0.f, 0.f, 0.f)
	};
};
#endif

void FLandscapeComponentSceneProxy::OnTransformChanged()
{
	// Set Lightmap ScaleBias
	int32 PatchExpandCountX = 0;
	int32 PatchExpandCountY = 0;
	int32 DesiredSize = 1; // output by GetTerrainExpandPatchCount but not used below
	const float LightMapRatio = ::GetTerrainExpandPatchCount(StaticLightingResolution, PatchExpandCountX, PatchExpandCountY, ComponentSizeQuads, (NumSubsections * (SubsectionSizeQuads + 1)), DesiredSize, StaticLightingLOD);
	const float LightmapLODScaleX = LightMapRatio / ((ComponentSizeVerts >> StaticLightingLOD) + 2 * PatchExpandCountX);
	const float LightmapLODScaleY = LightMapRatio / ((ComponentSizeVerts >> StaticLightingLOD) + 2 * PatchExpandCountY);
	const float LightmapBiasX = PatchExpandCountX * LightmapLODScaleX;
	const float LightmapBiasY = PatchExpandCountY * LightmapLODScaleY;
	const float LightmapScaleX = LightmapLODScaleX * (float)((ComponentSizeVerts >> StaticLightingLOD) - 1) / ComponentSizeQuads;
	const float LightmapScaleY = LightmapLODScaleY * (float)((ComponentSizeVerts >> StaticLightingLOD) - 1) / ComponentSizeQuads;
	const float LightmapExtendFactorX = (float)SubsectionSizeQuads * LightmapScaleX;
	const float LightmapExtendFactorY = (float)SubsectionSizeQuads * LightmapScaleY;

	// cache component's WorldToLocal
	FMatrix LtoW = GetLocalToWorld();
	WorldToLocal = LtoW.InverseFast();

	// cache component's LocalToWorldNoScaling
	LocalToWorldNoScaling = LtoW;
	LocalToWorldNoScaling.RemoveScaling();

	// Set FLandscapeUniformVSParameters for this subsection
	FLandscapeUniformShaderParameters LandscapeParams;
	LandscapeParams.HeightmapUVScaleBias = HeightmapScaleBias;
	LandscapeParams.WeightmapUVScaleBias = WeightmapScaleBias;
	LandscapeParams.LocalToWorldNoScaling = LocalToWorldNoScaling;

	LandscapeParams.LandscapeLightmapScaleBias = FVector4(
		LightmapScaleX,
		LightmapScaleY,
		LightmapBiasY,
		LightmapBiasX);
	LandscapeParams.SubsectionSizeVertsLayerUVPan = FVector4(
		SubsectionSizeVerts,
		1.f / (float)SubsectionSizeQuads,
		SectionBase.X,
		SectionBase.Y
		);
	LandscapeParams.SubsectionOffsetParams = FVector4(
		HeightmapSubsectionOffsetU,
		HeightmapSubsectionOffsetV,
		WeightmapSubsectionOffset,
		SubsectionSizeQuads
		);
	LandscapeParams.LightmapSubsectionOffsetParams = FVector4(
		LightmapExtendFactorX,
		LightmapExtendFactorY,
		0,
		0
		);

	LandscapeUniformShaderParameters.SetContents(LandscapeParams);
}

/**
* Draw the scene proxy as a dynamic element
*
* @param	PDI - draw interface to render to
* @param	View - current view
*/

void FLandscapeComponentSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* PDI)
{
	const int32 NumBatchesPerLOD = (ForcedLOD < 0 && NumSubsections > 1) ? (FMath::Square(NumSubsections) + 1) : 1;
	const int32 NumBatchesLastLOD = (ForcedLOD < 0) ? (1 + LastLOD - FirstLOD) * NumBatchesPerLOD : 1;

	StaticBatchParamArray.Empty(ForcedLOD < 0 ? (1 + LastLOD - FirstLOD) * NumBatchesPerLOD : 1);

	const int32 LastMaterialIndex = MaterialInterfacesByLOD.Num() - 1;
	const int32 LastMaterialLOD = FMath::Min(LastLOD, LastMaterialIndex);

	for (int i = FirstLOD; i <= LastLOD; ++i)
	{
		// the LastMaterialLOD covers all LODs up to LastLOD
		const bool bLast = (i >= LastMaterialLOD);

		FMeshBatch MeshBatch;
		MeshBatch.Elements.Empty(bLast ? NumBatchesLastLOD : NumBatchesPerLOD);

		UMaterialInterface* MaterialInterface = MaterialInterfacesByLOD[FMath::Min(i, LastMaterialIndex)];

		// Could be different from bRequiresAdjacencyInformation during shader compilation
		bool bCurrentRequiresAdjacencyInformation = MaterialRenderingRequiresAdjacencyInformation_RenderingThread(MaterialInterface, VertexFactory->GetType(), GetScene().GetFeatureLevel());

		if (bCurrentRequiresAdjacencyInformation)
		{
			check(SharedBuffers->AdjacencyIndexBuffers);
		}

		FMaterialRenderProxy* RenderProxy = MaterialInterface->GetRenderProxy(false);

		MeshBatch.VertexFactory = VertexFactory;
		MeshBatch.MaterialRenderProxy = RenderProxy;
		MeshBatch.LCI = ComponentLightInfo.Get();
		MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
		MeshBatch.CastShadow = true;
		MeshBatch.Type = bCurrentRequiresAdjacencyInformation ? PT_12_ControlPointPatchList : PT_TriangleList;
		MeshBatch.DepthPriorityGroup = SDPG_World;
		MeshBatch.LODIndex = 0;
		MeshBatch.bRequiresPerElementVisibility = true;

		for (int32 LOD = i; LOD <= (bLast ? LastLOD : i); LOD++)
		{
			int32 LodSubsectionSizeVerts = SubsectionSizeVerts >> LOD;

			if (ForcedLOD < 0 && NumSubsections > 1)
			{
				// Per-subsection batch elements
				for (int32 SubY = 0; SubY < NumSubsections; SubY++)
				{
					for (int32 SubX = 0; SubX < NumSubsections; SubX++)
					{
						FMeshBatchElement* BatchElement = new(MeshBatch.Elements) FMeshBatchElement;
						FLandscapeBatchElementParams* BatchElementParams = new(StaticBatchParamArray)FLandscapeBatchElementParams;
						BatchElement->UserData = BatchElementParams;
						BatchElement->PrimitiveUniformBufferResource = &GetUniformBuffer();
						BatchElementParams->LandscapeUniformShaderParametersResource = &LandscapeUniformShaderParameters;
						BatchElementParams->LocalToWorldNoScalingPtr = &LocalToWorldNoScaling;
						BatchElementParams->SceneProxy = this;
						BatchElementParams->SubX = SubX;
						BatchElementParams->SubY = SubY;
						BatchElementParams->CurrentLOD = LOD;
						uint32 NumPrimitives = FMath::Square((LodSubsectionSizeVerts - 1)) * 2;
						if (bCurrentRequiresAdjacencyInformation)
						{
							BatchElement->IndexBuffer = SharedBuffers->AdjacencyIndexBuffers->IndexBuffers[LOD];
							BatchElement->FirstIndex = (SubX + SubY * NumSubsections) * NumPrimitives * 12;
						}
						else
						{
							BatchElement->IndexBuffer = SharedBuffers->IndexBuffers[LOD];
							BatchElement->FirstIndex = (SubX + SubY * NumSubsections) * NumPrimitives * 3;
						}
						BatchElement->NumPrimitives = NumPrimitives;
						BatchElement->MinVertexIndex = SharedBuffers->IndexRanges[LOD].MinIndex[SubX][SubY];
						BatchElement->MaxVertexIndex = SharedBuffers->IndexRanges[LOD].MaxIndex[SubX][SubY];
					}
				}
			}

			// Combined batch element
			FMeshBatchElement* BatchElement = new(MeshBatch.Elements) FMeshBatchElement;
			FLandscapeBatchElementParams* BatchElementParams = new(StaticBatchParamArray)FLandscapeBatchElementParams;
			BatchElementParams->LocalToWorldNoScalingPtr = &LocalToWorldNoScaling;
			BatchElement->UserData = BatchElementParams;
			BatchElement->PrimitiveUniformBufferResource = &GetUniformBuffer();
			BatchElementParams->LandscapeUniformShaderParametersResource = &LandscapeUniformShaderParameters;
			BatchElementParams->SceneProxy = this;
			BatchElementParams->SubX = -1;
			BatchElementParams->SubY = -1;
			BatchElementParams->CurrentLOD = LOD;
			BatchElement->IndexBuffer = bCurrentRequiresAdjacencyInformation ? SharedBuffers->AdjacencyIndexBuffers->IndexBuffers[LOD] : SharedBuffers->IndexBuffers[LOD];
			BatchElement->NumPrimitives = FMath::Square((LodSubsectionSizeVerts - 1)) * FMath::Square(NumSubsections) * 2;
			BatchElement->FirstIndex = 0;
			BatchElement->MinVertexIndex = SharedBuffers->IndexRanges[LOD].MinIndexFull;
			BatchElement->MaxVertexIndex = SharedBuffers->IndexRanges[LOD].MaxIndexFull;
		}

		PDI->DrawMesh(MeshBatch, FLT_MAX);

		if (bLast)
		{
			break;
		}
	}
}

uint64 FLandscapeVertexFactory::GetStaticBatchElementVisibility(const class FSceneView& View, const struct FMeshBatch* Batch) const
{
	const FLandscapeComponentSceneProxy* SceneProxy = ((FLandscapeBatchElementParams*)Batch->Elements[0].UserData)->SceneProxy;
	return SceneProxy->GetStaticBatchElementVisibility(View, Batch);
}

uint64 FLandscapeComponentSceneProxy::GetStaticBatchElementVisibility(const class FSceneView& View, const struct FMeshBatch* Batch) const
{
	uint64 BatchesToRenderMask = 0;

	SCOPE_CYCLE_COUNTER(STAT_LandscapeStaticDrawLODTime);
	if (ForcedLOD >= 0)
	{
		// When forcing LOD we only create one Batch Element
		ensure(Batch->Elements.Num() == 1);
		int32 BatchElementIndex = 0;
		BatchesToRenderMask |= (((uint64)1) << BatchElementIndex);
		INC_DWORD_STAT(STAT_LandscapeDrawCalls);
		INC_DWORD_STAT_BY(STAT_LandscapeTriangles, Batch->Elements[BatchElementIndex].NumPrimitives);
	}
	else
	{
		// camera position in local heightmap space
		FVector CameraLocalPos3D = WorldToLocal.TransformPosition(View.ViewMatrices.GetViewOrigin());
		FVector2D CameraLocalPos(CameraLocalPos3D.X, CameraLocalPos3D.Y);

		int32 BatchesPerLOD = NumSubsections > 1 ? FMath::Square(NumSubsections) + 1 : 1;
		int32 CalculatedLods[LANDSCAPE_MAX_SUBSECTION_NUM][LANDSCAPE_MAX_SUBSECTION_NUM];
		int32 CombinedLOD = -1;
		int32 bAllSameLOD = true;

		int32 BatchLOD = ((FLandscapeBatchElementParams*)Batch->Elements[0].UserData)->CurrentLOD;

		for (int32 SubY = 0; SubY < NumSubsections; SubY++)
		{
			for (int32 SubX = 0; SubX < NumSubsections; SubX++)
			{
				int32 ThisSubsectionLOD = CalcLODForSubsection(View, SubX, SubY, CameraLocalPos);
				// check if all LODs are the same.
				if (ThisSubsectionLOD != CombinedLOD && CombinedLOD != -1)
				{
					bAllSameLOD = false;
				}
				CombinedLOD = ThisSubsectionLOD;
				CalculatedLods[SubX][SubY] = ThisSubsectionLOD;
			}
		}

		if (bAllSameLOD && NumSubsections > 1 && !GLandscapeDebugOptions.bDisableCombine)
		{
			// choose the combined batch element
			int32 BatchElementIndex = (CombinedLOD - BatchLOD + 1) * BatchesPerLOD - 1;
			if (Batch->Elements.IsValidIndex(BatchElementIndex))
			{
				BatchesToRenderMask |= (((uint64)1) << BatchElementIndex);
				INC_DWORD_STAT(STAT_LandscapeDrawCalls);
				INC_DWORD_STAT_BY(STAT_LandscapeTriangles, Batch->Elements[BatchElementIndex].NumPrimitives);
			}
		}
		else
		{
			for (int32 SubY = 0; SubY < NumSubsections; SubY++)
			{
				for (int32 SubX = 0; SubX < NumSubsections; SubX++)
				{
					int32 BatchElementIndex = (CalculatedLods[SubX][SubY] - BatchLOD) * BatchesPerLOD + SubY * NumSubsections + SubX;
					if (Batch->Elements.IsValidIndex(BatchElementIndex))
					{
						BatchesToRenderMask |= (((uint64)1) << BatchElementIndex);
						INC_DWORD_STAT(STAT_LandscapeDrawCalls);
						INC_DWORD_STAT_BY(STAT_LandscapeTriangles, Batch->Elements[BatchElementIndex].NumPrimitives);
					}
				}
			}
		}
	}

	INC_DWORD_STAT(STAT_LandscapeComponents);

	return BatchesToRenderMask;
}

float FLandscapeComponentSceneProxy::CalcDesiredLOD(const FSceneView& View, const FVector2D& CameraLocalPos, int32 SubX, int32 SubY) const
{
	int32 OverrideLOD = GetCVarForceLOD();
#if WITH_EDITOR
	if (View.Family->LandscapeLODOverride >= 0)
	{
		OverrideLOD = View.Family->LandscapeLODOverride;
	}
#endif
	if (OverrideLOD >= 0)
	{
		return FMath::Clamp<int32>(OverrideLOD, FirstLOD, LastLOD);
	}

	// FLandscapeComponentSceneProxy::NumSubsections, SubsectionSizeQuads, MaxLOD, LODFalloff and LODDistance are the same for all components and so are safe to use in the neighbour LOD calculations
	// HeightmapTexture, LODBias, ForcedLOD are component-specific with neighbor lookup
	const bool bIsInThisComponent = (SubX >= 0 && SubX < NumSubsections && SubY >= 0 && SubY < NumSubsections);

	auto* SubsectionHeightmapTexture = HeightmapTexture;
	int8 SubsectionForcedLOD = ForcedLOD;
	int8 SubsectionLODBias = LODBias;

	if (SubX < 0)
	{
		SubsectionHeightmapTexture = Neighbors[1] ? Neighbors[1]->HeightmapTexture : nullptr;
		SubsectionForcedLOD = Neighbors[1] ? Neighbors[1]->ForcedLOD : -1;
		SubsectionLODBias   = Neighbors[1] ? Neighbors[1]->LODBias : 0;
	}
	else if (SubX >= NumSubsections)
	{
		SubsectionHeightmapTexture = Neighbors[2] ? Neighbors[2]->HeightmapTexture : nullptr;
		SubsectionForcedLOD = Neighbors[2] ? Neighbors[2]->ForcedLOD : -1;
		SubsectionLODBias   = Neighbors[2] ? Neighbors[2]->LODBias : 0;
	}
	else if (SubY < 0)
	{
		SubsectionHeightmapTexture = Neighbors[0] ? Neighbors[0]->HeightmapTexture : nullptr;
		SubsectionForcedLOD = Neighbors[0] ? Neighbors[0]->ForcedLOD : -1;
		SubsectionLODBias   = Neighbors[0] ? Neighbors[0]->LODBias : 0;
	}
	else if (SubY >= NumSubsections)
	{
		SubsectionHeightmapTexture = Neighbors[3] ? Neighbors[3]->HeightmapTexture : nullptr;
		SubsectionForcedLOD = Neighbors[3] ? Neighbors[3]->ForcedLOD : -1;
		SubsectionLODBias   = Neighbors[3] ? Neighbors[3]->LODBias : 0;
	}

	SubsectionLODBias = FMath::Clamp<int8>(SubsectionLODBias + GLandscapeMeshLODBias, -MaxLOD, MaxLOD);

	const int32 MinStreamedLOD = SubsectionHeightmapTexture ? FMath::Min<int32>(((FTexture2DResource*)SubsectionHeightmapTexture->Resource)->GetCurrentFirstMip(), FMath::CeilLogTwo(SubsectionSizeVerts) - 1) : 0;

	float fLOD = FLT_MAX;

	if (SubsectionForcedLOD >= 0)
	{
		fLOD = SubsectionForcedLOD;
	}
	else
	{
		if (View.IsPerspectiveProjection())
		{
			FVector2D ComponentPosition(0.5f * (float)SubsectionSizeQuads, 0.5f * (float)SubsectionSizeQuads);
			FVector2D CurrentCameraLocalPos = CameraLocalPos - FVector2D(SubX * SubsectionSizeQuads, SubY * SubsectionSizeQuads);
			float ComponentDistance = FVector2D(CurrentCameraLocalPos - ComponentPosition).Size() + DistDiff;
			switch (LODFalloff)
			{
			case ELandscapeLODFalloff::SquareRoot:
				fLOD = FMath::Sqrt(FMath::Max(0.f, ComponentDistance / LODDistance));
				break;
			default:
			case ELandscapeLODFalloff::Linear:
				fLOD = ComponentDistance / LODDistance;
				break;
			}
		}
		else
		{
			float Scale = 1.0f / (View.ViewRect.Width() * View.ViewMatrices.GetProjectionMatrix().M[0][0]);

			// The "/ 5.0f" is totally arbitrary
			switch (LODFalloff)
			{
			case ELandscapeLODFalloff::SquareRoot:
				fLOD = FMath::Sqrt(Scale / 5.0f);
				break;
			default:
			case ELandscapeLODFalloff::Linear:
				fLOD = Scale / 5.0f;
				break;
			}
		}

		fLOD = FMath::Clamp<float>(fLOD, SubsectionLODBias, FMath::Min<int32>(MaxLOD, MaxLOD + SubsectionLODBias));
	}

	// ultimately due to texture streaming we sometimes need to go past MaxLOD
	fLOD = FMath::Max<float>(fLOD, MinStreamedLOD);

	return fLOD;
}

int32 FLandscapeComponentSceneProxy::CalcLODForSubsection(const FSceneView& View, int32 SubX, int32 SubY, const FVector2D& CameraLocalPos) const
{
	return FMath::FloorToInt(CalcDesiredLOD(View, CameraLocalPos, SubX, SubY));
}

void FLandscapeComponentSceneProxy::CalcLODParamsForSubsection(const FSceneView& View, const FVector2D& CameraLocalPos, int32 SubX, int32 SubY, int32 BatchLOD, float& OutfLOD, FVector4& OutNeighborLODs) const
{
	OutfLOD = FMath::Max<float>(BatchLOD, CalcDesiredLOD(View, CameraLocalPos, SubX, SubY));

	OutNeighborLODs[0] = FMath::Max<float>(OutfLOD, CalcDesiredLOD(View, CameraLocalPos, SubX,     SubY - 1));
	OutNeighborLODs[1] = FMath::Max<float>(OutfLOD, CalcDesiredLOD(View, CameraLocalPos, SubX - 1, SubY    ));
	OutNeighborLODs[2] = FMath::Max<float>(OutfLOD, CalcDesiredLOD(View, CameraLocalPos, SubX + 1, SubY    ));
	OutNeighborLODs[3] = FMath::Max<float>(OutfLOD, CalcDesiredLOD(View, CameraLocalPos, SubX,     SubY + 1));
}

namespace
{
    FLinearColor GetColorForLod(int32 CurrentLOD, int32 ForcedLOD)
    {
	    int32 ColorIndex = INDEX_NONE;
	    if (GEngine->LODColorationColors.Num() > 0)
	    {
		    ColorIndex = CurrentLOD;
		    ColorIndex = FMath::Clamp(ColorIndex, 0, GEngine->LODColorationColors.Num() - 1);
	    }
	    const FLinearColor& LODColor = ColorIndex != INDEX_NONE ? GEngine->LODColorationColors[ColorIndex] : FLinearColor::Gray;
	    return ForcedLOD >= 0 ? LODColor : LODColor * 0.2f;
	}
}

void FLandscapeComponentSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FLandscapeComponentSceneProxy_GetMeshElements);

#if WITH_EDITOR || !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const bool bInCollisionView = ViewFamily.EngineShowFlags.CollisionVisibility || ViewFamily.EngineShowFlags.CollisionPawn;
	const bool bDrawSimpleCollision  = ViewFamily.EngineShowFlags.CollisionPawn       && CollisionResponse.GetResponse(ECC_Pawn) != ECR_Ignore;
	const bool bDrawComplexCollision = ViewFamily.EngineShowFlags.CollisionVisibility && CollisionResponse.GetResponse(ECC_Visibility) != ECR_Ignore;
#endif

	int32 NumPasses = 0;
	int32 NumTriangles = 0;
	int32 NumDrawCalls = 0;
	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			FVector CameraLocalPos3D = WorldToLocal.TransformPosition(View->ViewMatrices.GetViewOrigin());
			FVector2D CameraLocalPos(CameraLocalPos3D.X, CameraLocalPos3D.Y);

			FLandscapeElementParamArray& ParameterArray = Collector.AllocateOneFrameResource<FLandscapeElementParamArray>();
			ParameterArray.ElementParams.Empty(NumSubsections * NumSubsections);
			ParameterArray.ElementParams.AddDefaulted(NumSubsections * NumSubsections);

			FMeshBatch& Mesh = Collector.AllocateMesh();
			Mesh.LCI = ComponentLightInfo.Get();
			Mesh.CastShadow = true;
			Mesh.VertexFactory = VertexFactory;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();

#if WITH_EDITOR
			FMeshBatch& MeshTools = Collector.AllocateMesh();
			MeshTools.LCI = ComponentLightInfo.Get();
			MeshTools.Type = PT_TriangleList;
			MeshTools.CastShadow = false;
			MeshTools.VertexFactory = VertexFactory;
			MeshTools.ReverseCulling = IsLocalToWorldDeterminantNegative();
#endif

			// Calculate the LOD to use for the material
			// TODO: Render different subsections with different material LODs like the static render pass does
			int32 MaterialLOD = MaterialInterfacesByLOD.Num() - 1;

			// Setup the LOD parameters
			int32 CalculatedLods[LANDSCAPE_MAX_SUBSECTION_NUM][LANDSCAPE_MAX_SUBSECTION_NUM];
			for (int32 SubY = 0; SubY < NumSubsections; SubY++)
			{
				for (int32 SubX = 0; SubX < NumSubsections; SubX++)
				{
					int32 CurrentLOD = CalcLODForSubsection(*View, SubX, SubY, CameraLocalPos);
#if WITH_EDITOR || !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					if (bInCollisionView)
					{
						if (bDrawSimpleCollision)
						{
							CurrentLOD = FMath::Max(CollisionMipLevel, SimpleCollisionMipLevel);
						}
						else if (bDrawComplexCollision)
						{
							CurrentLOD = CollisionMipLevel;
						}
					}
#endif
					CalculatedLods[SubY][SubX] = CurrentLOD;
					MaterialLOD = FMath::Min(MaterialLOD, CurrentLOD);
				}
			}

			UMaterialInterface* const MaterialInterface = MaterialInterfacesByLOD[MaterialLOD];

			// Could be different from bRequiresAdjacencyInformation during shader compilation
			// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
			bool bIsVxgiVoxelization = View->bIsVxgiVoxelization;
#else
			// View->bIsVxgiVoxelization is not declared if WITH_GFSDK_VXGI == 0
			bool bIsVxgiVoxelization = false;
#endif
			const bool bCurrentRequiresAdjacencyInformation = MaterialRenderingRequiresAdjacencyInformation_RenderingThread(MaterialInterface, VertexFactory->GetType(), View->GetFeatureLevel(), bIsVxgiVoxelization);
			// NVCHANGE_END: Add VXGI
			Mesh.Type = bCurrentRequiresAdjacencyInformation ? PT_12_ControlPointPatchList : PT_TriangleList;

			for (int32 SubY = 0; SubY < NumSubsections; SubY++)
			{
				for (int32 SubX = 0; SubX < NumSubsections; SubX++)
				{
					const int32 SubSectionIdx = SubX + SubY*NumSubsections;
					const int32 CurrentLOD = CalculatedLods[SubY][SubX];
			#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					// We simplify this by considering only the biggest LOD index for this mesh element.
					Mesh.VisualizeLODIndex = (int8)FMath::Max((int32)Mesh.VisualizeLODIndex, CurrentLOD);
			#endif
					FMeshBatchElement& BatchElement = (SubX == 0 && SubY == 0) ? *Mesh.Elements.GetData() : *(new(Mesh.Elements) FMeshBatchElement);
					BatchElement.PrimitiveUniformBufferResource = &GetUniformBuffer();
					FLandscapeBatchElementParams& BatchElementParams = ParameterArray.ElementParams[SubSectionIdx];
					BatchElementParams.LocalToWorldNoScalingPtr = &LocalToWorldNoScaling;
					BatchElement.UserData = &BatchElementParams;

					BatchElementParams.LandscapeUniformShaderParametersResource = &LandscapeUniformShaderParameters;
					BatchElementParams.SceneProxy = this;
					BatchElementParams.SubX = SubX;
					BatchElementParams.SubY = SubY;
					BatchElementParams.CurrentLOD = CurrentLOD;

					int32 LodSubsectionSizeVerts = (SubsectionSizeVerts >> CurrentLOD);
					uint32 NumPrimitives = FMath::Square((LodSubsectionSizeVerts - 1)) * 2;
					if (bCurrentRequiresAdjacencyInformation)
					{
						check(SharedBuffers->AdjacencyIndexBuffers);
						BatchElement.IndexBuffer = SharedBuffers->AdjacencyIndexBuffers->IndexBuffers[CurrentLOD];
						BatchElement.FirstIndex = (SubX + SubY * NumSubsections) * NumPrimitives * 12;
					}
					else
					{
						BatchElement.IndexBuffer = SharedBuffers->IndexBuffers[CurrentLOD];
						BatchElement.FirstIndex = (SubX + SubY * NumSubsections) * NumPrimitives * 3;
					}
					BatchElement.NumPrimitives = NumPrimitives;
					BatchElement.MinVertexIndex = SharedBuffers->IndexRanges[CurrentLOD].MinIndex[SubX][SubY];
					BatchElement.MaxVertexIndex = SharedBuffers->IndexRanges[CurrentLOD].MaxIndex[SubX][SubY];

#if WITH_EDITOR
					FMeshBatchElement& BatchElementTools = (SubX == 0 && SubY == 0) ? *MeshTools.Elements.GetData() : *(new(MeshTools.Elements) FMeshBatchElement);
					BatchElementTools.PrimitiveUniformBufferResource = &GetUniformBuffer();
					BatchElementTools.UserData = &BatchElementParams;

					// Tools never use tessellation
					BatchElementTools.IndexBuffer = SharedBuffers->IndexBuffers[CurrentLOD];
					BatchElementTools.NumPrimitives = NumPrimitives;
					BatchElementTools.FirstIndex = (SubX + SubY * NumSubsections) * NumPrimitives * 3;
					BatchElementTools.MinVertexIndex = SharedBuffers->IndexRanges[CurrentLOD].MinIndex[SubX][SubY];
					BatchElementTools.MaxVertexIndex = SharedBuffers->IndexRanges[CurrentLOD].MaxIndex[SubX][SubY];
#endif
				}
			}

			// Render the landscape component
#if WITH_EDITOR
			const bool bMaterialModifiesMeshPosition = MaterialInterface->GetRenderProxy(false)->GetMaterial(View->GetFeatureLevel())->MaterialModifiesMeshPosition_RenderThread();

			switch (GLandscapeViewMode)
			{
			case ELandscapeViewMode::DebugLayer:
				if (GLayerDebugColorMaterial)
				{
					auto DebugColorMaterialInstance = new FLandscapeDebugMaterialRenderProxy(GLayerDebugColorMaterial->GetRenderProxy(false),
						(EditToolRenderData.DebugChannelR >= 0 ? WeightmapTextures[EditToolRenderData.DebugChannelR / 4] : nullptr),
						(EditToolRenderData.DebugChannelG >= 0 ? WeightmapTextures[EditToolRenderData.DebugChannelG / 4] : nullptr),
						(EditToolRenderData.DebugChannelB >= 0 ? WeightmapTextures[EditToolRenderData.DebugChannelB / 4] : nullptr),
						(EditToolRenderData.DebugChannelR >= 0 ? DebugColorMask::Masks[EditToolRenderData.DebugChannelR % 4] : DebugColorMask::Masks[4]),
						(EditToolRenderData.DebugChannelG >= 0 ? DebugColorMask::Masks[EditToolRenderData.DebugChannelG % 4] : DebugColorMask::Masks[4]),
						(EditToolRenderData.DebugChannelB >= 0 ? DebugColorMask::Masks[EditToolRenderData.DebugChannelB % 4] : DebugColorMask::Masks[4])
						);

					MeshTools.MaterialRenderProxy = DebugColorMaterialInstance;
					Collector.RegisterOneFrameMaterialProxy(DebugColorMaterialInstance);

					MeshTools.bCanApplyViewModeOverrides = true;
					MeshTools.bUseWireframeSelectionColoring = IsSelected();

					Collector.AddMesh(ViewIndex, MeshTools);

					NumPasses++;
					NumTriangles += MeshTools.GetNumPrimitives();
					NumDrawCalls += MeshTools.Elements.Num();
				}
				break;

			case ELandscapeViewMode::LayerDensity:
				{
					int32 ColorIndex = FMath::Min<int32>(NumWeightmapLayerAllocations, GEngine->ShaderComplexityColors.Num());
					auto LayerDensityMaterialInstance = new FColoredMaterialRenderProxy(GEngine->LevelColorationUnlitMaterial->GetRenderProxy(false), ColorIndex ? GEngine->ShaderComplexityColors[ColorIndex - 1] : FLinearColor::Black);

					MeshTools.MaterialRenderProxy = LayerDensityMaterialInstance;
					Collector.RegisterOneFrameMaterialProxy(LayerDensityMaterialInstance);

					MeshTools.bCanApplyViewModeOverrides = true;
					MeshTools.bUseWireframeSelectionColoring = IsSelected();

					Collector.AddMesh(ViewIndex, MeshTools);

					NumPasses++;
					NumTriangles += MeshTools.GetNumPrimitives();
					NumDrawCalls += MeshTools.Elements.Num();
				}
				break;

			case ELandscapeViewMode::LayerUsage:
				if (GLandscapeLayerUsageMaterial)
				{
					float Rotation = ((SectionBase.X / ComponentSizeQuads) ^ (SectionBase.Y / ComponentSizeQuads)) & 1 ? 0 : 2.f * PI;
					auto LayerUsageMaterialInstance = new FLandscapeLayerUsageRenderProxy(GLandscapeLayerUsageMaterial->GetRenderProxy(false), ComponentSizeVerts, LayerColors, Rotation);
					MeshTools.MaterialRenderProxy = LayerUsageMaterialInstance;
					Collector.RegisterOneFrameMaterialProxy(LayerUsageMaterialInstance);
					MeshTools.bCanApplyViewModeOverrides = true;
					MeshTools.bUseWireframeSelectionColoring = IsSelected();
					Collector.AddMesh(ViewIndex, MeshTools);
					NumPasses++;
					NumTriangles += MeshTools.GetNumPrimitives();
					NumDrawCalls += MeshTools.Elements.Num();
				}
				break;

			case ELandscapeViewMode::LOD:
			{
				auto& TemplateMesh = bIsWireframe ? Mesh : MeshTools;
				for (int32 i = 0; i < TemplateMesh.Elements.Num(); i++)
				{
					FMeshBatch& LODMesh = Collector.AllocateMesh();
					LODMesh = TemplateMesh;
					LODMesh.Elements.Empty(1);
					LODMesh.Elements.Add(TemplateMesh.Elements[i]);
					int32 CurrentLOD = ((FLandscapeBatchElementParams*)TemplateMesh.Elements[i].UserData)->CurrentLOD;
					LODMesh.VisualizeLODIndex = CurrentLOD;
					FLinearColor Color = GetColorForLod(CurrentLOD, ForcedLOD);
					FMaterialRenderProxy* LODMaterialProxy =
						bMaterialModifiesMeshPosition && bIsWireframe
						? (FMaterialRenderProxy*)new FOverrideSelectionColorMaterialRenderProxy(MaterialInterface->GetRenderProxy(false), Color)
						: (FMaterialRenderProxy*)new FColoredMaterialRenderProxy(GEngine->LevelColorationUnlitMaterial->GetRenderProxy(false), Color);
					Collector.RegisterOneFrameMaterialProxy(LODMaterialProxy);
					LODMesh.MaterialRenderProxy = LODMaterialProxy;
					LODMesh.bCanApplyViewModeOverrides = !bIsWireframe;
					LODMesh.bWireframe = bIsWireframe;
					LODMesh.bUseWireframeSelectionColoring = IsSelected();
					Collector.AddMesh(ViewIndex, LODMesh);

					NumPasses++;
					NumTriangles += TemplateMesh.Elements[i].NumPrimitives;
					NumDrawCalls++;
				}
			}
			break;

			case ELandscapeViewMode::WireframeOnTop:
			{
				// base mesh
				Mesh.MaterialRenderProxy = MaterialInterface->GetRenderProxy(false);
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh(ViewIndex, Mesh);
				NumPasses++;
				NumTriangles += Mesh.GetNumPrimitives();
				NumDrawCalls += Mesh.Elements.Num();

				// wireframe on top
				FMeshBatch& WireMesh = Collector.AllocateMesh();
				WireMesh = MeshTools;
				auto WireMaterialInstance = new FColoredMaterialRenderProxy(GEngine->LevelColorationUnlitMaterial->GetRenderProxy(false), FLinearColor(0, 0, 1));
				WireMesh.MaterialRenderProxy = WireMaterialInstance;
				Collector.RegisterOneFrameMaterialProxy(WireMaterialInstance);
				WireMesh.bCanApplyViewModeOverrides = false;
				WireMesh.bWireframe = true;
				Collector.AddMesh(ViewIndex, WireMesh);
				NumPasses++;
				NumTriangles += WireMesh.GetNumPrimitives();
				NumDrawCalls++;
			}
			break;

			default:

#endif // WITH_EDITOR

#if WITH_EDITOR || !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (AllowDebugViewmodes() && bInCollisionView)
				{
					if (bDrawSimpleCollision || bDrawComplexCollision)
					{
						// Override the mesh's material with our material that draws the collision color
						auto CollisionMaterialInstance = new FColoredMaterialRenderProxy(
							GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy(IsSelected(), IsHovered()),
							WireframeColor
							);
						Collector.RegisterOneFrameMaterialProxy(CollisionMaterialInstance);

						Mesh.MaterialRenderProxy = CollisionMaterialInstance;
						Mesh.bCanApplyViewModeOverrides = true;
						Mesh.bUseWireframeSelectionColoring = IsSelected();

						Collector.AddMesh(ViewIndex, Mesh);

						NumPasses++;
						NumTriangles += Mesh.GetNumPrimitives();
						NumDrawCalls += Mesh.Elements.Num();
					}
				}
				else
#endif
				// Regular Landscape rendering. Only use the dynamic path if we're rendering a rich view or we've disabled the static path for debugging.
				if( IsRichView(ViewFamily) || 
					GLandscapeDebugOptions.bDisableStatic ||
					bIsWireframe ||
#if WITH_EDITOR
					(IsSelected() && !GLandscapeEditModeActive) ||
#else
					IsSelected() ||
#endif
					!IsStaticPathAvailable())
				{
					Mesh.MaterialRenderProxy = MaterialInterface->GetRenderProxy(false);

					Mesh.bCanApplyViewModeOverrides = true;
					Mesh.bUseWireframeSelectionColoring = IsSelected();

					Collector.AddMesh(ViewIndex, Mesh);

					NumPasses++;
					NumTriangles += Mesh.GetNumPrimitives();
					NumDrawCalls += Mesh.Elements.Num();
				}

#if WITH_EDITOR
			} // switch
#endif

#if WITH_EDITOR
			// Extra render passes for landscape tools
			if (GLandscapeEditModeActive)
			{
				// Region selection
				if (EditToolRenderData.SelectedType)
				{
					if ((GLandscapeEditRenderMode & ELandscapeEditRenderMode::SelectRegion) && (EditToolRenderData.SelectedType & FLandscapeEditToolRenderData::ST_REGION)
						&& !(GLandscapeEditRenderMode & ELandscapeEditRenderMode::Mask))
					{
						FMeshBatch& SelectMesh = Collector.AllocateMesh();
						SelectMesh = MeshTools;
						auto SelectMaterialInstance = new FLandscapeSelectMaterialRenderProxy(GSelectionRegionMaterial->GetRenderProxy(false), EditToolRenderData.DataTexture ? EditToolRenderData.DataTexture : GLandscapeBlackTexture);
						SelectMesh.MaterialRenderProxy = SelectMaterialInstance;
						Collector.RegisterOneFrameMaterialProxy(SelectMaterialInstance);
						Collector.AddMesh(ViewIndex, SelectMesh);
						NumPasses++;
						NumTriangles += SelectMesh.GetNumPrimitives();
						NumDrawCalls += SelectMesh.Elements.Num();
					}

					if ((GLandscapeEditRenderMode & ELandscapeEditRenderMode::SelectComponent) && (EditToolRenderData.SelectedType & FLandscapeEditToolRenderData::ST_COMPONENT))
					{
						FMeshBatch& SelectMesh = Collector.AllocateMesh();
						SelectMesh = MeshTools;
						SelectMesh.MaterialRenderProxy = GSelectionColorMaterial->GetRenderProxy(0);
						Collector.AddMesh(ViewIndex, SelectMesh);
						NumPasses++;
						NumTriangles += SelectMesh.GetNumPrimitives();
						NumDrawCalls += SelectMesh.Elements.Num();
					}
				}

				// Mask
				if ((GLandscapeEditRenderMode & ELandscapeEditRenderMode::SelectRegion) && (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Mask))
				{
					if (EditToolRenderData.SelectedType & FLandscapeEditToolRenderData::ST_REGION)
					{
						FMeshBatch& MaskMesh = Collector.AllocateMesh();
						MaskMesh = MeshTools;
						auto MaskMaterialInstance = new FLandscapeMaskMaterialRenderProxy(GMaskRegionMaterial->GetRenderProxy(false), EditToolRenderData.DataTexture ? EditToolRenderData.DataTexture : GLandscapeBlackTexture, !!(GLandscapeEditRenderMode & ELandscapeEditRenderMode::InvertedMask));
						MaskMesh.MaterialRenderProxy = MaskMaterialInstance;
						Collector.RegisterOneFrameMaterialProxy(MaskMaterialInstance);
						Collector.AddMesh(ViewIndex, MaskMesh);
						NumPasses++;
						NumTriangles += MaskMesh.GetNumPrimitives();
						NumDrawCalls += MaskMesh.Elements.Num();
					}
					else if (!(GLandscapeEditRenderMode & ELandscapeEditRenderMode::InvertedMask))
					{
						FMeshBatch& MaskMesh = Collector.AllocateMesh();
						MaskMesh = MeshTools;
						auto MaskMaterialInstance = new FLandscapeMaskMaterialRenderProxy(GMaskRegionMaterial->GetRenderProxy(false), GLandscapeBlackTexture, false);
						MaskMesh.MaterialRenderProxy = MaskMaterialInstance;
						Collector.RegisterOneFrameMaterialProxy(MaskMaterialInstance);
						Collector.AddMesh(ViewIndex, MaskMesh);
						NumPasses++;
						NumTriangles += MaskMesh.GetNumPrimitives();
						NumDrawCalls += MaskMesh.Elements.Num();
					}
				}

				// Edit mode tools
				if (EditToolRenderData.ToolMaterial)
				{
					FMeshBatch& EditMesh = Collector.AllocateMesh();
					EditMesh = MeshTools;
					EditMesh.MaterialRenderProxy = EditToolRenderData.ToolMaterial->GetRenderProxy(0);
					Collector.AddMesh(ViewIndex, EditMesh);
					NumPasses++;
					NumTriangles += EditMesh.GetNumPrimitives();
					NumDrawCalls += EditMesh.Elements.Num();
				}

				if (EditToolRenderData.GizmoMaterial && GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo)
				{
					FMeshBatch& EditMesh = Collector.AllocateMesh();
					EditMesh = MeshTools;
					EditMesh.MaterialRenderProxy = EditToolRenderData.GizmoMaterial->GetRenderProxy(0);
					Collector.AddMesh(ViewIndex, EditMesh);
					NumPasses++;
					NumTriangles += EditMesh.GetNumPrimitives();
					NumDrawCalls += EditMesh.Elements.Num();
				}
			}
#endif // WITH_EDITOR

			if (GLandscapeDebugOptions.bShowPatches)
			{
				DrawWireBox(Collector.GetPDI(ViewIndex), GetBounds().GetBox(), FColor(255, 255, 0), SDPG_World);
			}

			RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
		}
	}

	INC_DWORD_STAT_BY(STAT_LandscapeComponents, NumPasses);
	INC_DWORD_STAT_BY(STAT_LandscapeDrawCalls, NumDrawCalls);
	INC_DWORD_STAT_BY(STAT_LandscapeTriangles, NumTriangles * NumPasses);
}


//
// FLandscapeVertexBuffer
//

/**
* Initialize the RHI for this rendering resource
*/
void FLandscapeVertexBuffer::InitRHI()
{
	// create a static vertex buffer
	FRHIResourceCreateInfo CreateInfo;
	void* BufferData = nullptr;
	VertexBufferRHI = RHICreateAndLockVertexBuffer(NumVertices * sizeof(FLandscapeVertex), BUF_Static, CreateInfo, BufferData);
	FLandscapeVertex* Vertex = (FLandscapeVertex*)BufferData;
	int32 VertexIndex = 0;
	for (int32 SubY = 0; SubY < NumSubsections; SubY++)
	{
		for (int32 SubX = 0; SubX < NumSubsections; SubX++)
		{
			for (int32 y = 0; y < SubsectionSizeVerts; y++)
			{
				for (int32 x = 0; x < SubsectionSizeVerts; x++)
				{
					Vertex->VertexX = x;
					Vertex->VertexY = y;
					Vertex->SubX = SubX;
					Vertex->SubY = SubY;
					Vertex++;
					VertexIndex++;
				}
			}
		}
	}
	check(NumVertices == VertexIndex);
	RHIUnlockVertexBuffer(VertexBufferRHI);
}

//
// FLandscapeSharedBuffers
//

template <typename INDEX_TYPE>
void FLandscapeSharedBuffers::CreateIndexBuffers(ERHIFeatureLevel::Type InFeatureLevel, bool bRequiresAdjacencyInformation)
{
	if (InFeatureLevel <= ERHIFeatureLevel::ES3_1)
	{
		if (!bVertexScoresComputed)
		{
			bVertexScoresComputed = ComputeVertexScores();
		}
	}

	TMap<uint64, INDEX_TYPE> VertexMap;
	INDEX_TYPE VertexCount = 0;
	int32 SubsectionSizeQuads = SubsectionSizeVerts - 1;

	// Layout index buffer to determine best vertex order
	int32 MaxLOD = NumIndexBuffers - 1;
	for (int32 Mip = MaxLOD; Mip >= 0; Mip--)
	{
		int32 LodSubsectionSizeQuads = (SubsectionSizeVerts >> Mip) - 1;

		TArray<INDEX_TYPE> NewIndices;
		int32 ExpectedNumIndices = FMath::Square(NumSubsections) * FMath::Square(LodSubsectionSizeQuads) * 6;
		NewIndices.Empty(ExpectedNumIndices);

		int32& MaxIndexFull = IndexRanges[Mip].MaxIndexFull;
		int32& MinIndexFull = IndexRanges[Mip].MinIndexFull;
		MaxIndexFull = 0;
		MinIndexFull = MAX_int32;

		if (InFeatureLevel <= ERHIFeatureLevel::ES3_1)
		{
			// ES2 version
			float MipRatio = (float)SubsectionSizeQuads / (float)LodSubsectionSizeQuads; // Morph current MIP to base MIP

			for (int32 SubY = 0; SubY < NumSubsections; SubY++)
			{
				for (int32 SubX = 0; SubX < NumSubsections; SubX++)
				{
					TArray<INDEX_TYPE> SubIndices;
					SubIndices.Empty(FMath::Square(LodSubsectionSizeQuads) * 6);

					int32& MaxIndex = IndexRanges[Mip].MaxIndex[SubX][SubY];
					int32& MinIndex = IndexRanges[Mip].MinIndex[SubX][SubY];
					MaxIndex = 0;
					MinIndex = MAX_int32;

					for (int32 y = 0; y < LodSubsectionSizeQuads; y++)
					{
						for (int32 x = 0; x < LodSubsectionSizeQuads; x++)
						{
							int32 x0 = FMath::RoundToInt((float)x * MipRatio);
							int32 y0 = FMath::RoundToInt((float)y * MipRatio);
							int32 x1 = FMath::RoundToInt((float)(x + 1) * MipRatio);
							int32 y1 = FMath::RoundToInt((float)(y + 1) * MipRatio);

							FLandscapeVertexRef V00(x0, y0, SubX, SubY);
							FLandscapeVertexRef V10(x1, y0, SubX, SubY);
							FLandscapeVertexRef V11(x1, y1, SubX, SubY);
							FLandscapeVertexRef V01(x0, y1, SubX, SubY);

							uint64 Key00 = V00.MakeKey();
							uint64 Key10 = V10.MakeKey();
							uint64 Key11 = V11.MakeKey();
							uint64 Key01 = V01.MakeKey();

							INDEX_TYPE i00;
							INDEX_TYPE i10;
							INDEX_TYPE i11;
							INDEX_TYPE i01;

							INDEX_TYPE* KeyPtr = VertexMap.Find(Key00);
							if (KeyPtr == nullptr)
							{
								i00 = VertexCount++;
								VertexMap.Add(Key00, i00);
							}
							else
							{
								i00 = *KeyPtr;
							}

							KeyPtr = VertexMap.Find(Key10);
							if (KeyPtr == nullptr)
							{
								i10 = VertexCount++;
								VertexMap.Add(Key10, i10);
							}
							else
							{
								i10 = *KeyPtr;
							}

							KeyPtr = VertexMap.Find(Key11);
							if (KeyPtr == nullptr)
							{
								i11 = VertexCount++;
								VertexMap.Add(Key11, i11);
							}
							else
							{
								i11 = *KeyPtr;
							}

							KeyPtr = VertexMap.Find(Key01);
							if (KeyPtr == nullptr)
							{
								i01 = VertexCount++;
								VertexMap.Add(Key01, i01);
							}
							else
							{
								i01 = *KeyPtr;
							}

							// Update the min/max index ranges
							MaxIndex = FMath::Max<int32>(MaxIndex, i00);
							MinIndex = FMath::Min<int32>(MinIndex, i00);
							MaxIndex = FMath::Max<int32>(MaxIndex, i10);
							MinIndex = FMath::Min<int32>(MinIndex, i10);
							MaxIndex = FMath::Max<int32>(MaxIndex, i11);
							MinIndex = FMath::Min<int32>(MinIndex, i11);
							MaxIndex = FMath::Max<int32>(MaxIndex, i01);
							MinIndex = FMath::Min<int32>(MinIndex, i01);

							SubIndices.Add(i00);
							SubIndices.Add(i11);
							SubIndices.Add(i10);

							SubIndices.Add(i00);
							SubIndices.Add(i01);
							SubIndices.Add(i11);
						}
					}

					// update min/max for full subsection
					MaxIndexFull = FMath::Max<int32>(MaxIndexFull, MaxIndex);
					MinIndexFull = FMath::Min<int32>(MinIndexFull, MinIndex);

					TArray<INDEX_TYPE> NewSubIndices;
					::OptimizeFaces<INDEX_TYPE>(SubIndices, NewSubIndices, 32);
					NewIndices.Append(NewSubIndices);
				}
			}
		}
		else
		{
			// non-ES2 version
			int32 SubOffset = 0;
			for (int32 SubY = 0; SubY < NumSubsections; SubY++)
			{
				for (int32 SubX = 0; SubX < NumSubsections; SubX++)
				{
					int32& MaxIndex = IndexRanges[Mip].MaxIndex[SubX][SubY];
					int32& MinIndex = IndexRanges[Mip].MinIndex[SubX][SubY];
					MaxIndex = 0;
					MinIndex = MAX_int32;

					for (int32 y = 0; y < LodSubsectionSizeQuads; y++)
					{
						for (int32 x = 0; x < LodSubsectionSizeQuads; x++)
						{
							INDEX_TYPE i00 = (x + 0) + (y + 0) * SubsectionSizeVerts + SubOffset;
							INDEX_TYPE i10 = (x + 1) + (y + 0) * SubsectionSizeVerts + SubOffset;
							INDEX_TYPE i11 = (x + 1) + (y + 1) * SubsectionSizeVerts + SubOffset;
							INDEX_TYPE i01 = (x + 0) + (y + 1) * SubsectionSizeVerts + SubOffset;

							NewIndices.Add(i00);
							NewIndices.Add(i11);
							NewIndices.Add(i10);

							NewIndices.Add(i00);
							NewIndices.Add(i01);
							NewIndices.Add(i11);

							// Update the min/max index ranges
							MaxIndex = FMath::Max<int32>(MaxIndex, i00);
							MinIndex = FMath::Min<int32>(MinIndex, i00);
							MaxIndex = FMath::Max<int32>(MaxIndex, i10);
							MinIndex = FMath::Min<int32>(MinIndex, i10);
							MaxIndex = FMath::Max<int32>(MaxIndex, i11);
							MinIndex = FMath::Min<int32>(MinIndex, i11);
							MaxIndex = FMath::Max<int32>(MaxIndex, i01);
							MinIndex = FMath::Min<int32>(MinIndex, i01);
						}
					}

					// update min/max for full subsection
					MaxIndexFull = FMath::Max<int32>(MaxIndexFull, MaxIndex);
					MinIndexFull = FMath::Min<int32>(MinIndexFull, MinIndex);

					SubOffset += FMath::Square(SubsectionSizeVerts);
				}
			}

			check(MinIndexFull <= (uint32)((INDEX_TYPE)(~(INDEX_TYPE)0)));
			check(NewIndices.Num() == ExpectedNumIndices);
		}

		// Create and init new index buffer with index data
		FRawStaticIndexBuffer16or32<INDEX_TYPE>* IndexBuffer = (FRawStaticIndexBuffer16or32<INDEX_TYPE>*)IndexBuffers[Mip];
		if (!IndexBuffer)
		{
			IndexBuffer = new FRawStaticIndexBuffer16or32<INDEX_TYPE>(false);
		}
		IndexBuffer->AssignNewBuffer(NewIndices);

		// Delay init resource to keep CPU data until create AdjacencyIndexbuffers
		if (!bRequiresAdjacencyInformation)
		{
			IndexBuffer->InitResource();
		}

		IndexBuffers[Mip] = IndexBuffer;
	}
}

#if WITH_EDITOR
template <typename INDEX_TYPE>
void FLandscapeSharedBuffers::CreateGrassIndexBuffer()
{
	TArray<INDEX_TYPE> NewIndices;

	int32 ExpectedNumIndices = FMath::Square(NumSubsections) * (FMath::Square(SubsectionSizeVerts) * 4/3 - 1); // *4/3 is for mips, -1 because we only go down to 2x2 not 1x1
	NewIndices.Empty(ExpectedNumIndices);

	int32 NumMips = FMath::CeilLogTwo(SubsectionSizeVerts);

	for (int32 Mip = 0; Mip < NumMips; ++Mip)
	{
		// Store offset to the start of this mip in the index buffer
		GrassIndexMipOffsets.Add(NewIndices.Num());

		int32 MipSubsectionSizeVerts = SubsectionSizeVerts >> Mip;
		int32 SubOffset = 0;
		for (int32 SubY = 0; SubY < NumSubsections; SubY++)
		{
			for (int32 SubX = 0; SubX < NumSubsections; SubX++)
			{
				for (int32 y = 0; y < MipSubsectionSizeVerts; y++)
				{
					for (int32 x = 0; x < MipSubsectionSizeVerts; x++)
					{
						// intentionally using SubsectionSizeVerts not MipSubsectionSizeVerts, this is a vert buffer index not a mip vert index
						NewIndices.Add(x + y * SubsectionSizeVerts + SubOffset);
					}
				}

				// intentionally using SubsectionSizeVerts not MipSubsectionSizeVerts (as above)
				SubOffset += FMath::Square(SubsectionSizeVerts);
			}
		}
	}

	check(NewIndices.Num() == ExpectedNumIndices);

	// Create and init new index buffer with index data
	FRawStaticIndexBuffer16or32<INDEX_TYPE>* IndexBuffer = new FRawStaticIndexBuffer16or32<INDEX_TYPE>(false);
	IndexBuffer->AssignNewBuffer(NewIndices);
	IndexBuffer->InitResource();
	GrassIndexBuffer = IndexBuffer;
}
#endif

FLandscapeSharedBuffers::FLandscapeSharedBuffers(const int32 InSharedBuffersKey, const int32 InSubsectionSizeQuads, const int32 InNumSubsections, const ERHIFeatureLevel::Type InFeatureLevel, const bool bRequiresAdjacencyInformation)
	: SharedBuffersKey(InSharedBuffersKey)
	, NumIndexBuffers(FMath::CeilLogTwo(InSubsectionSizeQuads + 1))
	, SubsectionSizeVerts(InSubsectionSizeQuads + 1)
	, NumSubsections(InNumSubsections)
	, VertexFactory(nullptr)
	, VertexBuffer(nullptr)
	, AdjacencyIndexBuffers(nullptr)
	, bUse32BitIndices(false)
#if WITH_EDITOR
	, GrassIndexBuffer(nullptr)
#endif
{
	NumVertices = FMath::Square(SubsectionSizeVerts) * FMath::Square(NumSubsections);
	if (InFeatureLevel > ERHIFeatureLevel::ES3_1)
	{
		// Vertex Buffer cannot be shared
		VertexBuffer = new FLandscapeVertexBuffer(InFeatureLevel, NumVertices, SubsectionSizeVerts, NumSubsections);
	}
	IndexBuffers = new FIndexBuffer*[NumIndexBuffers];
	FMemory::Memzero(IndexBuffers, sizeof(FIndexBuffer*)* NumIndexBuffers);
	IndexRanges = new FLandscapeIndexRanges[NumIndexBuffers]();

	// See if we need to use 16 or 32-bit index buffers
	if (NumVertices > 65535)
	{
		bUse32BitIndices = true;
		CreateIndexBuffers<uint32>(InFeatureLevel, bRequiresAdjacencyInformation);
#if WITH_EDITOR
		if (InFeatureLevel > ERHIFeatureLevel::ES3_1)
		{
			CreateGrassIndexBuffer<uint32>();
		}
#endif
	}
	else
	{
		CreateIndexBuffers<uint16>(InFeatureLevel, bRequiresAdjacencyInformation);
#if WITH_EDITOR
		if (InFeatureLevel > ERHIFeatureLevel::ES3_1)
		{
			CreateGrassIndexBuffer<uint16>();
		}
#endif
	}
}

FLandscapeSharedBuffers::~FLandscapeSharedBuffers()
{
	delete VertexBuffer;

	for (int32 i = 0; i < NumIndexBuffers; i++)
	{
		IndexBuffers[i]->ReleaseResource();
		delete IndexBuffers[i];
	}
	delete[] IndexBuffers;
	delete[] IndexRanges;

#if WITH_EDITOR
	if (GrassIndexBuffer)
	{
		GrassIndexBuffer->ReleaseResource();
		delete GrassIndexBuffer;
	}
#endif

	if (AdjacencyIndexBuffers)
	{
		if (AdjacencyIndexBuffers->Release() == 0)
		{
			FLandscapeComponentSceneProxy::SharedAdjacencyIndexBufferMap.Remove(SharedBuffersKey);
		}
		AdjacencyIndexBuffers = nullptr;
	}

	delete VertexFactory;
}

template<typename IndexType>
static void BuildLandscapeAdjacencyIndexBuffer(int32 LODSubsectionSizeQuads, int32 NumSubsections, const FRawStaticIndexBuffer16or32<IndexType>* Indices, TArray<IndexType>& OutPnAenIndices)
{
	if (Indices && Indices->Num())
	{
		// Landscape use regular grid, so only expand Index buffer works
		// PN AEN Dominant Corner
		uint32 TriCount = LODSubsectionSizeQuads*LODSubsectionSizeQuads * 2;

		uint32 ExpandedCount = 12 * TriCount * NumSubsections * NumSubsections;

		OutPnAenIndices.Empty(ExpandedCount);
		OutPnAenIndices.AddUninitialized(ExpandedCount);

		for (int32 SubY = 0; SubY < NumSubsections; SubY++)
		{
			for (int32 SubX = 0; SubX < NumSubsections; SubX++)
			{
				uint32 SubsectionTriIndex = (SubX + SubY * NumSubsections) * TriCount;

				for (uint32 TriIdx = SubsectionTriIndex; TriIdx < SubsectionTriIndex + TriCount; ++TriIdx)
				{
					uint32 OutStartIdx = TriIdx * 12;
					uint32 InStartIdx = TriIdx * 3;
					OutPnAenIndices[OutStartIdx + 0] = Indices->Get(InStartIdx + 0);
					OutPnAenIndices[OutStartIdx + 1] = Indices->Get(InStartIdx + 1);
					OutPnAenIndices[OutStartIdx + 2] = Indices->Get(InStartIdx + 2);

					OutPnAenIndices[OutStartIdx + 3] = Indices->Get(InStartIdx + 0);
					OutPnAenIndices[OutStartIdx + 4] = Indices->Get(InStartIdx + 1);
					OutPnAenIndices[OutStartIdx + 5] = Indices->Get(InStartIdx + 1);
					OutPnAenIndices[OutStartIdx + 6] = Indices->Get(InStartIdx + 2);
					OutPnAenIndices[OutStartIdx + 7] = Indices->Get(InStartIdx + 2);
					OutPnAenIndices[OutStartIdx + 8] = Indices->Get(InStartIdx + 0);

					OutPnAenIndices[OutStartIdx + 9] = Indices->Get(InStartIdx + 0);
					OutPnAenIndices[OutStartIdx + 10] = Indices->Get(InStartIdx + 1);
					OutPnAenIndices[OutStartIdx + 11] = Indices->Get(InStartIdx + 2);
				}
			}
		}
	}
	else
	{
		OutPnAenIndices.Empty();
	}
}


FLandscapeSharedAdjacencyIndexBuffer::FLandscapeSharedAdjacencyIndexBuffer(FLandscapeSharedBuffers* Buffers)
{
	check(Buffers && Buffers->IndexBuffers);

	// Currently only support PN-AEN-Dominant Corner, which is the only mode for UE4 for now
	IndexBuffers.Empty(Buffers->NumIndexBuffers);

	bool b32BitIndex = Buffers->NumVertices > 65535;
	for (int32 i = 0; i < Buffers->NumIndexBuffers; ++i)
	{
		if (b32BitIndex)
		{
			TArray<uint32> OutPnAenIndices;
			BuildLandscapeAdjacencyIndexBuffer<uint32>((Buffers->SubsectionSizeVerts >> i) - 1, Buffers->NumSubsections, (FRawStaticIndexBuffer16or32<uint32>*)Buffers->IndexBuffers[i], OutPnAenIndices);

			FRawStaticIndexBuffer16or32<uint32>* IndexBuffer = new FRawStaticIndexBuffer16or32<uint32>();
			IndexBuffer->AssignNewBuffer(OutPnAenIndices);
			IndexBuffers.Add(IndexBuffer);
		}
		else
		{
			TArray<uint16> OutPnAenIndices;
			BuildLandscapeAdjacencyIndexBuffer<uint16>((Buffers->SubsectionSizeVerts >> i) - 1, Buffers->NumSubsections, (FRawStaticIndexBuffer16or32<uint16>*)Buffers->IndexBuffers[i], OutPnAenIndices);

			FRawStaticIndexBuffer16or32<uint16>* IndexBuffer = new FRawStaticIndexBuffer16or32<uint16>();
			IndexBuffer->AssignNewBuffer(OutPnAenIndices);
			IndexBuffers.Add(IndexBuffer);
		}

		IndexBuffers[i]->InitResource();
	}
}

FLandscapeSharedAdjacencyIndexBuffer::~FLandscapeSharedAdjacencyIndexBuffer()
{
	for (int32 i = 0; i < IndexBuffers.Num(); ++i)
	{
		IndexBuffers[i]->ReleaseResource();
		delete IndexBuffers[i];
	}
}

//
// FLandscapeVertexFactoryVertexShaderParameters
//

/** Shader parameters for use with FLandscapeVertexFactory */
class FLandscapeVertexFactoryVertexShaderParameters : public FVertexFactoryShaderParameters
{
public:
	/**
	* Bind shader constants by name
	* @param	ParameterMap - mapping of named shader constants to indices
	*/
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		HeightmapTextureParameter.Bind(ParameterMap, TEXT("HeightmapTexture"));
		HeightmapTextureParameterSampler.Bind(ParameterMap, TEXT("HeightmapTextureSampler"));
		LodValuesParameter.Bind(ParameterMap, TEXT("LodValues"));
		NeighborSectionLodParameter.Bind(ParameterMap, TEXT("NeighborSectionLod"));
		LodBiasParameter.Bind(ParameterMap, TEXT("LodBias"));
		SectionLodsParameter.Bind(ParameterMap, TEXT("SectionLods"));
		XYOffsetTextureParameter.Bind(ParameterMap, TEXT("XYOffsetmapTexture"));
		XYOffsetTextureParameterSampler.Bind(ParameterMap, TEXT("XYOffsetmapTextureSampler"));
	}

	/**
	* Serialize shader params to an archive
	* @param	Ar - archive to serialize to
	*/
	virtual void Serialize(FArchive& Ar) override
	{
		Ar << HeightmapTextureParameter;
		Ar << HeightmapTextureParameterSampler;
		Ar << LodValuesParameter;
		Ar << NeighborSectionLodParameter;
		Ar << LodBiasParameter;
		Ar << SectionLodsParameter;
		Ar << XYOffsetTextureParameter;
		Ar << XYOffsetTextureParameterSampler;
	}

	/**
	* Set any shader data specific to this vertex factory
	*/
	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* VertexShader, const class FVertexFactory* VertexFactory, const class FSceneView& View, const struct FMeshBatchElement& BatchElement, uint32 DataFlags) const override
	{
		SCOPE_CYCLE_COUNTER(STAT_LandscapeVFDrawTime);

		const FLandscapeBatchElementParams* BatchElementParams = (const FLandscapeBatchElementParams*)BatchElement.UserData;
		check(BatchElementParams);

		const FLandscapeComponentSceneProxy* SceneProxy = BatchElementParams->SceneProxy;
		SetUniformBufferParameter(RHICmdList, VertexShader->GetVertexShader(), VertexShader->GetUniformBufferParameter<FLandscapeUniformShaderParameters>(), *BatchElementParams->LandscapeUniformShaderParametersResource);

		if (HeightmapTextureParameter.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				VertexShader->GetVertexShader(),
				HeightmapTextureParameter,
				HeightmapTextureParameterSampler,
				TStaticSamplerState<SF_Point>::GetRHI(),
				SceneProxy->HeightmapTexture->Resource->TextureRHI
				);
		}

		if (LodBiasParameter.IsBound())
		{
			FVector4 LodBias(
				0.0f, // unused
				0.0f, // unused
				((FTexture2DResource*)SceneProxy->HeightmapTexture->Resource)->GetCurrentFirstMip(),
				SceneProxy->XYOffsetmapTexture ? ((FTexture2DResource*)SceneProxy->XYOffsetmapTexture->Resource)->GetCurrentFirstMip() : 0.0f
				);
			SetShaderValue(RHICmdList, VertexShader->GetVertexShader(), LodBiasParameter, LodBias);
		}

		// Calculate LOD params
		FVector CameraLocalPos3D = SceneProxy->WorldToLocal.TransformPosition(View.ViewMatrices.GetViewOrigin());
		FVector2D CameraLocalPos = FVector2D(CameraLocalPos3D.X, CameraLocalPos3D.Y);

		FVector4 fCurrentLODs;
		FVector4 CurrentNeighborLODs[4];

		if (BatchElementParams->SubX == -1)
		{
			for (int32 SubY = 0; SubY < SceneProxy->NumSubsections; SubY++)
			{
				for (int32 SubX = 0; SubX < SceneProxy->NumSubsections; SubX++)
				{
					int32 SubIndex = SubX + 2 * SubY;
					SceneProxy->CalcLODParamsForSubsection(View, CameraLocalPos, SubX, SubY, BatchElementParams->CurrentLOD, fCurrentLODs[SubIndex], CurrentNeighborLODs[SubIndex]);
				}
			}
		}
		else
		{
			int32 SubIndex = BatchElementParams->SubX + 2 * BatchElementParams->SubY;
			SceneProxy->CalcLODParamsForSubsection(View, CameraLocalPos, BatchElementParams->SubX, BatchElementParams->SubY, BatchElementParams->CurrentLOD, fCurrentLODs[SubIndex], CurrentNeighborLODs[SubIndex]);
		}

		if (SectionLodsParameter.IsBound())
		{
			SetShaderValue(RHICmdList, VertexShader->GetVertexShader(), SectionLodsParameter, fCurrentLODs);
		}

		if (NeighborSectionLodParameter.IsBound())
		{
			SetShaderValue(RHICmdList, VertexShader->GetVertexShader(), NeighborSectionLodParameter, CurrentNeighborLODs);
		}

		if (LodValuesParameter.IsBound())
		{
			FVector4 LodValues(
				BatchElementParams->CurrentLOD,
				0.0f, // unused
				(float)((SceneProxy->SubsectionSizeVerts >> BatchElementParams->CurrentLOD) - 1),
				1.f / (float)((SceneProxy->SubsectionSizeVerts >> BatchElementParams->CurrentLOD) - 1));

			SetShaderValue(RHICmdList, VertexShader->GetVertexShader(), LodValuesParameter, LodValues);
		}

		if (XYOffsetTextureParameter.IsBound() && SceneProxy->XYOffsetmapTexture)
		{
			SetTextureParameter(
				RHICmdList,
				VertexShader->GetVertexShader(),
				XYOffsetTextureParameter,
				XYOffsetTextureParameterSampler,
				TStaticSamplerState<SF_Point>::GetRHI(),
				SceneProxy->XYOffsetmapTexture->Resource->TextureRHI
				);
		}
	}

	virtual uint32 GetSize() const override
	{
		return sizeof(*this);
	}

protected:
	FShaderParameter LodValuesParameter;
	FShaderParameter NeighborSectionLodParameter;
	FShaderParameter LodBiasParameter;
	FShaderParameter SectionLodsParameter;
	FShaderResourceParameter HeightmapTextureParameter;
	FShaderResourceParameter HeightmapTextureParameterSampler;
	FShaderResourceParameter XYOffsetTextureParameter;
	FShaderResourceParameter XYOffsetTextureParameterSampler;
	TShaderUniformBufferParameter<FLandscapeUniformShaderParameters> LandscapeShaderParameters;
};

//
// FLandscapeVertexFactoryPixelShaderParameters
//
/**
* Bind shader constants by name
* @param	ParameterMap - mapping of named shader constants to indices
*/
void FLandscapeVertexFactoryPixelShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	NormalmapTextureParameter.Bind(ParameterMap, TEXT("NormalmapTexture"));
	NormalmapTextureParameterSampler.Bind(ParameterMap, TEXT("NormalmapTextureSampler"));
	LocalToWorldNoScalingParameter.Bind(ParameterMap, TEXT("LocalToWorldNoScaling"));
}

/**
* Serialize shader params to an archive
* @param	Ar - archive to serialize to
*/
void FLandscapeVertexFactoryPixelShaderParameters::Serialize(FArchive& Ar)
{
	Ar << NormalmapTextureParameter
	   << NormalmapTextureParameterSampler
	   << LocalToWorldNoScalingParameter;
}

/**
* Set any shader data specific to this vertex factory
*/
void FLandscapeVertexFactoryPixelShaderParameters::SetMesh(FRHICommandList& RHICmdList, FShader* PixelShader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const
{
	SCOPE_CYCLE_COUNTER(STAT_LandscapeVFDrawTime);

	const FLandscapeBatchElementParams* BatchElementParams = (const FLandscapeBatchElementParams*)BatchElement.UserData;

	if (LocalToWorldNoScalingParameter.IsBound())
	{
		SetShaderValue(RHICmdList, PixelShader->GetPixelShader(), LocalToWorldNoScalingParameter, *BatchElementParams->LocalToWorldNoScalingPtr);
	}

	if (NormalmapTextureParameter.IsBound())
	{
		SetTextureParameter(
			RHICmdList,
			PixelShader->GetPixelShader(),
			NormalmapTextureParameter,
			NormalmapTextureParameterSampler,
			BatchElementParams->SceneProxy->NormalmapTexture->Resource);
	}
}


//
// FLandscapeVertexFactory
//

void FLandscapeVertexFactory::InitRHI()
{
	// list of declaration items
	FVertexDeclarationElementList Elements;

	// position decls
	Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));

	// create the actual device decls
	InitDeclaration(Elements);
}

FVertexFactoryShaderParameters* FLandscapeVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	switch (ShaderFrequency)
	{
	case SF_Vertex:
		return new FLandscapeVertexFactoryVertexShaderParameters();
		break;
	case SF_Pixel:
		return new FLandscapeVertexFactoryPixelShaderParameters();
		break;
	default:
		return nullptr;
	}
}

void FLandscapeVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
}


IMPLEMENT_VERTEX_FACTORY_TYPE(FLandscapeVertexFactory, "/Engine/Private/LandscapeVertexFactory.ush", true, true, true, false, false);

/**
* Copy the data from another vertex factory
* @param Other - factory to copy from
*/
void FLandscapeVertexFactory::Copy(const FLandscapeVertexFactory& Other)
{
	//SetSceneProxy(Other.Proxy());
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FLandscapeVertexFactoryCopyData,
		FLandscapeVertexFactory*, VertexFactory, this,
		const FDataType*, DataCopy, &Other.Data,
		{
			VertexFactory->Data = *DataCopy;
		});
	BeginUpdateResourceRHI(this);
}

//
// FLandscapeXYOffsetVertexFactory
//

void FLandscapeXYOffsetVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FLandscapeVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("LANDSCAPE_XYOFFSET"), TEXT("1"));
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FLandscapeXYOffsetVertexFactory, "/Engine/Private/LandscapeVertexFactory.ush", true, true, true, false, false);

/** ULandscapeMaterialInstanceConstant */
ULandscapeMaterialInstanceConstant::ULandscapeMaterialInstanceConstant(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsLayerThumbnail = false;
}

class FLandscapeMaterialResource : public FMaterialResource
{
	const bool bIsLayerThumbnail;
	const bool bDisableTessellation;

public:
	FLandscapeMaterialResource(ULandscapeMaterialInstanceConstant* Parent)
		: bIsLayerThumbnail(Parent->bIsLayerThumbnail)
		, bDisableTessellation(Parent->bDisableTessellation)
	{
	}

	void GetShaderMapId(EShaderPlatform Platform, FMaterialShaderMapId& OutId) const override
	{
		FMaterialResource::GetShaderMapId(Platform, OutId);

		if (bIsLayerThumbnail || bDisableTessellation)
		{
			FSHA1 Hash;
			Hash.Update(OutId.BasePropertyOverridesHash.Hash, ARRAY_COUNT(OutId.BasePropertyOverridesHash.Hash));

			const FString HashString = TEXT("bOverride_TessellationMode");
			Hash.UpdateWithString(*HashString, HashString.Len());

			Hash.Final();
			Hash.GetHash(OutId.BasePropertyOverridesHash.Hash);
		}
	}

	bool IsUsedWithLandscape() const override
	{
		return !bIsLayerThumbnail;
	}

	bool IsUsedWithStaticLighting() const override
	{
		if (bIsLayerThumbnail)
		{
			return false;
		}
		return FMaterialResource::IsUsedWithStaticLighting();
	}

	bool IsUsedWithSkeletalMesh()          const override { return false; }
	bool IsUsedWithParticleSystem()        const override { return false; }
	bool IsUsedWithParticleSprites()       const override { return false; }
	bool IsUsedWithBeamTrails()            const override { return false; }
	bool IsUsedWithMeshParticles()         const override { return false; }
	bool IsUsedWithNiagaraSprites()       const override { return false; }
	bool IsUsedWithNiagaraRibbons()       const override { return false; }
	bool IsUsedWithNiagaraMeshParticles()       const override { return false; }
	bool IsUsedWithMorphTargets()          const override { return false; }
	bool IsUsedWithSplineMeshes()          const override { return false; }
	bool IsUsedWithInstancedStaticMeshes() const override { return false; }
	bool IsUsedWithAPEXCloth()             const override { return false; }
	EMaterialTessellationMode GetTessellationMode() const override { return (bIsLayerThumbnail || bDisableTessellation) ? MTM_NoTessellation : FMaterialResource::GetTessellationMode(); };

	bool ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const override
	{
		if (VertexFactoryType)
		{
			// Always check against FLocalVertexFactory in editor builds as it is required to render thumbnails
			// Thumbnail MICs are only rendered in the preview scene using a simple LocalVertexFactory
			static const FName LocalVertexFactory = FName(TEXT("FLocalVertexFactory"));
			if (VertexFactoryType->GetFName() == LocalVertexFactory)
			{
				if (Algo::Find(GetAllowedShaderTypes(), ShaderType->GetFName()))
				{
					return FMaterialResource::ShouldCache(Platform, ShaderType, VertexFactoryType);
				}
				else
				{
					if (Algo::Find(GetExcludedShaderTypes(), ShaderType->GetFName()))
					{
						UE_LOG(LogLandscape, VeryVerbose, TEXT("Excluding shader %s from landscape thumbnail material"), ShaderType->GetName());
						return false;
					}
					else
					{
						UE_LOG(LogLandscape, Warning, TEXT("Shader %s unknown by landscape thumbnail material, please add to either AllowedShaderTypes or ExcludedShaderTypes"), ShaderType->GetName());
						return FMaterialResource::ShouldCache(Platform, ShaderType, VertexFactoryType);
					}
				}
			}

			if (!bIsLayerThumbnail)
			{
				// Landscape MICs are only for use with the Landscape vertex factories
				// Todo: only compile LandscapeXYOffsetVertexFactory if we are using it
				static const FName LandscapeVertexFactory = FName(TEXT("FLandscapeVertexFactory"));
				static const FName LandscapeXYOffsetVertexFactory = FName(TEXT("FLandscapeXYOffsetVertexFactory"));
				static const FName LandscapeVertexFactoryMobile = FName(TEXT("FLandscapeVertexFactoryMobile"));
				if (VertexFactoryType->GetFName() == LandscapeVertexFactory ||
					VertexFactoryType->GetFName() == LandscapeXYOffsetVertexFactory ||
					VertexFactoryType->GetFName() == LandscapeVertexFactoryMobile)
				{
					return FMaterialResource::ShouldCache(Platform, ShaderType, VertexFactoryType);
				}
			}
		}

		return false;
	}

	static const TArray<FName>& GetAllowedShaderTypes()
	{
		// reduce the number of shaders compiled for the thumbnail materials by only compiling with shader types known to be used by the preview scene
		static const TArray<FName> AllowedShaderTypes =
		{
			FName(TEXT("TBasePassVSFNoLightMapPolicy")),
			FName(TEXT("TBasePassPSFNoLightMapPolicy")),
			FName(TEXT("TBasePassVSFCachedPointIndirectLightingPolicy")),
			FName(TEXT("TBasePassPSFCachedPointIndirectLightingPolicy")),
			FName(TEXT("TShadowDepthVSVertexShadowDepth_OutputDepthfalse")),
			FName(TEXT("TShadowDepthVSVertexShadowDepth_OutputDepthtrue")), // used by LPV
			FName(TEXT("TShadowDepthPSPixelShadowDepth_NonPerspectiveCorrectfalse")),
			FName(TEXT("TShadowDepthPSPixelShadowDepth_NonPerspectiveCorrecttrue")), // used by LPV
			FName(TEXT("TBasePassPSFSimpleDirectionalLightLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleDirectionalLightLightingPolicySkylight")),
			FName(TEXT("TBasePassVSFSimpleDirectionalLightLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleNoLightmapLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleNoLightmapLightingPolicySkylight")),
			FName(TEXT("TBasePassVSFSimpleNoLightmapLightingPolicy")),
			FName(TEXT("TDepthOnlyVS<false>")),
			FName(TEXT("TDepthOnlyVS<true>")),
			FName(TEXT("FDepthOnlyPS")),
			// UE-44519, masked material with landscape layers requires FHitProxy shaders.
			FName(TEXT("FHitProxyVS")),
			FName(TEXT("FHitProxyPS")),

			FName(TEXT("TBasePassVSFSimpleStationaryLightVolumetricLightmapShadowsLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleStationaryLightSingleSampleShadowsLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleStationaryLightSingleSampleShadowsLightingPolicySkylight")),
			FName(TEXT("TBasePassVSFSimpleStationaryLightSingleSampleShadowsLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleStationaryLightPrecomputedShadowsLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleStationaryLightPrecomputedShadowsLightingPolicySkylight")),
			FName(TEXT("TBasePassVSFSimpleStationaryLightPrecomputedShadowsLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleLightmapOnlyLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleLightmapOnlyLightingPolicySkylight")),
			FName(TEXT("TBasePassVSFSimpleLightmapOnlyLightingPolicy")),

			// Mobile
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMLightingPolicyINT32_MAXHDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMLightingPolicyINT32_MAXHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMLightingPolicy0HDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMLightingPolicy0HDRLinear64")),
			FName(TEXT("TMobileBasePassVSFMobileMovableDirectionalLightCSMLightingPolicyHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightLightingPolicyINT32_MAXHDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightLightingPolicyINT32_MAXHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightLightingPolicy0HDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightLightingPolicy0HDRLinear64")),
			FName(TEXT("TMobileBasePassVSFMobileMovableDirectionalLightLightingPolicyHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileDirectionalLightCSMAndSHIndirectPolicyINT32_MAXHDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileDirectionalLightCSMAndSHIndirectPolicyINT32_MAXHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileDirectionalLightCSMAndSHIndirectPolicy0HDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileDirectionalLightCSMAndSHIndirectPolicy0HDRLinear64")),
			FName(TEXT("TMobileBasePassVSFMobileDirectionalLightCSMAndSHIndirectPolicyHDRLinear64")),

			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMAndSHIndirectPolicyINT32_MAXHDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMAndSHIndirectPolicyINT32_MAXHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMAndSHIndirectPolicy0HDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMAndSHIndirectPolicy0HDRLinear64")),
			FName(TEXT("TMobileBasePassVSFMobileMovableDirectionalLightCSMAndSHIndirectPolicyHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightAndSHIndirectPolicyINT32_MAXHDRLinear64Skylight")),

			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightAndSHIndirectPolicyINT32_MAXHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightAndSHIndirectPolicy0HDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightAndSHIndirectPolicy0HDRLinear64")),
			FName(TEXT("TMobileBasePassVSFMobileMovableDirectionalLightAndSHIndirectPolicyHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileDirectionalLightAndSHIndirectPolicyINT32_MAXHDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileDirectionalLightAndSHIndirectPolicyINT32_MAXHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileDirectionalLightAndSHIndirectPolicy0HDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileDirectionalLightAndSHIndirectPolicy0HDRLinear64")),
			FName(TEXT("TMobileBasePassVSFMobileDirectionalLightAndSHIndirectPolicyHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFNoLightMapPolicyINT32_MAXHDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFNoLightMapPolicyINT32_MAXHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFNoLightMapPolicy0HDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFNoLightMapPolicy0HDRLinear64")),
			FName(TEXT("TMobileBasePassVSFNoLightMapPolicyHDRLinear64")),

			// Forward shading required
			FName(TEXT("TBasePassPSFCachedPointIndirectLightingPolicySkylight")),
			FName(TEXT("TBasePassPSFNoLightMapPolicySkylight")),
		};
		return AllowedShaderTypes;
	}

	static const TArray<FName>& GetExcludedShaderTypes()
	{
		// shader types known *not* to be used by the preview scene
		static const TArray<FName> ExcludedShaderTypes =
		{
			// This is not an exhaustive list
			FName(TEXT("FDebugViewModeVS")),
			FName(TEXT("FConvertToUniformMeshVS")),
			FName(TEXT("FConvertToUniformMeshGS")),
			FName(TEXT("FVelocityVS")),
			FName(TEXT("FVelocityPS")),

			// No lightmap on thumbnails
			FName(TEXT("TLightMapDensityVSFNoLightMapPolicy")),
			FName(TEXT("TLightMapDensityPSFNoLightMapPolicy")),
			FName(TEXT("TLightMapDensityVSFDummyLightMapPolicy")),
			FName(TEXT("TLightMapDensityPSFDummyLightMapPolicy")),
			FName(TEXT("TLightMapDensityPSTLightMapPolicyHQ")),
			FName(TEXT("TLightMapDensityVSTLightMapPolicyHQ")),
			FName(TEXT("TLightMapDensityPSTLightMapPolicyLQ")),
			FName(TEXT("TLightMapDensityVSTLightMapPolicyLQ")),
			FName(TEXT("TBasePassPSTDistanceFieldShadowsAndLightMapPolicyHQ")),
			FName(TEXT("TBasePassPSTDistanceFieldShadowsAndLightMapPolicyHQSkylight")),
			FName(TEXT("TBasePassVSTDistanceFieldShadowsAndLightMapPolicyHQ")),
			FName(TEXT("TBasePassPSTLightMapPolicyHQ")),
			FName(TEXT("TBasePassPSTLightMapPolicyHQSkylight")),
			FName(TEXT("TBasePassVSTLightMapPolicyHQ")),
			FName(TEXT("TBasePassPSTLightMapPolicyLQ")),
			FName(TEXT("TBasePassPSTLightMapPolicyLQSkylight")),
			FName(TEXT("TBasePassVSTLightMapPolicyLQ")),

			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMWithLightmapPolicyINT32_MAXHDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMWithLightmapPolicyINT32_MAXHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMWithLightmapPolicy0HDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMWithLightmapPolicy0HDRLinear64")),
			FName(TEXT("TMobileBasePassVSFMobileMovableDirectionalLightCSMWithLightmapPolicyHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightWithLightmapPolicyINT32_MAXHDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightWithLightmapPolicyINT32_MAXHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightWithLightmapPolicy0HDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileMovableDirectionalLightWithLightmapPolicy0HDRLinear64")),
			FName(TEXT("TMobileBasePassVSFMobileMovableDirectionalLightWithLightmapPolicyHDRLinear64")),

			FName(TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicyINT32_MAXHDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicyINT32_MAXHDRLinear64")),

			FName(TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy0HDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy0HDRLinear64")),
			FName(TEXT("TMobileBasePassVSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicyHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsAndLQLightMapPolicyINT32_MAXHDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsAndLQLightMapPolicyINT32_MAXHDRLinear64")),
			FName(TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsAndLQLightMapPolicy0HDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsAndLQLightMapPolicy0HDRLinear64")),
			FName(TEXT("TMobileBasePassVSFMobileDistanceFieldShadowsAndLQLightMapPolicyHDRLinear64")),
			FName(TEXT("TMobileBasePassPSTLightMapPolicyLQINT32_MAXHDRLinear64Skylight")),
			FName(TEXT("TMobileBasePassPSTLightMapPolicyLQINT32_MAXHDRLinear64")),
			FName(TEXT("TMobileBasePassPSTLightMapPolicyLQ0HDRLinear64Skylight")),

			FName(TEXT("TMobileBasePassPSTLightMapPolicyLQ0HDRLinear64")),
			FName(TEXT("TMobileBasePassVSTLightMapPolicyLQHDRLinear64")),

			FName(TEXT("TBasePassPSFNoLightMapPolicySkylight")),
			FName(TEXT("TBasePassPSFCachedPointIndirectLightingPolicySkylight")),
			FName(TEXT("TBasePassVSFCachedVolumeIndirectLightingPolicy")),
			FName(TEXT("TBasePassPSFCachedVolumeIndirectLightingPolicy")),
			FName(TEXT("TBasePassPSFCachedVolumeIndirectLightingPolicySkylight")),
			FName(TEXT("TBasePassPSFPrecomputedVolumetricLightmapLightingPolicySkylight")),
			FName(TEXT("TBasePassVSFPrecomputedVolumetricLightmapLightingPolicy")),
			FName(TEXT("TBasePassPSFPrecomputedVolumetricLightmapLightingPolicy")),
			FName(TEXT("TBasePassPSFPrecomputedVolumetricLightmapLightingPolicySkylight")),

			FName(TEXT("TBasePassPSFSimpleStationaryLightVolumetricLightmapShadowsLightingPolicy")),

			FName(TEXT("TBasePassVSFNoLightMapPolicyAtmosphericFog")),
			FName(TEXT("TBasePassVSFCachedPointIndirectLightingPolicyAtmosphericFog")),
			FName(TEXT("TBasePassVSFSelfShadowedCachedPointIndirectLightingPolicy")),
			FName(TEXT("TBasePassPSFSelfShadowedCachedPointIndirectLightingPolicy")),
			FName(TEXT("TBasePassPSFSelfShadowedCachedPointIndirectLightingPolicySkylight")),
			FName(TEXT("TBasePassVSFSelfShadowedCachedPointIndirectLightingPolicyAtmosphericFog")),
			FName(TEXT("TBasePassVSFSelfShadowedTranslucencyPolicy")),
			FName(TEXT("TBasePassPSFSelfShadowedTranslucencyPolicy")),
			FName(TEXT("TBasePassPSFSelfShadowedTranslucencyPolicySkylight")),
			FName(TEXT("TBasePassVSFSelfShadowedTranslucencyPolicyAtmosphericFog")),

			FName(TEXT("TShadowDepthVSVertexShadowDepth_PerspectiveCorrectfalse")),
			FName(TEXT("TShadowDepthVSVertexShadowDepth_PerspectiveCorrecttrue")),
			FName(TEXT("TShadowDepthVSVertexShadowDepth_OnePassPointLightfalse")),
			FName(TEXT("TShadowDepthPSPixelShadowDepth_PerspectiveCorrectfalse")),
			FName(TEXT("TShadowDepthPSPixelShadowDepth_PerspectiveCorrecttrue")),
			FName(TEXT("TShadowDepthPSPixelShadowDepth_OnePassPointLightfalse")),
			FName(TEXT("TShadowDepthPSPixelShadowDepth_OnePassPointLighttrue")),

			FName(TEXT("TShadowDepthVSForGSVertexShadowDepth_OutputDepthfalse")),
			FName(TEXT("TShadowDepthVSForGSVertexShadowDepth_OutputDepthtrue")),
			FName(TEXT("TShadowDepthVSForGSVertexShadowDepth_PerspectiveCorrectfalse")),
			FName(TEXT("TShadowDepthVSForGSVertexShadowDepth_PerspectiveCorrecttrue")),
			FName(TEXT("TShadowDepthVSForGSVertexShadowDepth_OnePassPointLightfalse")),
			FName(TEXT("FOnePassPointShadowDepthGS")),

			FName(TEXT("TTranslucencyShadowDepthVS<TranslucencyShadowDepth_Standard>")),
			FName(TEXT("TTranslucencyShadowDepthPS<TranslucencyShadowDepth_Standard>")),
			FName(TEXT("TTranslucencyShadowDepthVS<TranslucencyShadowDepth_PerspectiveCorrect>")),
			FName(TEXT("TTranslucencyShadowDepthPS<TranslucencyShadowDepth_PerspectiveCorrect>")),

			FName(TEXT("TShadowDepthVSForGSVertexShadowDepth_OnePassPointLightPositionOnly")),
			FName(TEXT("TShadowDepthVSVertexShadowDepth_OnePassPointLightPositionOnly")),
			FName(TEXT("TShadowDepthVSVertexShadowDepth_OutputDepthPositionOnly")),
			FName(TEXT("TShadowDepthVSVertexShadowDepth_PerspectiveCorrectPositionOnly")),

			FName(TEXT("TBasePassVSTDistanceFieldShadowsAndLightMapPolicyHQAtmosphericFog")),
			FName(TEXT("TBasePassVSTLightMapPolicyHQAtmosphericFog")),
			FName(TEXT("TBasePassVSTLightMapPolicyLQAtmosphericFog")),
			FName(TEXT("TBasePassVSFPrecomputedVolumetricLightmapLightingPolicyAtmosphericFog")),
			FName(TEXT("TBasePassPSFSelfShadowedVolumetricLightmapPolicy")),
			FName(TEXT("TBasePassPSFSelfShadowedVolumetricLightmapPolicySkylight")),
			FName(TEXT("TBasePassVSFSelfShadowedVolumetricLightmapPolicyAtmosphericFog")),
			FName(TEXT("TBasePassVSFSelfShadowedVolumetricLightmapPolicy")),

			FName(TEXT("TBasePassVSFSimpleStationaryLightVolumetricLightmapShadowsLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleStationaryLightSingleSampleShadowsLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleStationaryLightSingleSampleShadowsLightingPolicySkylight")),
			FName(TEXT("TBasePassVSFSimpleStationaryLightSingleSampleShadowsLightingPolicy")),

			FName(TEXT("TBasePassPSFSimpleStationaryLightPrecomputedShadowsLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleStationaryLightPrecomputedShadowsLightingPolicySkylight ")),
			FName(TEXT("TBasePassVSFSimpleStationaryLightPrecomputedShadowsLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleLightmapOnlyLightingPolicy")),
			FName(TEXT("TBasePassPSFSimpleLightmapOnlyLightingPolicySkylight")),

			FName(TEXT("TBasePassVSFSimpleLightmapOnlyLightingPolicy")),

			FName(TEXT("TShadowDepthDSVertexShadowDepth_OnePassPointLightfalse")),
			FName(TEXT("TShadowDepthHSVertexShadowDepth_OnePassPointLightfalse")),
			FName(TEXT("TShadowDepthDSVertexShadowDepth_OutputDepthfalse")),
			FName(TEXT("TShadowDepthHSVertexShadowDepth_OutputDepthfalse")),
			FName(TEXT("TShadowDepthDSVertexShadowDepth_OutputDepthtrue")),
			FName(TEXT("TShadowDepthHSVertexShadowDepth_OutputDepthtrue")),

			FName(TEXT("TShadowDepthDSVertexShadowDepth_PerspectiveCorrectfalse")),
			FName(TEXT("TShadowDepthHSVertexShadowDepth_PerspectiveCorrectfalse")),
			FName(TEXT("TShadowDepthDSVertexShadowDepth_PerspectiveCorrecttrue")),
			FName(TEXT("TShadowDepthHSVertexShadowDepth_PerspectiveCorrecttrue")),

			FName(TEXT("FVelocityDS")),
			FName(TEXT("FVelocityHS")),
			FName(TEXT("FHitProxyDS")),
			FName(TEXT("FHitProxyHS")),

			FName(TEXT("TLightMapDensityDSTLightMapPolicyHQ")),
			FName(TEXT("TLightMapDensityHSTLightMapPolicyHQ")),
			FName(TEXT("TLightMapDensityDSTLightMapPolicyLQ")),
			FName(TEXT("TLightMapDensityHSTLightMapPolicyLQ")),
			FName(TEXT("TLightMapDensityDSFDummyLightMapPolicy")),
			FName(TEXT("TLightMapDensityHSFDummyLightMapPolicy")),
			FName(TEXT("TLightMapDensityDSFNoLightMapPolicy")),
			FName(TEXT("TLightMapDensityHSFNoLightMapPolicy")),
			FName(TEXT("FDepthOnlyDS")),
			FName(TEXT("FDepthOnlyHS")),
			FName(TEXT("FDebugViewModeDS")),
			FName(TEXT("FDebugViewModeHS")),
			FName(TEXT("TBasePassDSTDistanceFieldShadowsAndLightMapPolicyHQ")),
			FName(TEXT("TBasePassHSTDistanceFieldShadowsAndLightMapPolicyHQ")),

			FName(TEXT("TBasePassDSTLightMapPolicyHQ")),
			FName(TEXT("TBasePassHSTLightMapPolicyHQ")),
			FName(TEXT("TBasePassDSTLightMapPolicyLQ")),
			FName(TEXT("TBasePassHSTLightMapPolicyLQ")),
			FName(TEXT("TBasePassDSFCachedPointIndirectLightingPolicy")),
			FName(TEXT("TBasePassHSFCachedPointIndirectLightingPolicy")),
			FName(TEXT("TBasePassDSFCachedVolumeIndirectLightingPolicy")),
			FName(TEXT("TBasePassHSFCachedVolumeIndirectLightingPolicy")),

			FName(TEXT("TBasePassDSFPrecomputedVolumetricLightmapLightingPolicy")),
			FName(TEXT("TBasePassHSFPrecomputedVolumetricLightmapLightingPolicy")),
			FName(TEXT("TBasePassDSFNoLightMapPolicy")),
			FName(TEXT("TBasePassHSFNoLightMapPolicy")),
		};
		return ExcludedShaderTypes;
	}
};

FMaterialResource* ULandscapeMaterialInstanceConstant::AllocatePermutationResource()
{
	return new FLandscapeMaterialResource(this);
}

bool ULandscapeMaterialInstanceConstant::HasOverridenBaseProperties() const
{
	if (Parent)
	{
		// force a static permutation for ULandscapeMaterialInstanceConstants
		if (!Parent->IsA<ULandscapeMaterialInstanceConstant>())
		{
			return true;
		}
		ULandscapeMaterialInstanceConstant* LandscapeMICParent = CastChecked<ULandscapeMaterialInstanceConstant>(Parent);
		if (bDisableTessellation != LandscapeMICParent->bDisableTessellation)
		{
			return true;
		}
	}

	return Super::HasOverridenBaseProperties();
}

//////////////////////////////////////////////////////////////////////////

void ULandscapeComponent::GetStreamingTextureInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(GetOuter());
	FSphere BoundingSphere = Bounds.GetSphere();
	float LocalStreamingDistanceMultiplier = 1.f;
	float TexelFactor = 0.0f;
	if (Proxy)
	{
		LocalStreamingDistanceMultiplier = FMath::Max(0.0f, Proxy->StreamingDistanceMultiplier);
		TexelFactor = 0.75f * LocalStreamingDistanceMultiplier * ComponentSizeQuads * FMath::Abs(Proxy->GetRootComponent()->RelativeScale3D.X);
	}

	ERHIFeatureLevel::Type FeatureLevel = LevelContext.GetFeatureLevel();

	// TODO - LOD Materials - Currently all LOD materials are instances of [0] so have the same textures
    UMaterialInterface* MaterialInterface = FeatureLevel >= ERHIFeatureLevel::SM4 ? MaterialInstances[0] : MobileMaterialInterface;

	// Normal usage...
	// Enumerate the textures used by the material.
	if (MaterialInterface)
	{
		TArray<UTexture*> Textures;
		MaterialInterface->GetUsedTextures(Textures, EMaterialQualityLevel::Num, false, FeatureLevel, false);
		// Add each texture to the output with the appropriate parameters.
		// TODO: Take into account which UVIndex is being used.
		for (int32 TextureIndex = 0; TextureIndex < Textures.Num(); TextureIndex++)
		{
			UTexture2D* Texture2D = Cast<UTexture2D>(Textures[TextureIndex]);
			if (!Texture2D) continue;

			FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures)FStreamingTexturePrimitiveInfo;
			StreamingTexture.Bounds = BoundingSphere;
			StreamingTexture.TexelFactor = TexelFactor;
			StreamingTexture.Texture = Texture2D;
		}

		UMaterial* Material = MaterialInterface->GetMaterial();
		if (Material)
		{
			int32 NumExpressions = Material->Expressions.Num();
			for (int32 ExpressionIndex = 0; ExpressionIndex < NumExpressions; ExpressionIndex++)
			{
				UMaterialExpression* Expression = Material->Expressions[ExpressionIndex];
				UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expression);

				// TODO: This is only works for direct Coordinate Texture Sample cases
				if (TextureSample && TextureSample->Coordinates.IsConnected())
				{
					UMaterialExpressionTextureCoordinate* TextureCoordinate = nullptr;
					UMaterialExpressionLandscapeLayerCoords* TerrainTextureCoordinate = nullptr;
		
					for (UMaterialExpression* FindExp : Material->Expressions)
					{
						if (FindExp && FindExp->GetFName() == TextureSample->Coordinates.ExpressionName)
						{
							TextureCoordinate = Cast<UMaterialExpressionTextureCoordinate>(FindExp);
							if (!TextureCoordinate)
							{
								TerrainTextureCoordinate = Cast<UMaterialExpressionLandscapeLayerCoords>(FindExp);
							}
							break;
						}
					}

					if (TextureCoordinate || TerrainTextureCoordinate)
					{
						for (int32 i = 0; i < OutStreamingTextures.Num(); ++i)
						{
							FStreamingTexturePrimitiveInfo& StreamingTexture = OutStreamingTextures[i];
							if (StreamingTexture.Texture == TextureSample->Texture)
							{
								if (TextureCoordinate)
								{
									StreamingTexture.TexelFactor = TexelFactor * FPlatformMath::Max(TextureCoordinate->UTiling, TextureCoordinate->VTiling);
								}
								else //if ( TerrainTextureCoordinate )
								{
									StreamingTexture.TexelFactor = TexelFactor * TerrainTextureCoordinate->MappingScale;
								}
								break;
							}
						}
					}
				}
			}
		}

		// Lightmap
		const FMeshMapBuildData* MapBuildData = GetMeshMapBuildData();

		FLightMap2D* Lightmap = MapBuildData && MapBuildData->LightMap ? MapBuildData->LightMap->GetLightMap2D() : nullptr;
		uint32 LightmapIndex = AllowHighQualityLightmaps(FeatureLevel) ? 0 : 1;
		if (Lightmap && Lightmap->IsValid(LightmapIndex))
		{
			const FVector2D& Scale = Lightmap->GetCoordinateScale();
			if (Scale.X > SMALL_NUMBER && Scale.Y > SMALL_NUMBER)
			{
				const float LightmapTexelFactor = TexelFactor / FMath::Min(Scale.X, Scale.Y);
				new (OutStreamingTextures) FStreamingTexturePrimitiveInfo(Lightmap->GetTexture(LightmapIndex), Bounds, LightmapTexelFactor);
				new (OutStreamingTextures) FStreamingTexturePrimitiveInfo(Lightmap->GetAOMaterialMaskTexture(), Bounds, LightmapTexelFactor);
				new (OutStreamingTextures) FStreamingTexturePrimitiveInfo(Lightmap->GetSkyOcclusionTexture(), Bounds, LightmapTexelFactor);
			}
		}

		// Shadowmap
		FShadowMap2D* Shadowmap = MapBuildData && MapBuildData->ShadowMap ? MapBuildData->ShadowMap->GetShadowMap2D() : nullptr;
		if (Shadowmap && Shadowmap->IsValid())
		{
			const FVector2D& Scale = Shadowmap->GetCoordinateScale();
			if (Scale.X > SMALL_NUMBER && Scale.Y > SMALL_NUMBER)
			{
				const float ShadowmapTexelFactor = TexelFactor / FMath::Min(Scale.X, Scale.Y);
				new (OutStreamingTextures) FStreamingTexturePrimitiveInfo(Shadowmap->GetTexture(), Bounds, ShadowmapTexelFactor);
			}
		}
	}

	// Weightmap
	for (int32 TextureIndex = 0; TextureIndex < WeightmapTextures.Num(); TextureIndex++)
	{
		FStreamingTexturePrimitiveInfo& StreamingWeightmap = *new(OutStreamingTextures)FStreamingTexturePrimitiveInfo;
		StreamingWeightmap.Bounds = BoundingSphere;
		StreamingWeightmap.TexelFactor = TexelFactor;
		StreamingWeightmap.Texture = WeightmapTextures[TextureIndex];
	}

	// Heightmap
	if (HeightmapTexture)
	{
		FStreamingTexturePrimitiveInfo& StreamingHeightmap = *new(OutStreamingTextures)FStreamingTexturePrimitiveInfo;
		StreamingHeightmap.Bounds = BoundingSphere;

		float HeightmapTexelFactor = TexelFactor * (static_cast<float>(HeightmapTexture->GetSizeY()) / (ComponentSizeQuads + 1));
		StreamingHeightmap.TexelFactor = ForcedLOD >= 0 ? -(1 << (13 - ForcedLOD)) : HeightmapTexelFactor; // Minus Value indicate forced resolution (Mip 13 for 8k texture)
		StreamingHeightmap.Texture = HeightmapTexture;
	}

	// XYOffset
	if (XYOffsetmapTexture)
	{
		FStreamingTexturePrimitiveInfo& StreamingXYOffset = *new(OutStreamingTextures)FStreamingTexturePrimitiveInfo;
		StreamingXYOffset.Bounds = BoundingSphere;
		StreamingXYOffset.TexelFactor = TexelFactor;
		StreamingXYOffset.Texture = XYOffsetmapTexture;
	}

#if WITH_EDITOR
	if (GIsEditor && EditToolRenderData.DataTexture)
	{
		FStreamingTexturePrimitiveInfo& StreamingDatamap = *new(OutStreamingTextures)FStreamingTexturePrimitiveInfo;
		StreamingDatamap.Bounds = BoundingSphere;
		StreamingDatamap.TexelFactor = TexelFactor;
		StreamingDatamap.Texture = EditToolRenderData.DataTexture;
	}
#endif
}

void ALandscapeProxy::ChangeLODDistanceFactor(float InLODDistanceFactor)
{
	LODDistanceFactor = FMath::Clamp<float>(InLODDistanceFactor, 0.1f, MAX_LANDSCAPE_LOD_DISTANCE_FACTOR);
	float LODFactor;
	switch (LODFalloff)
	{
	case ELandscapeLODFalloff::SquareRoot:
		LODFactor = FMath::Square(FMath::Min(LANDSCAPE_LOD_SQUARE_ROOT_FACTOR * LODDistanceFactor, MAX_LANDSCAPE_LOD_DISTANCE_FACTOR));
		break;
	default:
	case ELandscapeLODFalloff::Linear:
		LODFactor = LODDistanceFactor;
		break;
	}

	if (LandscapeComponents.Num())
	{
		int32 CompNum = LandscapeComponents.Num();
		FLandscapeComponentSceneProxy** Proxies = new FLandscapeComponentSceneProxy*[CompNum];
		for (int32 Idx = 0; Idx < CompNum; ++Idx)
		{
			Proxies[Idx] = (FLandscapeComponentSceneProxy*)(LandscapeComponents[Idx]->SceneProxy);
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			LandscapeChangeLODDistanceFactorCommand,
			FLandscapeComponentSceneProxy**, Proxies, Proxies,
			int32, CompNum, CompNum,
			float, InLODDistanceFactor, FMath::Sqrt(2.f * FMath::Square((float)SubsectionSizeQuads)) * LANDSCAPE_LOD_DISTANCE_FACTOR / LODFactor,
			{
				for (int32 Idx = 0; Idx < CompNum; ++Idx)
				{
					Proxies[Idx]->ChangeLODDistanceFactor_RenderThread(InLODDistanceFactor);
				}
				delete[] Proxies;
			}
		);
	}
};

void FLandscapeComponentSceneProxy::ChangeLODDistanceFactor_RenderThread(float InLODDistanceFactor)
{
	LODDistance = InLODDistanceFactor;
}

bool FLandscapeComponentSceneProxy::HeightfieldHasPendingStreaming() const
{
	return HeightmapTexture && HeightmapTexture->bHasStreamingUpdatePending;
}

void FLandscapeComponentSceneProxy::GetHeightfieldRepresentation(UTexture2D*& OutHeightmapTexture, UTexture2D*& OutDiffuseColorTexture, FHeightfieldComponentDescription& OutDescription)
{
	OutHeightmapTexture = HeightmapTexture;
	OutDiffuseColorTexture = BaseColorForGITexture;
	OutDescription.HeightfieldScaleBias = HeightmapScaleBias;

	OutDescription.MinMaxUV = FVector4(
		HeightmapScaleBias.Z, 
		HeightmapScaleBias.W, 
		HeightmapScaleBias.Z + SubsectionSizeVerts * NumSubsections * HeightmapScaleBias.X - HeightmapScaleBias.X, 
		HeightmapScaleBias.W + SubsectionSizeVerts * NumSubsections * HeightmapScaleBias.Y - HeightmapScaleBias.Y);

	OutDescription.HeightfieldRect = FIntRect(SectionBase.X, SectionBase.Y, SectionBase.X + NumSubsections * SubsectionSizeQuads, SectionBase.Y + NumSubsections * SubsectionSizeQuads);

	OutDescription.NumSubsections = NumSubsections;

	OutDescription.SubsectionScaleAndBias = FVector4(SubsectionSizeQuads, SubsectionSizeQuads, HeightmapSubsectionOffsetU, HeightmapSubsectionOffsetV);
}

void FLandscapeComponentSceneProxy::GetLCIs(FLCIArray& LCIs)
{
	FLightCacheInterface* LCI = ComponentLightInfo.Get();
	if (LCI)
	{
		LCIs.Push(LCI);
	}
}

//
// FLandscapeNeighborInfo
//
void FLandscapeNeighborInfo::RegisterNeighbors()
{
	if (!bRegistered)
	{
		// Register ourselves in the map.
		TMap<FIntPoint, const FLandscapeNeighborInfo*>& SceneProxyMap = SharedSceneProxyMap.FindOrAdd(LandscapeKey);

		const FLandscapeNeighborInfo* Existing = SceneProxyMap.FindRef(ComponentBase);
		if (Existing == nullptr)//(ensure(Existing == nullptr))
		{
			SceneProxyMap.Add(ComponentBase, this);
			bRegistered = true;

			// Find Neighbors
			Neighbors[0] = SceneProxyMap.FindRef(ComponentBase + FIntPoint(0, -1));
			Neighbors[1] = SceneProxyMap.FindRef(ComponentBase + FIntPoint(-1, 0));
			Neighbors[2] = SceneProxyMap.FindRef(ComponentBase + FIntPoint(1, 0));
			Neighbors[3] = SceneProxyMap.FindRef(ComponentBase + FIntPoint(0, 1));

			// Add ourselves to our neighbors
			if (Neighbors[0])
			{
				Neighbors[0]->Neighbors[3] = this;
			}
			if (Neighbors[1])
			{
				Neighbors[1]->Neighbors[2] = this;
			}
			if (Neighbors[2])
			{
				Neighbors[2]->Neighbors[1] = this;
			}
			if (Neighbors[3])
			{
				Neighbors[3]->Neighbors[0] = this;
			}
		}
		else
		{
			UE_LOG(LogLandscape, Warning, TEXT("Duplicate ComponentBase %d, %d"), ComponentBase.X, ComponentBase.Y);
		}
	}
}

void FLandscapeNeighborInfo::UnregisterNeighbors()
{
	if (bRegistered)
	{
		// Remove ourselves from the map
		TMap<FIntPoint, const FLandscapeNeighborInfo*>* SceneProxyMap = SharedSceneProxyMap.Find(LandscapeKey);
		check(SceneProxyMap);

		const FLandscapeNeighborInfo* MapEntry = SceneProxyMap->FindRef(ComponentBase);
		if (MapEntry == this) //(/*ensure*/(MapEntry == this))
		{
			SceneProxyMap->Remove(ComponentBase);

			if (SceneProxyMap->Num() == 0)
			{
				// remove the entire LandscapeKey entry as this is the last scene proxy
				SharedSceneProxyMap.Remove(LandscapeKey);
			}
			else
			{
				// remove reference to us from our neighbors
				if (Neighbors[0])
				{
					Neighbors[0]->Neighbors[3] = nullptr;
				}
				if (Neighbors[1])
				{
					Neighbors[1]->Neighbors[2] = nullptr;
				}
				if (Neighbors[2])
				{
					Neighbors[2]->Neighbors[1] = nullptr;
				}
				if (Neighbors[3])
				{
					Neighbors[3]->Neighbors[0] = nullptr;
				}
			}
		}
	}
}

//
// FLandscapeMeshProxySceneProxy
//
FLandscapeMeshProxySceneProxy::FLandscapeMeshProxySceneProxy(UStaticMeshComponent* InComponent, const FGuid& InGuid, const TArray<FIntPoint>& InProxyComponentBases, int8 InProxyLOD)
: FStaticMeshSceneProxy(InComponent, false)
{
	if (!IsComponentLevelVisible())
	{
		bNeedsLevelAddedToWorldNotification = true;
	}

	ProxyNeighborInfos.Empty(InProxyComponentBases.Num());
	for (FIntPoint ComponentBase : InProxyComponentBases)
	{
		new(ProxyNeighborInfos) FLandscapeNeighborInfo(InComponent->GetWorld(), InGuid, ComponentBase, nullptr, InProxyLOD, 0);
	}
}

void FLandscapeMeshProxySceneProxy::CreateRenderThreadResources()
{
	FStaticMeshSceneProxy::CreateRenderThreadResources();

	if (IsComponentLevelVisible())
	{
		for (FLandscapeNeighborInfo& Info : ProxyNeighborInfos)
		{
			Info.RegisterNeighbors();
		}
	}
}

void FLandscapeMeshProxySceneProxy::OnLevelAddedToWorld()
{
	for (FLandscapeNeighborInfo& Info : ProxyNeighborInfos)
	{
		Info.RegisterNeighbors();
	}
}

FLandscapeMeshProxySceneProxy::~FLandscapeMeshProxySceneProxy()
{
	for (FLandscapeNeighborInfo& Info : ProxyNeighborInfos)
	{
		Info.UnregisterNeighbors();
	}
}

FPrimitiveSceneProxy* ULandscapeMeshProxyComponent::CreateSceneProxy()
{
	if (GetStaticMesh() == NULL
		|| GetStaticMesh()->RenderData == NULL
		|| GetStaticMesh()->RenderData->LODResources.Num() == 0
		|| GetStaticMesh()->RenderData->LODResources[0].VertexBuffer.GetNumVertices() == 0)
	{
		return NULL;
	}

	return new FLandscapeMeshProxySceneProxy(this, LandscapeGuid, ProxyComponentBases, ProxyLOD);
}
