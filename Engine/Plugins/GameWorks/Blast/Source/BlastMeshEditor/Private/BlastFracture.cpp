// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlastFracture.h"
#include "BlastFractureSettings.h"
#include "BlastMeshEditorModule.h"
#include "BlastMesh.h"
#include "BlastMeshFactory.h"
#include "BlastMeshUtilities.h"

#include "NvBlastExtAuthoring.h"
#include "NvBlastExtAuthoringTypes.h"
#include "NvBlastExtAuthoringBondGenerator.h"
#include "NvBlastExtAuthoringCollisionBuilder.h"
#include "NvBlastExtAuthoringMesh.h"
#include "NvBlastExtAuthoringFractureTool.h"
#include "NvBlastExtAuthoringCutout.h"
#include "NvBlastGlobals.h"

#include "Misc/MessageDialog.h"
#include "ScopedSlowTask.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "RandomStream.h"
#include "Misc/ScopeLock.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include "ComponentReregisterContext.h"
#include "Engine/StaticMesh.h"
#include "Components/SkinnedMeshComponent.h"
#include "../Private/MeshMergeHelpers.h"
#include "RawMesh.h"
#include "Engine/Texture2D.h"


#undef WITH_APEX
#define WITH_APEX 0
#include "PhysXPublic.h"
#include "Physics/IPhysXCooking.h"
#include "Physics/IPhysXCookingModule.h"


#define LOCTEXT_NAMESPACE "BlastMeshEditor"

#if UE_BUILD_DEVELOPMENT
#include <ctime>

class FBlastScopedProfiler
{
	std::clock_t Clock;
	double Duration = 0;
	bool IsStarted = false;
	FString Name;
public:
	FBlastScopedProfiler(const FString& InName, bool ShouldStart = true)
	{
		Name = InName;
		if (ShouldStart)
		{
			Start();
		}
	}

	~FBlastScopedProfiler()
	{
		Stop();
		UE_LOG(LogBlastMeshEditor, Verbose, TEXT("Blast Profiler: %s - Execution time, s: %f"), *Name, Duration / (double)CLOCKS_PER_SEC);
	}

	void Start()
	{
		if (IsStarted)
		{
			Stop();
		}
		Clock = std::clock();
		IsStarted = true;
	}

	void Stop()
	{
		if (IsStarted)
		{
			Duration += (std::clock() - Clock); // (double)CLOCKS_PER_SEC;
			IsStarted = false;
		}
	}
};
#else
class FBlastScopedProfiler
{
public:
	FBlastScopedProfiler(const FString& InName, bool ShouldStart = true) {}
	void Start() {}
	void Stop() {}
};
#endif

const FName FBlastFracture::InteriorMaterialID("Interior_Material");

inline int32 GetFractureChunkId(TSharedPtr<FFractureSession> session, uint32 ChunkIndex)
{
	if (session->IsRootFractured)
	{
		return ChunkIndex ? session->FractureData->assetToFractureChunkIdMap[ChunkIndex] : 0;
	}
	return ChunkIndex;
}

//////////////////////////////////////////////////////////////////////////
// FFractureRandomGenerator
//////////////////////////////////////////////////////////////////////////

class FFractureRandomGenerator : public Nv::Blast::RandomGeneratorBase
{
	FRandomStream RStream;
public:
	float getRandomValue()
	{
		return RStream.GetFraction();
	}
	void seed(int32_t seed)
	{
		if (seed < 0)
		{
			RStream.GenerateNewSeed();
		}
		else
		{
			RStream.Initialize(seed);
		}
	}
};


FBlastFracture::FBlastFracture()
{
	RandomGenerator = MakeShareable(new FFractureRandomGenerator());
	Config = NewObject<UBlastFractureSettingsConfig>();
}

FBlastFracture::~FBlastFracture()
{
}

UBlastFractureSettings* FBlastFracture::CreateFractureSettings(class FBlastMeshEditor* Editor) const
{
	UBlastFractureSettings* Settings = NewObject<UBlastFractureSettings>();
	Settings->BlastMeshEditor = Editor;
	return Settings;
}

TSharedPtr<FFractureSession> FBlastFracture::StartFractureSession(UBlastMesh* InBlastMesh, UStaticMesh* InSourceStaticMesh, UBlastFractureSettings* Settings)
{
	FScopeLock Lock(&ExclusiveFractureSection);

	TSharedPtr<FFractureSession> FractureSession = MakeShareable(new FFractureSession());
	FractureSession->FractureTool = TSharedPtr<Nv::Blast::FractureTool>(NvBlastExtAuthoringCreateFractureTool(), [](Nv::Blast::FractureTool* p)
	{
		p->release();
	});
	FractureSession->BlastMesh = InBlastMesh;
	if (!FractureSession->FractureTool.IsValid() || InBlastMesh == nullptr)
	{
		UE_LOG(LogBlastMeshEditor, Error, TEXT("Failed to start fracture session"));
		return nullptr;
	}

	FTransform UE4ToBlastTransform;
	FRawMesh InSourceRawMesh;
	if (InSourceStaticMesh != nullptr)
	{
		UE4ToBlastTransform = UBlastMeshFactory::GetTransformUE4ToBlastCoordinateSystem(nullptr);
		FMeshMergeHelpers::RetrieveMesh(InSourceStaticMesh, 0, InSourceRawMesh);
		BuildSmoothingGroups(InSourceRawMesh); //Retrieve mesh just assign default smoothing group 1 for each face. So we need to generate it.

		Nv::Blast::Mesh* Mesh = CreateAuthoringMeshFromRawMesh(InSourceRawMesh, UE4ToBlastTransform);
		FractureSession->FractureTool->setSourceMesh(Mesh);

		LoadFracturedMesh(FractureSession, -1, InSourceStaticMesh);
		
		//need this to force saving fracture tool state
		FractureSession->IsRootFractured = true;
		FractureSession->IsMeshCreatedFromFractureData = true;
	}
	else
	{
		FBlastScopedProfiler LFTSP("Load fracture tool state");
		auto& FTD = InBlastMesh->FractureToolData;
		uint32 ChunkCount = FTD.VerticesOffset.Num() - 1;
		if (FTD.VerticesOffset.Num() != 0 && FTD.EdgesOffset.Num() == ChunkCount + 1 && FTD.FacesOffset.Num() == ChunkCount + 1)
		{
			for (uint32 ChunkIndex = 0; ChunkIndex < ChunkCount; ChunkIndex++)
			{
				uint32 vb = FTD.VerticesOffset[ChunkIndex], ve = FTD.VerticesOffset[ChunkIndex + 1];
				uint32 eb = FTD.EdgesOffset[ChunkIndex], ee = FTD.EdgesOffset[ChunkIndex + 1];
				uint32 fb = FTD.FacesOffset[ChunkIndex], fe = FTD.FacesOffset[ChunkIndex + 1];
				Nv::Blast::Mesh* ChunkMesh = NvBlastExtAuthoringCreateMeshFromFacets((Nv::Blast::Vertex*)FTD.Vertices.GetData() + vb,
					(Nv::Blast::Edge*)FTD.Edges.GetData() + eb, (Nv::Blast::Facet*)FTD.Faces.GetData() + fb, ve - vb, ee - eb, fe - fb);
				if (ChunkIndex == 0)
				{
					FractureSession->FractureTool->setSourceMesh(ChunkMesh);
				}
				else
				{
					FractureSession->FractureTool->setChunkMesh(ChunkMesh, InBlastMesh->GetChunkInfo(ChunkIndex).parentChunkIndex);
				}	
			}
		}
		else //If there is no saved fracture tool state, we can load chunks from SkeletalMesh. Note: smoothing groups will be lost.
		{
			UE4ToBlastTransform = UBlastMeshFactory::GetTransformUE4ToBlastCoordinateSystem(Cast<UFbxSkeletalMeshImportData>(InBlastMesh->Mesh->AssetImportData));
			TArray<FRawMesh> RawMeshes;
			RawMeshes.SetNum(InBlastMesh->GetChunkCount());
			{
				FBlastScopedProfiler SCMP("GetRenderMesh");
				InBlastMesh->GetRenderMesh(0, RawMeshes);
			}
			for (int32 ChunkId = 0; ChunkId < RawMeshes.Num(); ChunkId++)
			{
				Nv::Blast::Mesh* ChunkMesh = CreateAuthoringMeshFromRawMesh(RawMeshes[ChunkId], UE4ToBlastTransform);
				if (ChunkId == 0)
				{
					FractureSession->FractureTool->setSourceMesh(ChunkMesh);
				}
				else
				{
					FractureSession->FractureTool->setChunkMesh(ChunkMesh, InBlastMesh->GetChunkInfo(ChunkId).parentChunkIndex);
				}			
			}
		}
		if (!FractureSession->IsRootFractured)
		{
			int32 SupportLevel = Settings->bDefaultSupportDepth ? Settings->DefaultSupportDepth : -1;
			LoadFractureData(FractureSession, SupportLevel, nullptr);
		}
	}

	if (Settings)
	{
		PopulateSettingsFromBlastMesh(Settings, FractureSession->BlastMesh);
	}

	return FractureSession;
}

