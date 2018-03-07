//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononScene.h"

#include "PhononCommon.h"
#include "PhononGeometryComponent.h"
#include "PhononMaterialComponent.h"

#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "SteamAudioSettings.h"

#if WITH_EDITOR

#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeDataAccess.h"

#endif // WITH_EDITOR

/*

 The scene export functions set up the following material index layout on the Phonon backend:

 <Presets>
 Default static mesh material
 Default BSP material
 Default landscape material
 <Custom static mesh materials>

 Note that it results in the CUSTOM preset being unused, but the code is simpler this way.

*/

namespace SteamAudio
{

#if WITH_EDITOR

	static void LoadBSPGeometry(UWorld* World, IPLhandle PhononScene, TArray<IPLhandle>* PhononStaticMeshes);
	static void LoadStaticMeshActors(UWorld* World, IPLhandle PhononScene, TArray<IPLhandle>* PhononStaticMeshes);
	static void LoadLandscapeActors(UWorld* World, IPLhandle PhononScene, TArray<IPLhandle>* PhononStaticMeshes);
	static void RegisterStaticMesh(IPLhandle PhononScene, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices, TArray<IPLhandle>* PhononStaticMeshes);
	static void SetCommonSceneMaterials(IPLhandle PhononScene);
	static int32 CalculateNumMaterials(UWorld* World);

	//==============================================================================================================================================
	// High level scene export
	//==============================================================================================================================================