void FBlastFracture::FinishFractureSession(FFractureSessionPtr FractureSession)
{
	auto FS = FractureSession.Pin();
	if (FS->IsRootFractured && FS->IsMeshCreatedFromFractureData)
	{
		{
			FBlastScopedProfiler SOMP("Save optimized mesh");

			TComponentReregisterContext<USkinnedMeshComponent> ReregisterContext;

			if (FS->BlastMesh->Mesh)
			{
				FS->BlastMesh->Mesh->ReleaseResources();
				FS->BlastMesh->Mesh->ReleaseResourcesFence.Wait();
			}
			CreateSkeletalMeshFromAuthoring(FS, true, nullptr);

			FS->BlastMesh->RebuildIndexToBoneNameMap();
			FS->BlastMesh->RebuildCookedBodySetupsIfRequired(true);
			FS->BlastMesh->Mesh->RebuildIndexBufferRanges();

			FS->BlastMesh->PostLoad();
		}
		if (FS->FractureData.IsValid())
		{
			auto FTD = &FS->BlastMesh->FractureToolData;// = NewObject<FBlastFractureToolData>(FS->BlastMesh);
			uint32_t ChunkCount = FS->FractureTool->getChunkCount();
			FTD->VerticesOffset.Reset(ChunkCount + 1);
			FTD->EdgesOffset.Reset(ChunkCount + 1);
			FTD->FacesOffset.Reset(ChunkCount + 1);
			FTD->VerticesOffset.Push(0);
			FTD->EdgesOffset.Push(0);
			FTD->FacesOffset.Push(0);
			for (uint32_t ChunkIndex = 0; ChunkIndex < ChunkCount; ChunkIndex++)
			{
				auto& Info = FS->FractureTool->getChunkInfo(ChunkIndex);
				FTD->VerticesOffset.Push(FTD->VerticesOffset.Last() + Info.meshData->getVerticesCount());
				FTD->EdgesOffset.Push(FTD->EdgesOffset.Last() + Info.meshData->getEdgesCount());
				FTD->FacesOffset.Push(FTD->FacesOffset.Last() + Info.meshData->getFacetCount());
			}
			FTD->Vertices.SetNumUninitialized(FTD->VerticesOffset.Last() * sizeof(Nv::Blast::Vertex));
			FTD->Edges.SetNumUninitialized(FTD->EdgesOffset.Last() * sizeof(Nv::Blast::Edge));
			FTD->Faces.SetNumUninitialized(FTD->FacesOffset.Last() * sizeof(Nv::Blast::Facet));

			physx::PxVec3 Offset;
			float Scale;
			FS->FractureTool->getTransformation(Offset, Scale);
			for (uint32_t ChunkIndex = 0; ChunkIndex < ChunkCount; ChunkIndex++)
			{
				auto& Info = FS->FractureTool->getChunkInfo(ChunkIndex);
				FMemory::Memcpy(FTD->Vertices.GetData() + FTD->VerticesOffset[ChunkIndex] * sizeof(Nv::Blast::Vertex), 
					Info.meshData->getVertices(), Info.meshData->getVerticesCount() * sizeof(Nv::Blast::Vertex));
				for (uint32_t v = 0; v < Info.meshData->getVerticesCount(); v++)
				{
					((Nv::Blast::Vertex*)FTD->Vertices.GetData() + FTD->VerticesOffset[ChunkIndex] + v)->p = Info.meshData->getVertices()[v].p * Scale + Offset;
				}
				FMemory::Memcpy(FTD->Edges.GetData() + FTD->EdgesOffset[ChunkIndex] * sizeof(Nv::Blast::Edge), 
					Info.meshData->getEdges(), Info.meshData->getEdgesCount() * sizeof(Nv::Blast::Edge));
				FMemory::Memcpy(FTD->Faces.GetData() + FTD->FacesOffset[ChunkIndex] * sizeof(Nv::Blast::Facet), 
					Info.meshData->getFacetsBuffer(), Info.meshData->getFacetCount() * sizeof(Nv::Blast::Facet));
			}
		}
	}
}

void FBlastFracture::GetVoronoiSites(TSharedPtr<FFractureSession> FractureSession, int32 ChunkId, TArray<FVector>& Sites)
{
	if (!FractureSession.IsValid() || !FractureSession->IsRootFractured)
	{
		return;
	}
	auto Generator = FractureSession->SitesGeneratorMap.Find(GetFractureChunkId(FractureSession, ChunkId));
	if (Generator == nullptr || !Generator->Generator.IsValid())
	{
		return;
	}
	const physx::PxVec3* sites = nullptr;
	int32 sitesCount = Generator->Generator->getVoronoiSites(sites);
	Sites.Reset(sitesCount);
	auto SkelMeshImportData = Cast<UFbxSkeletalMeshImportData>(FractureSession->BlastMesh->Mesh->AssetImportData);
	auto Converter = UBlastMeshFactory::GetTransformBlastToUE4CoordinateSystem(SkelMeshImportData);
	for (int32 i = 0; i < sitesCount; i++)
	{
		Sites.Add(Converter.TransformPosition(FVector(sites[i].x, sites[i].y, sites[i].z)));
	}
}

void FBlastFracture::Fracture(UBlastFractureSettings* Settings, TSet<int32>& SelectedChunkIndices, int32 ClickedChunkIndex)
{
	FBlastScopedProfiler FP("Fracture");
	FScopeLock Lock(&ExclusiveFractureSection);

	if (Settings == nullptr || !Settings->FractureSession.IsValid())
	{
		return;
	}
	auto FractureSession = Settings->FractureSession;
	
	auto InteriorMaterial = Settings->InteriorMaterial;
	if (Settings->InteriorMaterialSlotName == NAME_None)
	{
		FractureSession->FractureTool->setInteriorMaterialId(MATERIAL_INTERIOR);
	}
	else
	{
		int32_t InteriorMatID = MATERIAL_INTERIOR;
		const auto& MatList = FractureSession->BlastMesh->Mesh->Materials;
		for (int32 I = 0; I < MatList.Num(); I++)
		{
			const FSkeletalMaterial& Mat = MatList[I];
			if ((Mat.ImportedMaterialSlotName.IsNone() ? Mat.MaterialSlotName : Mat.ImportedMaterialSlotName) == Settings->InteriorMaterialSlotName)
			{
				InteriorMatID = I;
				InteriorMaterial = MatList[I].MaterialInterface;
				if (InteriorMaterial != nullptr)
				{
					break;
				}
			}
		}
		FractureSession->FractureTool->setInteriorMaterialId(InteriorMatID);
	}

	if (ClickedChunkIndex != INDEX_NONE)
	{
		if (SelectedChunkIndices.Num() == 0 )
		{
			SelectedChunkIndices.Reset();
			SelectedChunkIndices.Add(ClickedChunkIndex);
		}

		//case 1: clicked chunk is child of SelectedChunk and SelectedChunk the same as last fracture chunk - continue fracturing
		//case 2: otherwise - finallize previous chunk fracture and start new one for clicked chunk
		//if (!FractureSession->FractureData.IsValid() 
		//	|| FractureSession->FractureData->chunkDescs[ClickedChunkIndex].parentChunkIndex != SelectedChunkIndices[0]
		//	|| FractureSession->LastFractureId != GetFractureChunkId(FractureSession, SelectedChunkIndices[0]))
		//{
		//	FractureSession->LastFractureId = -1;
		//	SelectedChunkIndices[0] = ClickedChunkIndex;
		//}
	}
	else
	{
		//FractureSession->LastFractureId = -1;
	}
	//SelectedChunkIndices.Sort(); NOTE: Don't know is it really need here. Looks like fracture works correctly without it.
	int32 FirstInvalidChunk = FractureSession->BlastMesh->GetChunkCount();
	EAppReturnType::Type action = EAppReturnType::Cancel;
	TArray<int32> FracturedChunks;
	FractureSession->FractureTool->setRemoveIslands(Settings->bRemoveIslands);
	bool IsCancel = false;

	auto UE4ToBlastTransform = UBlastMeshFactory::GetTransformUE4ToBlastCoordinateSystem(Cast<UFbxSkeletalMeshImportData>(FractureSession->BlastMesh->Mesh->AssetImportData));

	for (int32 ChunkIndex : SelectedChunkIndices)
	{
		if (IsCancel)
		{
			break;
		}
		int32_t FractureChunkId = GetFractureChunkId(FractureSession, ChunkIndex);
		if (ChunkIndex && (ChunkIndex >= FirstInvalidChunk || FractureChunkId == INDEX_NONE))
		{
			continue;
		}
		int32 RandomSeed = Settings->bUseFractureSeed ? Settings->FractureSeed : -1;
		bool IsReplace = Settings->bReplaceFracturedChunk;
		switch (Settings->FractureMethod)
		{
		case EBlastFractureMethod::VoronoiUniform:
			if (!FractureVoronoi(FractureSession, FractureChunkId, RandomSeed, IsReplace,
				Settings->VoronoiUniformFracture->CellCount,
				Settings->VoronoiUniformFracture->CellAnisotropy, Settings->VoronoiUniformFracture->CellRotation,
				Settings->VoronoiUniformFracture->ForceReset))
			{
				IsCancel = true;
			}
			break;
		case EBlastFractureMethod::VoronoiClustered:
			if (!FractureClusteredVoronoi(FractureSession, FractureChunkId, RandomSeed, IsReplace,
				Settings->VoronoiClusteredFracture->CellCount, Settings->VoronoiClusteredFracture->ClusterCount,
				Settings->VoronoiClusteredFracture->ClusterRadius,
				Settings->VoronoiClusteredFracture->CellAnisotropy, Settings->VoronoiClusteredFracture->CellRotation,
				Settings->VoronoiClusteredFracture->ForceReset))
			{
				IsCancel = true;
			}
			break;
		case EBlastFractureMethod::VoronoiRadial:
			if (!FractureRadial(FractureSession, FractureChunkId, RandomSeed, IsReplace,
				UE4ToBlastTransform.TransformPosition(Settings->RadialFracture->Origin),
				UE4ToBlastTransform.TransformVector(Settings->RadialFracture->Normal),
				Settings->RadialFracture->Radius,
				Settings->RadialFracture->AngularSteps, Settings->RadialFracture->RadialSteps,
				Settings->RadialFracture->AngleOffset, Settings->RadialFracture->Variability,
				Settings->RadialFracture->CellAnisotropy, Settings->RadialFracture->CellRotation,
				Settings->RadialFracture->ForceReset))
			{
				IsCancel = true;
			}
			break;
		case EBlastFractureMethod::VoronoiInSphere:
			if (!FractureInSphere(FractureSession, FractureChunkId, RandomSeed, IsReplace,
				Settings->InSphereFracture->CellCount, Settings->InSphereFracture->Radius, 
				UE4ToBlastTransform.TransformPosition(Settings->InSphereFracture->Origin),
				Settings->InSphereFracture->CellAnisotropy, Settings->InSphereFracture->CellRotation,
				Settings->InSphereFracture->ForceReset))
			{
				//IsCancel = true;
			}
			break;
		case EBlastFractureMethod::VoronoiRemoveInSphere:
			if (!RemoveInSphere(FractureSession, FractureChunkId, RandomSeed, IsReplace,
				Settings->RemoveInSphere->Radius,
				UE4ToBlastTransform.TransformPosition(Settings->RemoveInSphere->Origin),
				Settings->RemoveInSphere->Probability, Settings->RemoveInSphere->ForceReset))
			{
				//IsCancel = true;
			}
			break;
		case EBlastFractureMethod::UniformSlicing:
			if (!FractureUniformSlicing(FractureSession, FractureChunkId, RandomSeed, IsReplace,
				Settings->UniformSlicingFracture->SlicesCount, Settings->UniformSlicingFracture->AngleVariation,
				Settings->UniformSlicingFracture->OffsetVariation,
				Settings->UniformSlicingFracture->Amplitude, Settings->UniformSlicingFracture->Frequency,
				Settings->UniformSlicingFracture->OctaveNumber, Settings->UniformSlicingFracture->SurfaceResolution))
			{
				IsCancel = true;
			}
			break;
		case EBlastFractureMethod::Cutout:
			if (!FractureCutout(FractureSession, FractureChunkId, RandomSeed, IsReplace,
				Settings->CutoutFracture->Pattern, Settings->CutoutFracture->Origin, Settings->CutoutFracture->Normal,
				Settings->CutoutFracture->Size, Settings->CutoutFracture->RotationZ,
				Settings->CutoutFracture->bPeriodic, Settings->CutoutFracture->bFillGaps,
				Settings->CutoutFracture->Amplitude, Settings->CutoutFracture->Frequency,
				Settings->CutoutFracture->OctaveNumber, Settings->CutoutFracture->SurfaceResolution))
			{
				IsCancel = true;
			}
			break;
		case EBlastFractureMethod::Cut:
			if (!FractureCut(FractureSession, FractureChunkId, RandomSeed, IsReplace,
				UE4ToBlastTransform.TransformPosition(Settings->CutFracture->Point), 
				UE4ToBlastTransform.TransformPosition(Settings->CutFracture->Normal),
				Settings->CutFracture->Amplitude, Settings->CutFracture->Frequency,
				Settings->CutFracture->OctaveNumber, Settings->CutFracture->SurfaceResolution))
			{
				IsCancel = true;
			}
			break;
		}
		FracturedChunks.Add(ChunkIndex);
	}
	if (FracturedChunks.Num())
	{
		int32 SupportLevel = Settings->bDefaultSupportDepth ? Settings->DefaultSupportDepth : -1;
		LoadFracturedMesh(Settings->FractureSession, SupportLevel, nullptr, InteriorMaterial);
		PopulateSettingsFromBlastMesh(Settings, FractureSession->BlastMesh);
	}
}