	/**
	 * Loads scene geometry, providing handles to the Phonon scene object and Phonon static meshes.
	 */
	void LoadScene(UWorld* World, IPLhandle* PhononScene, TArray<IPLhandle>* PhononStaticMeshes)
	{
		check(World);
		check(PhononScene);
		check(PhononStaticMeshes);

		UE_LOG(LogSteamAudio, Log, TEXT("Loading Phonon scene."));

		IPLSimulationSettings SimulationSettings;
		SimulationSettings.sceneType = IPL_SCENETYPE_PHONON;
		IPLerror IplResult = iplCreateScene(SteamAudio::GlobalContext, nullptr, SimulationSettings, CalculateNumMaterials(World), PhononScene);
		if (IplResult != IPL_STATUS_SUCCESS)
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("Error creating Phonon scene."));
			return;
		}

		LoadStaticMeshActors(World, *PhononScene, PhononStaticMeshes);

		if (GetDefault<USteamAudioSettings>()->ExportLandscapeGeometry)
		{
			LoadLandscapeActors(World, *PhononScene, PhononStaticMeshes);
		}

		if (GetDefault<USteamAudioSettings>()->ExportBSPGeometry)
		{
			LoadBSPGeometry(World, *PhononScene, PhononStaticMeshes);
		}

		SetCommonSceneMaterials(*PhononScene);

		iplFinalizeScene(*PhononScene, nullptr);
	}

	//==============================================================================================================================================
	// Static mesh geometry export
	//==============================================================================================================================================

	/**
	 * Populates VertexArray with the given mesh's vertices. Converts from UE4 coords to Phonon coords. Returns the number of vertices added.
	 */
	static int32 GetMeshVerts(TActorIterator<AStaticMeshActor>& AStaticMeshItr, TArray<IPLVector3>& VertexArray)
	{
		check(AStaticMeshItr->GetStaticMeshComponent()->GetStaticMesh()->HasValidRenderData());

		int32 NumVerts = 0;

		auto& LODModel = AStaticMeshItr->GetStaticMeshComponent()->GetStaticMesh()->RenderData->LODResources[0];
		auto Indices = LODModel.IndexBuffer.GetArrayView();

		for (auto Section : LODModel.Sections)
		{
			for (auto TriIndex = 0; TriIndex < (int32)Section.NumTriangles; ++TriIndex)
			{
				auto BaseIndex = Section.FirstIndex + TriIndex * 3;
				for (auto v = 2; v > -1; v--)
				{
					auto i = Indices[BaseIndex + v];
					auto vtx = AStaticMeshItr->ActorToWorld().TransformPosition(LODModel.PositionVertexBuffer.VertexPosition(i));
					VertexArray.Add(UnrealToPhononIPLVector3(vtx));
					NumVerts++;
				}
			}
		}

		return NumVerts;
	}

	/**
	 * Walks up the actor attachment chain, checking for a Phonon Geometry component.
	 */
	static bool IsActorPhononGeometry(AActor* Actor)
	{
		auto CurrentActor = Actor;
		while (CurrentActor)
		{
			if (CurrentActor->GetComponentByClass(UPhononGeometryComponent::StaticClass()))
			{
				return true;
			}
			CurrentActor = CurrentActor->GetAttachParentActor();
		}

		return false;
	}

	static UPhononMaterialComponent* GetPhononMaterialComponent(AActor* Actor)
	{
		auto CurrentActor = Actor;
		while (CurrentActor)
		{
			if (CurrentActor->GetComponentByClass(UPhononMaterialComponent::StaticClass()))
			{
				return static_cast<UPhononMaterialComponent*>(CurrentActor->GetComponentByClass(UPhononMaterialComponent::StaticClass()));
			}
			CurrentActor = CurrentActor->GetAttachParentActor();
		}

		return nullptr;
	}

	/**
	 * Loads any static mesh actors, adding any Phonon static meshes to the provided array.
	 */
	static void LoadStaticMeshActors(UWorld* World, IPLhandle PhononScene, TArray<IPLhandle>* PhononStaticMeshes)
	{
		check(World);
		check(PhononScene);
		check(PhononStaticMeshes);

		UE_LOG(LogSteamAudio, Log, TEXT("Loading static mesh actors."));

		TArray<IPLVector3> IplVertices;
		TArray<IPLTriangle> IplTriangles;
		TArray<IPLint32> IplMaterialIndices;

		for (TActorIterator<AStaticMeshActor> AStaticMeshItr(World); AStaticMeshItr; ++AStaticMeshItr)
		{
			// Only consider static mesh actors that have both an acoustic geometry component attached and valid render data
			if (IsActorPhononGeometry(*AStaticMeshItr) && AStaticMeshItr->GetStaticMeshComponent()->GetStaticMesh() &&
				AStaticMeshItr->GetStaticMeshComponent()->GetStaticMesh()->HasValidRenderData())
			{
				auto PhononGeometryComponent = static_cast<UPhononGeometryComponent*>(
					AStaticMeshItr->GetComponentByClass(UPhononGeometryComponent::StaticClass()));

				auto StartVertexIdx = IplVertices.Num();
				auto NumMeshVerts = GetMeshVerts(AStaticMeshItr, IplVertices);

				for (auto i = 0; i < NumMeshVerts / 3; ++i)
				{
					IPLTriangle IplTriangle;
					IplTriangle.indices[0] = StartVertexIdx + i * 3;
					IplTriangle.indices[1] = StartVertexIdx + i * 3 + 1;
					IplTriangle.indices[2] = StartVertexIdx + i * 3 + 2;
					IplTriangles.Add(IplTriangle);
				}

				auto PhononMaterialComponent = GetPhononMaterialComponent(*AStaticMeshItr);
				auto MaterialIndex = 0;

				if (PhononMaterialComponent)
				{
					MaterialIndex = PhononMaterialComponent->MaterialIndex;
					iplSetSceneMaterial(PhononScene, PhononMaterialComponent->MaterialIndex, PhononMaterialComponent->GetMaterialPreset());
				}
				else
				{
					// The default static mesh material is always registered at size(MaterialPresets)
					MaterialIndex = MaterialPresets.Num();
				}

				for (auto i = 0; i < IplTriangles.Num(); ++i)
				{
					IplMaterialIndices.Add(MaterialIndex);
				}
			}
		}

		// Register a new static mesh with Phonon
		RegisterStaticMesh(PhononScene, IplVertices, IplTriangles, IplMaterialIndices, PhononStaticMeshes);
	}

	//==============================================================================================================================================
	// BSP geometry export
	//==============================================================================================================================================

	/**
	 * Loads any BSP geometry, adding any Phonon static meshes to the provided array.
	 */
	static void LoadBSPGeometry(UWorld* World, IPLhandle PhononScene, TArray<IPLhandle>* PhononStaticMeshes)
	{
		check(World);
		check(PhononScene);
		check(PhononStaticMeshes);

		UE_LOG(LogSteamAudio, Log, TEXT("Loading BSP geometry."));

		TArray<IPLVector3> IplVertices;
		TArray<IPLTriangle> IplTriangles;
		TArray<IPLint32> IplMaterialIndices;

		// Gather and convert all world vertices to Phonon coords
		for (auto& WorldVertex : World->GetModel()->Points)
		{
			IplVertices.Add(SteamAudio::UnrealToPhononIPLVector3(WorldVertex));
		}

		// Gather vertex indices for all faces ("nodes" are faces)
		for (auto& WorldNode : World->GetModel()->Nodes)
		{
			// Ignore degenerate faces
			if (WorldNode.NumVertices <= 2)
			{
				continue;
			}

			// Faces are organized as triangle fans
			int32 Index0 = World->GetModel()->Verts[WorldNode.iVertPool + 0].pVertex;
			int32 Index1 = World->GetModel()->Verts[WorldNode.iVertPool + 1].pVertex;
			int32 Index2;

			for (auto v = 2; v < WorldNode.NumVertices; ++v)
			{
				Index2 = World->GetModel()->Verts[WorldNode.iVertPool + v].pVertex;

				IPLTriangle IplTriangle;
				IplTriangle.indices[0] = Index0;
				IplTriangle.indices[1] = Index1;
				IplTriangle.indices[2] = Index2;
				IplTriangles.Add(IplTriangle);

				Index1 = Index2;
			}
		}

		// The default BSP material is always registered at size(MaterialPresets) + 1
		auto MaterialIdx = MaterialPresets.Num() + 1;
		for (auto i = 0; i < IplTriangles.Num(); ++i)
		{
			IplMaterialIndices.Add(MaterialIdx);
		}

		// Register a new static mesh with Phonon
		RegisterStaticMesh(PhononScene, IplVertices, IplTriangles, IplMaterialIndices, PhononStaticMeshes);
	}

	//==============================================================================================================================================
	// Landscape geometry export
	//==============================================================================================================================================

	/**
	 * Loads any Landscape actors, adding any Phonon static meshes to the provided array.
	 */
	static void LoadLandscapeActors(UWorld* World, IPLhandle PhononScene, TArray<IPLhandle>* PhononStaticMeshes)
	{
		check(World);
		check(PhononScene);
		check(PhononStaticMeshes);

		UE_LOG(LogSteamAudio, Log, TEXT("Loading landscape actors."));

		TArray<IPLVector3> IplVertices;
		TArray<IPLTriangle> IplTriangles;
		TArray<IPLint32> IplMaterialIndices;

		for (TActorIterator<ALandscape> ALandscapeItr(World); ALandscapeItr; ++ALandscapeItr)
		{
			ULandscapeInfo* LandscapeInfo = ALandscapeItr->GetLandscapeInfo();

			for (auto It = LandscapeInfo->XYtoComponentMap.CreateIterator(); It; ++It)
			{
				ULandscapeComponent* Component = It.Value();
				FLandscapeComponentDataInterface CDI(Component);

				for (auto y = 0; y < Component->ComponentSizeQuads; ++y)
				{
					for (auto x = 0; x < Component->ComponentSizeQuads; ++x)
					{
						auto StartIndex = IplVertices.Num();

						IplVertices.Add(SteamAudio::UnrealToPhononIPLVector3(CDI.GetWorldVertex(x, y)));
						IplVertices.Add(SteamAudio::UnrealToPhononIPLVector3(CDI.GetWorldVertex(x, y + 1)));
						IplVertices.Add(SteamAudio::UnrealToPhononIPLVector3(CDI.GetWorldVertex(x + 1, y + 1)));
						IplVertices.Add(SteamAudio::UnrealToPhononIPLVector3(CDI.GetWorldVertex(x + 1, y)));

						IPLTriangle Triangle;

						Triangle.indices[0] = StartIndex;
						Triangle.indices[1] = StartIndex + 2;
						Triangle.indices[2] = StartIndex + 3;
						IplTriangles.Add(Triangle);

						Triangle.indices[0] = StartIndex;
						Triangle.indices[1] = StartIndex + 1;
						Triangle.indices[2] = StartIndex + 2;
						IplTriangles.Add(Triangle);
					}
				}
			}
		}

		// The default landscape material is always registered at size(MaterialPresets) + 2
		auto MaterialIdx = MaterialPresets.Num() + 2;
		for (auto i = 0; i < IplTriangles.Num(); ++i)
		{
			IplMaterialIndices.Add(MaterialIdx);
		}

		// Register a new static mesh with Phonon
		RegisterStaticMesh(PhononScene, IplVertices, IplTriangles, IplMaterialIndices, PhononStaticMeshes);
	}

	//==============================================================================================================================================
	// Utility functions
	//==============================================================================================================================================

	/**
	 * Registers a new static mesh with Phonon, adding its handle to the provided array of static meshes.
	 */
	static void RegisterStaticMesh(IPLhandle PhononScene, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices, TArray<IPLhandle>* PhononStaticMeshes)
	{
		if (IplVertices.Num() > 0)
		{
			UE_LOG(LogSteamAudio, Log, TEXT("Registering new mesh with %d verts."), IplVertices.Num());

			IPLhandle IplStaticMesh = nullptr;
			auto IplResult = iplCreateStaticMesh(PhononScene, IplVertices.Num(), IplTriangles.Num(), &IplStaticMesh);
			if (IplResult != IPL_STATUS_SUCCESS)
			{
				UE_LOG(LogSteamAudio, Warning, TEXT("Error adding a new object to the acoustic scene."));
				return;
			}

			iplSetStaticMeshMaterials(PhononScene, IplStaticMesh, IplMaterialIndices.GetData());
			iplSetStaticMeshVertices(PhononScene, IplStaticMesh, IplVertices.GetData());
			iplSetStaticMeshTriangles(PhononScene, IplStaticMesh, IplTriangles.GetData());
			PhononStaticMeshes->Add(IplStaticMesh);
		}
		else
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("Skipping mesh registration because no vertices were found."));
		}
	}

	/**
	 * Calculates the total number of materials that must be registered with Phonon. This includes presets and any custom materials.
	 */
	static int32 CalculateNumMaterials(UWorld* World)
	{
		check(World);

		// There are size(MaterialPresets) + 3 fixed slots.
		int32 NumMaterials = MaterialPresets.Num() + 3;

		for (TActorIterator<AActor> AActorItr(World); AActorItr; ++AActorItr)
		{
			auto PhononMaterialComponent = static_cast<UPhononMaterialComponent*>(
				AActorItr->GetComponentByClass(UPhononMaterialComponent::StaticClass()));
			if (PhononMaterialComponent)
			{
				if (PhononMaterialComponent->MaterialPreset == EPhononMaterial::CUSTOM)
				{
					PhononMaterialComponent->MaterialIndex = NumMaterials++;
				}
				else
				{
					PhononMaterialComponent->MaterialIndex = static_cast<int32>(PhononMaterialComponent->MaterialPreset);
				}
			}
		}

		return NumMaterials;
	}

	/**
	 * Registers any presets and default materials for static mesh actors, BSP geometry, and landscape actors.
	 */
	static void SetCommonSceneMaterials(IPLhandle PhononScene)
	{
		check(PhononScene);

		for (const auto& Preset : MaterialPresets)
		{
			iplSetSceneMaterial(PhononScene, static_cast<int32>(Preset.Key), Preset.Value);
		}

		iplSetSceneMaterial(PhononScene, MaterialPresets.Num(), GetDefault<USteamAudioSettings>()->GetDefaultStaticMeshMaterial());
		iplSetSceneMaterial(PhononScene, MaterialPresets.Num() + 1, GetDefault<USteamAudioSettings>()->GetDefaultBSPMaterial());
		iplSetSceneMaterial(PhononScene, MaterialPresets.Num() + 2, GetDefault<USteamAudioSettings>()->GetDefaultLandscapeMaterial());
	}

#endif // WITH_EDITOR

	uint32 GetNumTrianglesForStaticMesh(AStaticMeshActor* StaticMeshActor)
	{
		auto NumTriangles = 0;

		if (StaticMeshActor == nullptr)
		{
			return NumTriangles;
		}

		const auto& LODModel = StaticMeshActor->GetStaticMeshComponent()->GetStaticMesh()->RenderData->LODResources[0];
		for (const auto& Section : LODModel.Sections)
		{
			NumTriangles += Section.NumTriangles;
		}

		return NumTriangles;
	}

	uint32 GetNumTrianglesAtRoot(AActor* RootActor)
	{
		auto NumTriangles = 0;

		if (RootActor == nullptr)
		{
			return NumTriangles;
		}

		NumTriangles = GetNumTrianglesForStaticMesh(Cast<AStaticMeshActor>(RootActor));

		TArray<AActor*> AttachedActors;
		RootActor->GetAttachedActors(AttachedActors);

		for (auto AttachedActor : AttachedActors)
		{
			NumTriangles += GetNumTrianglesAtRoot(AttachedActor);
		}

		return NumTriangles;
	}
}