void FBlastFracture::FitUvs(UBlastFractureSettings* Settings, float Size, bool OnlySpecified, TSet<int32>& ChunkIndices)
{
	FScopeLock Lock(&ExclusiveFractureSection);
	if (ChunkIndices.Num() > 0 && OnlySpecified)
	{
		for (int32 ChunkIndex : ChunkIndices) 
		{
			int32 ChunkId = GetFractureChunkId(Settings->FractureSession, ChunkIndex);
			Settings->FractureSession->FractureTool->fitUvToRect(Size, ChunkId);
		}
	}
	else
	{
		Settings->FractureSession->FractureTool->fitAllUvToRect(Size);
	}
	int32 SupportLevel = Settings->bDefaultSupportDepth ? Settings->DefaultSupportDepth : -1;
	ReloadGraphicsMesh(Settings->FractureSession, SupportLevel, nullptr, Settings->InteriorMaterial);
}

void FBlastFracture::BuildChunkHierarchy(UBlastFractureSettings* Settings, uint32 Threshold, uint32 TargetedClusterSize)
{
	FScopeLock Lock(&ExclusiveFractureSection);
	
	if (Settings == nullptr || !Settings->FractureSession.IsValid() || !Settings->FractureSession->IsRootFractured)
	{
		return;
	}
	int32 SupportLevel = Settings->bDefaultSupportDepth ? Settings->DefaultSupportDepth : -1;
	
	Settings->FractureSession->FractureTool->uniteChunks(Threshold, TargetedClusterSize);

	LoadFracturedMesh(Settings->FractureSession, SupportLevel, nullptr, Settings->InteriorMaterial);
}

bool CreatePhysXAsset(TSharedPtr<Nv::Blast::AuthoringResult> FractureData, UBlastMesh* BlastMesh, UFbxSkeletalMeshImportData* SkelMeshImportData = nullptr)
{
	if (SkelMeshImportData == nullptr)
	{
		SkelMeshImportData = Cast<UFbxSkeletalMeshImportData>(BlastMesh->Mesh->AssetImportData);
	}
	FBlastScopedProfiler PACP("PhysicsAssetCreation");
	TMap<FName, TArray<FBlastCollisionHull>> hulls;
	hulls.Empty();
	auto Converter = UBlastMeshFactory::GetTransformBlastToUE4CoordinateSystem(SkelMeshImportData);
	for (uint32 ci = 0; ci < FractureData->chunkCount; ci++)
	{
		auto& chunkHulls = hulls.Add(*(FString("chunk_") + FString::FromInt(ci)));
		for (uint32 hi = FractureData->collisionHullOffset[ci]; hi < FractureData->collisionHullOffset[ci + 1]; hi++)
		{
			FBlastCollisionHull h;
			auto fh = FractureData->collisionHull[hi];
			for (uint32 pi = 0; pi < fh->pointsCount; pi++)
			{
				auto p = fh->points[pi];
				h.Points.Add(Converter.TransformPosition(FVector(p.x, p.y, p.z)));
			}
			for (uint32 ii = 0; ii < fh->indicesCount; ii++)
			{
				h.Indices.Add(fh->indices[ii]);
			}
			for (uint32 pdi = 0; pdi < fh->polygonDataCount; pdi++)
			{
				h.PolygonData.Add(*reinterpret_cast<FBlastCollisionHull::HullPolygon*>(&fh->polygonData[pdi]));
			}
			chunkHulls.Add(h);
		}
	}
	if (!UBlastMeshFactory::RebuildPhysicsAsset(BlastMesh, hulls))
	{
		return false;
	}
	return true;
}

void FBlastFracture::RebuildCollisionMesh(UBlastFractureSettings* Settings, uint32_t MaxNumOfConvex, uint32_t Resolution, float Concavity, const TSet<int32>& ChunkIndices)
{
	FScopeLock Lock(&ExclusiveFractureSection);

	if (Settings == nullptr || !Settings->FractureSession.IsValid() || !Settings->FractureSession->IsRootFractured)
	{
		return;
	}
	FScopedSlowTask SlowTask(1, LOCTEXT("RebuildCollisionMesh", "Recalculating collision mesh, this may take a while."));

	SlowTask.MakeDialog();
	SlowTask.EnterProgressFrame();

	auto FractureData = Settings->FractureSession->FractureData;
	uint32_t ChunkCount = FractureData->chunkCount;
	Nv::Blast::CollisionParams Param;
	Param.maximumNumberOfHulls = MaxNumOfConvex;
	Param.voxelGridResolution = Resolution;
	Param.concavity = Concavity;

	auto CollisionBuilder = NvBlastExtAuthoringCreateConvexMeshBuilder(GetPhysXCookingModule()->GetPhysXCooking()->GetCooking(), 
		&GPhysXSDK->getPhysicsInsertionCallback());

	NvBlastExtAuthoringBuildCollisionMeshes(*FractureData, *CollisionBuilder, Param, ChunkIndices.Num(), ChunkIndices.Num() ? (uint32_t*)&ChunkIndices.Array()[0] : nullptr);

	if (FractureData->collisionHull != nullptr)
	{
		CreatePhysXAsset(FractureData, Settings->FractureSession->BlastMesh);
	}
}

void FBlastFracture::RemoveChildren(UBlastFractureSettings* Settings, TSet<int32>& SelectedChunkIndices)
{
	FScopeLock Lock(&ExclusiveFractureSection);

	if (Settings == nullptr || !Settings->FractureSession.IsValid() || SelectedChunkIndices.Num() == 0)
	{
		return;
	}
	auto FractureSession = Settings->FractureSession;
	int32 SupportLevel = Settings->bDefaultSupportDepth ? Settings->DefaultSupportDepth : -1;
	bool IsMeshChanged = false;
	for (int32 ChunkIndex : SelectedChunkIndices)
	{
		int32_t FractureChunkId = GetFractureChunkId(FractureSession, ChunkIndex);
		IsMeshChanged |= FractureSession->FractureTool->deleteAllChildrenOfChunk(FractureChunkId);
		auto Generator = FractureSession->SitesGeneratorMap.Find(FractureChunkId);
		if (Generator != nullptr && Generator->Generator.IsValid())
		{
			//Generator->Generator->release();
			Generator->Generator.Reset();
		}
	}
	if (IsMeshChanged)
	{
		LoadFracturedMesh(FractureSession, SupportLevel, nullptr, nullptr);
	}
}

void BoneRearragementDfs(uint32 v,TArray<TArray<uint32>> & graph, TArray<bool>& used, TArray<uint32>& chunkListInOrder)
{
	used[v] = true;
	for (int32 id = 0; id < graph[v].Num(); ++id)
	{
		if (!used[graph[v][id]])
		{
			BoneRearragementDfs(graph[v][id], graph, used, chunkListInOrder);
		}
	}
	chunkListInOrder.Add(v);
}

void FBlastFracture::ReloadGraphicsMesh(FFractureSessionPtr FractureSession, int32_t DefaultSupportDepth, UStaticMesh* InSourceStaticMesh, UMaterialInterface* InteriorMaterial)
{
	TComponentReregisterContext<USkinnedMeshComponent> ReregisterContext;
	auto FS = FractureSession.Pin();
	if (FS->FractureData.Get() == nullptr)
	{
		IPhysXCookingModule* CookingModule = GetPhysXCookingModule();
		PxCooking* CookingInstance = CookingModule->GetPhysXCooking()->GetCooking();
		auto BondGenerator = NvBlastExtAuthoringCreateBondGenerator(CookingInstance, &GPhysXSDK->getPhysicsInsertionCallback());
		auto CollisionBuilder = NvBlastExtAuthoringCreateConvexMeshBuilder(CookingInstance, &GPhysXSDK->getPhysicsInsertionCallback());
		Nv::Blast::CollisionParams param;
		param.maximumNumberOfHulls = 1;
		param.voxelGridResolution = 0;
		{
			FBlastScopedProfiler EAPFP("NvBlastExtAuthoringProcessFracture");
			FS->FractureData = TSharedPtr<Nv::Blast::AuthoringResult>(
				NvBlastExtAuthoringProcessFracture(*FS->FractureTool, *BondGenerator, *CollisionBuilder, param, DefaultSupportDepth),
				[](Nv::Blast::AuthoringResult* p)
			{
				p->release();
			});
		}
		BondGenerator->release();
		CollisionBuilder->release();
		if (!FS->FractureData.IsValid())
		{
			return;
		}
	}
	else
	{
		NvBlastExtAuthoringUpdateGraphicsMesh(*FS->FractureTool.Get(), *FS->FractureData.Get());
	}


	if (FS->ChunkToBoneIndex.Num() == 0)
	{
		TArray<uint32_t> chunkListInOrder;

		{
			TArray<TArray<uint32_t> > graph;
			graph.SetNum(FS->FractureData->chunkCount);
			for (uint32 ci = 0; ci < FS->FractureData->chunkCount; ++ci)
			{
				if (FS->FractureData->chunkDescs[ci].parentChunkIndex != UINT32_MAX)
				{
					graph[FS->FractureData->chunkDescs[ci].parentChunkIndex].Add(ci);
				}
			}

			TArray<bool> used;
			used.SetNumZeroed(FS->FractureData->chunkCount);

			for (uint32 ci = 0; ci < FS->FractureData->chunkCount; ++ci)
			{
				if (!used[ci])
				{
					BoneRearragementDfs(ci, graph, used, chunkListInOrder);
				}
			}
		}

		FS->ChunkToBoneIndexPrev = FS->ChunkToBoneIndex;
		FS->ChunkToBoneIndex.Empty();
		FS->ChunkToBoneIndex.Add(INDEX_NONE, 0);
		int32 BoneIndex = 1;
		for (int32 ci = FS->FractureData->chunkCount - 1; ci >= 0; --ci)
		{
			FS->ChunkToBoneIndex.Add(chunkListInOrder[ci], BoneIndex++);
		}		
	}
	UBlastMesh* BlastMesh = FS->BlastMesh;
	if (BlastMesh->Mesh)
	{
		BlastMesh->Mesh->ReleaseResources();
		BlastMesh->Mesh->ReleaseResourcesFence.Wait();
	}
	CreateSkeletalMeshFromAuthoring(FS, false, InteriorMaterial);
	
	BlastMesh->RebuildIndexToBoneNameMap();
	BlastMesh->Mesh->RebuildIndexBufferRanges();
	BlastMesh->PostLoad();
};

bool FBlastFracture::LoadFractureData(FFractureSessionPtr FractureSession, int32_t DefaultSupportDepth, UStaticMesh* InSourceStaticMesh)
{
	auto FS = FractureSession.Pin();
	UBlastMesh* BlastMesh = FS->BlastMesh;
	if (!FS.IsValid() || (InSourceStaticMesh == nullptr && BlastMesh == nullptr))
	{
		return false;
	}

	if (FS->FractureData.IsValid())
	{
		FS->FractureIdMap.Reset(FS->FractureData->chunkCount);
		for (uint32 i = 0; i < FS->FractureData->chunkCount; i++)
		{
			FS->FractureIdMap.Add(GetFractureChunkId(FS, i));
		}
	}
	else
	{
		FS->FractureIdMap.Empty();
	}

	IPhysXCookingModule* CookingModule = GetPhysXCookingModule();
	PxCooking* CookingInstance = CookingModule->GetPhysXCooking()->GetCooking();
	auto BondGenerator = NvBlastExtAuthoringCreateBondGenerator(CookingInstance, &GPhysXSDK->getPhysicsInsertionCallback());
	auto CollisionBuilder = NvBlastExtAuthoringCreateConvexMeshBuilder(CookingInstance, &GPhysXSDK->getPhysicsInsertionCallback());
	Nv::Blast::CollisionParams param;
	param.maximumNumberOfHulls = 1;
	param.voxelGridResolution = 0;
	{
		FBlastScopedProfiler EAPFP("NvBlastExtAuthoringProcessFracture");
		FS->FractureData = TSharedPtr<Nv::Blast::AuthoringResult>(
			NvBlastExtAuthoringProcessFracture(*FS->FractureTool, *BondGenerator, *CollisionBuilder, param, DefaultSupportDepth),
			[](Nv::Blast::AuthoringResult* p)
		{
			p->release();
		});
	}
	BondGenerator->release();
	CollisionBuilder->release();
	if (!FS->FractureData.IsValid())
	{
		return false;
	}
	FS->IsRootFractured = true;

	TArray<uint32_t> chunkListInOrder;

	{
		TArray<TArray<uint32_t> > graph;
		graph.SetNum(FS->FractureData->chunkCount);
		for (uint32 ci = 0; ci < FS->FractureData->chunkCount; ++ci)
		{
			if (FS->FractureData->chunkDescs[ci].parentChunkIndex != UINT32_MAX)
			{
				graph[FS->FractureData->chunkDescs[ci].parentChunkIndex].Add(ci);
			}
		}

		TArray<bool> used;
		used.SetNumZeroed(FS->FractureData->chunkCount);

		for (uint32 ci = 0; ci < FS->FractureData->chunkCount; ++ci)
		{
			if (!used[ci])
			{
				BoneRearragementDfs(ci, graph, used, chunkListInOrder);
			}
		}
	}

	FS->ChunkToBoneIndexPrev = FS->ChunkToBoneIndex;
	FS->ChunkToBoneIndex.Empty();
	FS->ChunkToBoneIndex.Add(INDEX_NONE, 0);
	int32 BoneIndex = 1;
	for (int32 ci = FS->FractureData->chunkCount - 1; ci >= 0; --ci)
	{
		FS->ChunkToBoneIndex.Add(chunkListInOrder[ci], BoneIndex++);
	}

	return true;
}

void FBlastFracture::LoadFracturedMesh(FFractureSessionPtr FractureSession, int32_t DefaultSupportDepth, UStaticMesh* InSourceStaticMesh, UMaterialInterface* InteriorMaterial)
{
	FBlastScopedProfiler LFMP("LoadFracturedMesh");
	TComponentReregisterContext<USkinnedMeshComponent> ReregisterContext;

	auto FS = FractureSession.Pin();
	UBlastMesh* BlastMesh = FS->BlastMesh;

	if (!LoadFractureData(FractureSession, DefaultSupportDepth, InSourceStaticMesh))
	{
		return;
	}

	if (BlastMesh->Mesh)
	{
		BlastMesh->Mesh->ReleaseResources();
		BlastMesh->Mesh->ReleaseResourcesFence.Wait();
	}

	TArray<FSkeletalMaterial> ExistingMaterials;
	UFbxSkeletalMeshImportData* SkelMeshImportData = nullptr;
	FTransform RootTransform(FTransform::Identity);

	if (InSourceStaticMesh != nullptr)
	{
		CreateSkeletalMeshFromAuthoring(FS, InSourceStaticMesh);

	}
	else
	{
		FBlastScopedProfiler USMFAP("UpdateSkeletalMeshFromAuthoring");

		SkelMeshImportData = Cast<UFbxSkeletalMeshImportData>(BlastMesh->Mesh->AssetImportData);

		UpdateSkeletalMeshFromAuthoring(FS, InteriorMaterial);
	}

	//* generate NvBlastAsset and setup it for Mesh
	UBlastMeshFactory::TransformBlastAssetToUE4CoordinateSystem(FS->FractureData->asset, SkelMeshImportData);
	BlastMesh->CopyFromLoadedAsset(FS->FractureData->asset);

	if (!CreatePhysXAsset(FS->FractureData, BlastMesh, SkelMeshImportData))
	{
		return;
	}

	// Have to manually call this, since it doesn't get called on create
	BlastMesh->RebuildIndexToBoneNameMap();
	BlastMesh->RebuildCookedBodySetupsIfRequired(true);
	BlastMesh->Mesh->RebuildIndexBufferRanges();

	BlastMesh->PostLoad();
}


void FBlastFracture::PopulateSettingsFromBlastMesh(UBlastFractureSettings* Settings, UBlastMesh* BlastMesh)
{
	//If we have an interior material from a previous fracture, default to that
	if (BlastMesh->Mesh && Settings->InteriorMaterialSlotName == NAME_None)
	{
		for (FSkeletalMaterial& MatSlot : BlastMesh->Mesh->Materials)
		{
			const bool bCompareTrailingNumber = false;
			if (MatSlot.ImportedMaterialSlotName.IsEqual(FBlastFracture::InteriorMaterialID, ENameCase::IgnoreCase, bCompareTrailingNumber))
			{
				Settings->InteriorMaterialSlotName = MatSlot.ImportedMaterialSlotName;
				break;
			}
		}
	}
}

TSharedPtr <Nv::Blast::VoronoiSitesGenerator> FBlastFracture::GetVoronoiSitesGenerator(TSharedPtr<FFractureSession> FractureSession, int32 FractureChunkId, bool ForceReset)
{
	auto SitesGenerator = FractureSession->SitesGeneratorMap.Find(FractureChunkId);
	if (SitesGenerator == nullptr || !SitesGenerator->Generator.IsValid() || ForceReset)
	{
		FractureSession->SitesGeneratorMap.Remove(FractureChunkId);
		SitesGenerator = &(FractureSession->SitesGeneratorMap.Add(FractureChunkId, FFractureSession::ChunkSitesGenerator()));
		SitesGenerator->Mesh = TSharedPtr <Nv::Blast::Mesh>(FractureSession->FractureTool->createChunkMesh(FractureChunkId), [](Nv::Blast::Mesh* p)
		{
			p->release();
		});
		if (SitesGenerator->Mesh.Get() == nullptr)
		{
			return TSharedPtr <Nv::Blast::VoronoiSitesGenerator>(nullptr);
		}
		SitesGenerator->Generator = TSharedPtr <Nv::Blast::VoronoiSitesGenerator>(
			NvBlastExtAuthoringCreateVoronoiSitesGenerator(SitesGenerator->Mesh.Get(), RandomGenerator.Get()),
			[](Nv::Blast::VoronoiSitesGenerator* p)
		{
			p->release();
		});
	}
	return SitesGenerator->Generator;
};

bool FBlastFracture::FractureVoronoi(TSharedPtr<FFractureSession> FractureSession, uint32 FractureChunkId, int32 RandomSeed, bool IsReplace,
	uint32 CellCount, FVector CellAnisotropy, FQuat CellRotation, bool ForceReset)
{
	FBlastScopedProfiler FVP("FractureVoronoi");
	RandomGenerator->seed(RandomSeed);
	auto VoronoiSitesGenerator = GetVoronoiSitesGenerator(FractureSession, FractureChunkId, ForceReset);
	if (VoronoiSitesGenerator.IsValid())
	{
		{
			FBlastScopedProfiler UGSP("FractureVoronoi::uniformlyGenerateSitesInMesh");
			VoronoiSitesGenerator->uniformlyGenerateSitesInMesh(CellCount);
		}
		const physx::PxVec3* sites = nullptr;
		uint32_t sitesCount = VoronoiSitesGenerator->getVoronoiSites(sites);
		if (FractureSession->FractureTool->getChunkDepth(FractureChunkId) == 0)
		{
			IsReplace = false;
		}
		physx::PxVec3 ca(CellAnisotropy.X, CellAnisotropy.Y, CellAnisotropy.Z);
		physx::PxQuat cr(CellRotation.X, CellRotation.Y, CellRotation.Z, CellRotation.W);
		{
			FBlastScopedProfiler VFP("FractureVoronoi::voronoiFracturing");
			if (FractureSession->FractureTool->voronoiFracturing(FractureChunkId, sitesCount, sites, ca, cr, IsReplace) == 0)
			{
				return true;
			}
		}
	}
	UE_LOG(LogBlastMeshEditor, Error, TEXT("Failed to fracture with Voronoi"));
	return false;
}

bool FBlastFracture::FractureClusteredVoronoi(TSharedPtr<FFractureSession> FractureSession, uint32 FractureChunkId, int32 RandomSeed, bool IsReplace,
	uint32 CellCount, uint32 ClusterCount, float ClusterRadius, FVector CellAnisotropy, FQuat CellRotation, bool ForceReset)
{
	FBlastScopedProfiler FCVP("FractureClusteredVoronoi");
	RandomGenerator->seed(RandomSeed);
	auto VoronoiSitesGenerator = GetVoronoiSitesGenerator(FractureSession, FractureChunkId, ForceReset);
	if (VoronoiSitesGenerator.IsValid())
	{
		VoronoiSitesGenerator->clusteredSitesGeneration(ClusterCount, CellCount, ClusterRadius);
		const physx::PxVec3* sites = nullptr;
		uint32_t sitesCount = VoronoiSitesGenerator->getVoronoiSites(sites);
		if (FractureSession->FractureTool->getChunkDepth(FractureChunkId) == 0)
		{
			IsReplace = false;
		}
		physx::PxVec3 ca(CellAnisotropy.X, CellAnisotropy.Y, CellAnisotropy.Z);
		physx::PxQuat cr(CellRotation.X, CellRotation.Y, CellRotation.Z, CellRotation.W);
		if (FractureSession->FractureTool->voronoiFracturing(FractureChunkId, sitesCount, sites, ca, cr, IsReplace) == 0)
		{
			return true;
		}
	}
	UE_LOG(LogBlastMeshEditor, Error, TEXT("Failed to fracture with clustered Voronoi"));
	return false;
}

bool FBlastFracture::FractureRadial(TSharedPtr<FFractureSession> FractureSession, uint32 FractureChunkId, int32 RandomSeed, bool IsReplace,
	FVector Origin, FVector Normal, float Radius, uint32_t AngularSteps, uint32_t RadialSteps, float AngleOffset, float Variability,
	FVector CellAnisotropy, FQuat CellRotation, bool ForceReset)
{
	FBlastScopedProfiler FRP("FractureRadial");
	RandomGenerator->seed(RandomSeed);
	auto VoronoiSitesGenerator = GetVoronoiSitesGenerator(FractureSession, FractureChunkId, ForceReset);
	if (VoronoiSitesGenerator.IsValid())
	{
		Normal.Normalize();
		physx::PxVec3 O(Origin.X, Origin.Y, Origin.Z);
		physx::PxVec3 N(Normal.X, Normal.Y, Normal.Z);
		VoronoiSitesGenerator->radialPattern(O, N, Radius, AngularSteps, RadialSteps, AngleOffset, Variability);
		const physx::PxVec3* sites = nullptr;
		uint32_t sitesCount = VoronoiSitesGenerator->getVoronoiSites(sites);
		if (FractureSession->FractureTool->getChunkDepth(FractureChunkId) == 0)
		{
			IsReplace = false;
		}
		physx::PxVec3 ca(CellAnisotropy.X, CellAnisotropy.Y, CellAnisotropy.Z);
		physx::PxQuat cr(CellRotation.X, CellRotation.Y, CellRotation.Z, CellRotation.W);
		if (FractureSession->FractureTool->voronoiFracturing(FractureChunkId, sitesCount, sites, ca, cr, IsReplace) == 0)
		{
			return true;
		}
	}
	UE_LOG(LogBlastMeshEditor, Error, TEXT("Failed to fracture with Voronoi"));
	return false;
}

bool FBlastFracture::FractureInSphere(TSharedPtr<FFractureSession> FractureSession, uint32 FractureChunkId, int32 RandomSeed, bool IsReplace,
	uint32 CellCount, float Radius, FVector Origin, FVector CellAnisotropy, FQuat CellRotation, bool ForceReset)
{
	FBlastScopedProfiler FISP("FractureInSphere");
	RandomGenerator->seed(RandomSeed);
	auto VoronoiSitesGenerator = GetVoronoiSitesGenerator(FractureSession, FractureChunkId, ForceReset);
	if (VoronoiSitesGenerator.IsValid())
	{
		physx::PxVec3 O(Origin.X, Origin.Y, Origin.Z);
		VoronoiSitesGenerator->generateInSphere(CellCount, Radius, O);
		const physx::PxVec3* sites = nullptr;
		uint32_t sitesCount = VoronoiSitesGenerator->getVoronoiSites(sites);
		if (FractureSession->FractureTool->getChunkDepth(FractureChunkId) == 0)
		{
			IsReplace = false;
		}
		physx::PxVec3 ca(CellAnisotropy.X, CellAnisotropy.Y, CellAnisotropy.Z);
		physx::PxQuat cr(CellRotation.X, CellRotation.Y, CellRotation.Z, CellRotation.W);
		if (FractureSession->FractureTool->voronoiFracturing(FractureChunkId, sitesCount, sites, ca, cr, IsReplace) == 0)
		{
			return true;
		}
	}
	UE_LOG(LogBlastMeshEditor, Error, TEXT("Failed to fracture with Voronoi in sphere"));
	return false;
}

bool FBlastFracture::RemoveInSphere(TSharedPtr<FFractureSession> FractureSession, uint32 FractureChunkId, int32 RandomSeed, bool IsReplace,
	float Radius, FVector Origin, float Probability, bool ForceReset)
{
	FBlastScopedProfiler RISP("RemoveInSphere");
	RandomGenerator->seed(RandomSeed);
	auto VoronoiSitesGenerator = GetVoronoiSitesGenerator(FractureSession, FractureChunkId, ForceReset);
	if (VoronoiSitesGenerator.IsValid())
	{
		physx::PxVec3 O(Origin.X, Origin.Y, Origin.Z);
		VoronoiSitesGenerator->deleteInSphere(Radius, O, Probability);
		const physx::PxVec3* sites = nullptr;
		uint32_t sitesCount = VoronoiSitesGenerator->getVoronoiSites(sites);
		if (FractureSession->FractureTool->getChunkDepth(FractureChunkId) == 0)
		{
			IsReplace = false;
		}
		if (FractureSession->FractureTool->voronoiFracturing(FractureChunkId, sitesCount, sites, physx::PxVec3(1.f), physx::PxQuat(physx::PxIdentity), IsReplace) == 0)
		{
			return true;
		}
	}
	UE_LOG(LogBlastMeshEditor, Error, TEXT("Failed to fracture with Voronoi in sphere"));
	return false;
}

bool FBlastFracture::FractureUniformSlicing(TSharedPtr<FFractureSession> FractureSession, uint32 FractureChunkId, int32 RandomSeed, bool IsReplace,
	FIntVector SlicesCount, float AngleVariation, float OffsetVariation, 
	float NoiseAmplitude, float NoiseFrequency, int32 NoiseOctaveNumber, int32 SurfaceResolution)
{
	FBlastScopedProfiler FUSP("FractureUniformSlicing");
	RandomGenerator->seed(RandomSeed);
	Nv::Blast::SlicingConfiguration slConfig;
	slConfig.x_slices = SlicesCount.X;
	slConfig.y_slices = SlicesCount.Y;
	slConfig.z_slices = SlicesCount.Z;
	slConfig.angle_variations = AngleVariation;
	slConfig.offset_variations = OffsetVariation;
	slConfig.noise.amplitude = NoiseAmplitude;
	slConfig.noise.frequency = NoiseFrequency;
	slConfig.noise.octaveNumber = NoiseOctaveNumber;
	slConfig.noise.surfaceResolution = SurfaceResolution;
	if (FractureSession->FractureTool->getChunkDepth(FractureChunkId) == 0)
	{
		IsReplace = false;
	}
	if (FractureSession->FractureTool->slicing(FractureChunkId, slConfig, IsReplace, RandomGenerator.Get()) != 0)
	{
		UE_LOG(LogBlastMeshEditor, Error, TEXT("Failed to fracture with slicing"));
		return false;
	}
	return true;
}

bool FBlastFracture::FractureCutout(TSharedPtr<FFractureSession> FractureSession, uint32 FractureChunkId, int32 RandomSeed, bool IsReplace,
	UTexture2D* Pattern, FVector Origin, FVector Normal, FVector2D Size, float RotationZ, bool bPeriodic, bool bFillGaps,
	float NoiseAmplitude, float NoiseFrequency, int32 NoiseOctaveNumber, int32 SurfaceResolution)
{
	FBlastScopedProfiler FCP("FractureCutout");
	RandomGenerator->seed(RandomSeed);

	Normal.Normalize();
	auto UE4ToBlastTransform = UBlastMeshFactory::GetTransformUE4ToBlastCoordinateSystem(Cast<UFbxSkeletalMeshImportData>(FractureSession->BlastMesh->Mesh->AssetImportData));
	//FTransform ScaleTr; ScaleTr.SetScale3D(FVector(Scale.X, Scale.Y, 1.f) * 0.5f);
	FTransform YawTr(FQuat(FVector(0.f, 0.f, 1.f), FMath::DegreesToRadians(RotationZ)));
	FTransform Tr(FQuat::FindBetweenNormals(FVector(0.f, 0.f, 1.f), Normal), Origin);
	Tr = YawTr * Tr * UE4ToBlastTransform;

	Nv::Blast::CutoutConfiguration CutoutConfig;
	CutoutConfig.transform = physx::PxTransform(Tr.GetLocation().X, Tr.GetLocation().Y, Tr.GetLocation().Z,
		physx::PxQuat(Tr.GetRotation().X, Tr.GetRotation().Y, Tr.GetRotation().Z, Tr.GetRotation().W));
	CutoutConfig.scale = physx::PxVec2(Size.X, Size.Y);
	CutoutConfig.isRelativeTransform = false;
	CutoutConfig.noise.amplitude = NoiseAmplitude;
	CutoutConfig.noise.frequency = NoiseFrequency;
	CutoutConfig.noise.octaveNumber = NoiseOctaveNumber;
	CutoutConfig.noise.surfaceResolution = SurfaceResolution;
	if (Pattern != nullptr)
	{
		CutoutConfig.cutoutSet = NvBlastExtAuthoringCreateCutoutSet();
		
		TArray <uint8_t> Buf, Mip; 
		Pattern->Source.GetMipData(Mip, 0);
		int32 sz = Pattern->Source.GetSizeX() * Pattern->Source.GetSizeY();
		Buf.Reserve(sz * 3);
		for (int32 i = 0; i < sz; i++)
		{
			Buf.Push(Mip[i * 4]);
			Buf.Push(Mip[i * 4 + 1]);
			Buf.Push(Mip[i * 4 + 2]);
		}
		float SegmentationErrorThreshold = 1e-3; //Move this to advanced settings?
		float SnapThreshold = 1.f;
		NvBlastExtAuthoringBuildCutoutSet(*CutoutConfig.cutoutSet, Buf.GetData(), Pattern->Source.GetSizeX(), Pattern->Source.GetSizeY(), SegmentationErrorThreshold, SnapThreshold, bPeriodic, bFillGaps);
	}
	else
	{
		UE_LOG(LogBlastMeshEditor, Error, TEXT("Cutout Fracture: Texture with cutout pattern not found."));
		return false;
	}
	if (FractureSession->FractureTool->getChunkDepth(FractureChunkId) == 0)
	{
		IsReplace = false;
	}
	if (FractureSession->FractureTool->cutout(FractureChunkId, CutoutConfig, IsReplace, RandomGenerator.Get()) != 0)
	{
		UE_LOG(LogBlastMeshEditor, Error, TEXT("Failed to perform cutout fracture"));
		return false;
	}
	return true;
}

bool FBlastFracture::FractureCut(TSharedPtr<FFractureSession> FractureSession, uint32 FractureChunkId, int32 RandomSeed, bool IsReplace,
	FVector Origin, FVector Normal,
	float NoiseAmplitude, float NoiseFrequency, int32 NoiseOctaveNumber, int32 SurfaceResolution)
{
	Normal.Normalize();
	RandomGenerator->seed(RandomSeed);
	Nv::Blast::NoiseConfiguration NoiseConfig;
	NoiseConfig.amplitude = NoiseAmplitude;
	NoiseConfig.frequency = NoiseFrequency;
	NoiseConfig.octaveNumber = NoiseOctaveNumber;
	NoiseConfig.surfaceResolution = SurfaceResolution;
	if (FractureSession->FractureTool->getChunkDepth(FractureChunkId) == 0)
	{
		IsReplace = false;
	}
	if (FractureSession->FractureTool->cut(FractureChunkId, physx::PxVec3(Normal.X, Normal.Y, Normal.Z), physx::PxVec3(Origin.X, Origin.Y, Origin.Z), NoiseConfig, IsReplace, RandomGenerator.Get()) != 0)
	{
		UE_LOG(LogBlastMeshEditor, Error, TEXT("Failed to perform cut fracture"));
		return false;
	}
	return true;
}

static bool ParseBool(const FString& str, bool& value)
{
	if (str.Compare("true") == 0)
	{
		value = true;
		return true;
	}
	if (str.Compare("false") == 0)
	{
		value = false;
		return true;
	}
	return false;
}

TWeakPtr<FBlastFracture> FBlastFracture::Instance = nullptr;

TSharedPtr<FBlastFracture> FBlastFracture::GetInstance()
{
	TSharedPtr<FBlastFracture> SharedPtr;
	if (!Instance.IsValid())
	{
		SharedPtr = MakeShareable(new FBlastFracture());
		Instance = SharedPtr;
	}
	return Instance.Pin();
}

#undef LOCTEXT_NAMESPACE
