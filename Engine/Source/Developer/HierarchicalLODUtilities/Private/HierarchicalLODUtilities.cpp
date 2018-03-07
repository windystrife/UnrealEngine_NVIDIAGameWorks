// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HierarchicalLODUtilities.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/LODActor.h"
#include "Components/BrushComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Model.h"
#include "Engine/Polys.h"
#include "HierarchicalLODUtilitiesModule.h"

#include "MeshUtilities.h"
#include "StaticMeshResources.h"
#include "HierarchicalLODVolume.h"

#include "Interfaces/IProjectManager.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

#include "BSPOps.h"
#include "Builders/CubeBuilder.h"

#include "AssetRegistryModule.h" 
#include "Engine/LevelStreaming.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Toolkits/AssetEditorManager.h"
#include "ScopedTransaction.h"
#include "PackageTools.h"
#endif // WITH_EDITOR

#include "HierarchicalLODProxyProcessor.h"
#include "IMeshReductionManagerModule.h"
#include "MeshMergeModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogHierarchicalLODUtilities, Verbose, All);

#define LOCTEXT_NAMESPACE "HierarchicalLODUtils"

void FHierarchicalLODUtilities::ExtractStaticMeshComponentsFromLODActor(AActor* Actor, TArray<UStaticMeshComponent*>& InOutComponents)
{
	ALODActor* LODActor = CastChecked<ALODActor>(Actor);
	for (auto& ChildActor : LODActor->SubActors)
	{
		TArray<UStaticMeshComponent*> ChildComponents;
		if (ChildActor->IsA<ALODActor>())
		{
			ExtractStaticMeshComponentsFromLODActor(ChildActor, ChildComponents);
		}
		else
		{
			ChildActor->GetComponents<UStaticMeshComponent>(ChildComponents);
		}

		InOutComponents.Append(ChildComponents);
	}
}

void FHierarchicalLODUtilities::ExtractSubActorsFromLODActor(AActor* Actor, TArray<AActor*>& InOutActors)
{
	ALODActor* LODActor = CastChecked<ALODActor>(Actor);
	for (auto& ChildActor : LODActor->SubActors)
	{
		TArray<AActor*> ChildActors;
		if (ChildActor->IsA<ALODActor>())
		{
			ExtractSubActorsFromLODActor(ChildActor, ChildActors);
		}
		else
		{
			ChildActors.Add(ChildActor);
		}

		InOutActors.Append(ChildActors);
	}
}

float FHierarchicalLODUtilities::CalculateScreenSizeFromDrawDistance(const float SphereRadius, const FMatrix& ProjectionMatrix, const float Distance)
{
	return ComputeBoundsScreenSize(FVector::ZeroVector, SphereRadius, FVector(0.0f, 0.0f, Distance), ProjectionMatrix);
}

float FHierarchicalLODUtilities::CalculateDrawDistanceFromScreenSize(const float SphereRadius, const float ScreenSize, const FMatrix& ProjectionMatrix)
{
	return ComputeBoundsDrawDistance(ScreenSize, SphereRadius, ProjectionMatrix);
}

UPackage* FHierarchicalLODUtilities::CreateOrRetrieveLevelHLODPackage(ULevel* InLevel)
{
	checkf(InLevel != nullptr, TEXT("Invalid Level supplied"));

	UPackage* HLODPackage = nullptr;
	UPackage* LevelOuterMost = InLevel->GetOutermost();

	const FString PathName = FPackageName::GetLongPackagePath(LevelOuterMost->GetPathName());
	const FString BaseName = FPackageName::GetShortName(LevelOuterMost->GetPathName());
	const FString HLODLevelPackageName = FString::Printf(TEXT("%s/HLOD/%s_HLOD"), *PathName, *BaseName);

	HLODPackage = CreatePackage(NULL, *HLODLevelPackageName);
	HLODPackage->FullyLoad();
	HLODPackage->Modify();
	
	// Target level filename
	const FString HLODLevelFileName = FPackageName::LongPackageNameToFilename(HLODLevelPackageName);
	// This is a hack to avoid save file dialog when we will be saving HLOD map package
	HLODPackage->FileName = FName(*HLODLevelFileName);

	return HLODPackage;	
}

bool FHierarchicalLODUtilities::BuildStaticMeshForLODActor(ALODActor* LODActor, UPackage* AssetsOuter, const FHierarchicalSimplification& LODSetup)
{
	if (AssetsOuter && LODActor)
	{
		if (!LODActor->IsDirty())
		{
			return false;
		}

		UE_LOG(LogHierarchicalLODUtilities, Log, TEXT("Building Proxy Mesh for Cluster %s"), *LODActor->GetName());
		const FScopedTransaction Transaction(LOCTEXT("UndoAction_BuildProxyMesh", "Building Proxy Mesh for Cluster"));
		LODActor->Modify();		

		// Delete actor assets before generating new ones
		FHierarchicalLODUtilities::DestroyClusterData(LODActor);	
		
		TArray<UPrimitiveComponent*> AllComponents;
		{
			for (auto& Actor : LODActor->SubActors)
			{
				TArray<UStaticMeshComponent*> Components;

				if (Actor->IsA<ALODActor>())
				{
					ExtractStaticMeshComponentsFromLODActor(Actor, Components);
				}
				else
				{
					Actor->GetComponents<UStaticMeshComponent>(Components);
				}

				// TODO: support instanced static meshes
				Components.RemoveAll([](UStaticMeshComponent* Val)
				{ 
					return Val->IsA(UInstancedStaticMeshComponent::StaticClass()) || !Val->ShouldGenerateAutoLOD();
				});

				AllComponents.Append(Components);
			}
		}

		// it shouldn't even have come here if it didn't have any staticmesh
		if (ensure(AllComponents.Num() > 0))
		{
			// In case we don't have outer generated assets should have same path as LOD level
			const FString AssetsPath = AssetsOuter->GetName() + TEXT("/");
			AActor* FirstActor = LODActor->SubActors[0];

			TArray<UObject*> OutAssets;
			FVector OutProxyLocation = FVector::ZeroVector;
			UStaticMesh* MainMesh = nullptr;

			// Generate proxy mesh and proxy material assets
			IMeshReductionManagerModule& MeshReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");

			const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
			// should give unique name, so use level + actor name
			const FString PackageName = FString::Printf(TEXT("LOD_%s"), *FirstActor->GetName());
			if (MeshReductionModule.GetMeshMergingInterface() && LODSetup.bSimplifyMesh)
			{
				TArray<AActor*> Actors;
				ExtractSubActorsFromLODActor(LODActor, Actors);

				FHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<FHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
				FHierarchicalLODProxyProcessor* Processor = Module.GetProxyProcessor();

				FHierarchicalSimplification OverrideLODSetup = LODSetup;

				FMeshProxySettings ProxySettings = LODSetup.ProxySetting;
				if (LODActor->bOverrideMaterialMergeSettings)
				{
					ProxySettings.MaterialSettings = LODActor->MaterialSettings;
				}

				if (LODActor->bOverrideScreenSize)
				{
					ProxySettings.ScreenSize = LODActor->ScreenSize;
				}

				if (LODActor->bOverrideTransitionScreenSize)
				{
					OverrideLODSetup.TransitionScreenSize = LODActor->TransitionScreenSize;
				}

				FGuid JobID = Processor->AddProxyJob(LODActor, OverrideLODSetup);
				MeshMergeUtilities.CreateProxyMesh(Actors, ProxySettings, AssetsOuter, PackageName, JobID, Processor->GetCallbackDelegate(), true, OverrideLODSetup.TransitionScreenSize);
				return true;
			}
			else
			{
				FMeshMergingSettings MergeSettings = LODSetup.MergeSetting;
				if (LODActor->bOverrideMaterialMergeSettings)
				{
					MergeSettings.MaterialSettings = LODActor->MaterialSettings;
				}

				MeshMergeUtilities.MergeComponentsToStaticMesh(AllComponents, FirstActor->GetWorld(), MergeSettings, AssetsOuter, PackageName, OutAssets, OutProxyLocation, LODSetup.TransitionScreenSize, true);
				
				// set staticmesh
				for (auto& Asset : OutAssets)
				{
					UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset);

					if (StaticMesh)
					{
						MainMesh = StaticMesh;
					}
				}

				if (!MainMesh)
				{
					return false;
				}

				// make sure the mesh won't affect navmesh generation
				MainMesh->MarkAsNotHavingNavigationData();

				LODActor->SetStaticMesh(MainMesh);
				LODActor->SetActorLocation(OutProxyLocation);
				LODActor->SubObjects = OutAssets;

				// Check resulting mesh and give a warning if it exceeds the vertex / triangle cap for certain platforms
				FProjectStatus ProjectStatus;
				if (IProjectManager::Get().QueryStatusForCurrentProject(ProjectStatus) && (ProjectStatus.IsTargetPlatformSupported(TEXT("Android")) || ProjectStatus.IsTargetPlatformSupported(TEXT("IOS"))))
				{
					if (MainMesh->RenderData.IsValid() && MainMesh->RenderData->LODResources.Num() && MainMesh->RenderData->LODResources[0].IndexBuffer.Is32Bit())
					{
						FMessageLog("HLODResults").Warning()							
							->AddToken(FUObjectToken::Create(LODActor))
							->AddToken(FTextToken::Create(LOCTEXT("HLODError_MeshNotBuildTwo", " Mesh has more that 65535 vertices, incompatible with mobile; forcing 16-bit (will probably cause rendering issues).")));
					}
				}

				// At the moment this assumes a fixed field of view of 90 degrees (horizontal and vertical axi)
				static const float FOVRad = 90.0f * (float)PI / 360.0f;
				static const FMatrix ProjectionMatrix = FPerspectiveMatrix(FOVRad, 1920, 1080, 0.01f);
				FBoxSphereBounds Bounds = LODActor->GetStaticMeshComponent()->CalcBounds(FTransform());
				LODActor->LODDrawDistance = CalculateDrawDistanceFromScreenSize(Bounds.SphereRadius, LODSetup.TransitionScreenSize, ProjectionMatrix);
				LODActor->GetStaticMeshComponent()->MinDrawDistance = LODActor->LODDrawDistance;
				LODActor->UpdateSubActorLODParents();

				// Freshly build so mark not dirty
				LODActor->SetIsDirty(false);

				return true;
			}
		}
	}
	return false;
}

EClusterGenerationError FHierarchicalLODUtilities::ShouldGenerateCluster(AActor* Actor)
{
	if (!Actor)
	{
		return EClusterGenerationError::InvalidActor;
	}

	if (Actor->bHidden)
	{
		return EClusterGenerationError::ActorHiddenInGame;
	}

	if (!Actor->bEnableAutoLODGeneration)
	{
		return EClusterGenerationError::ExcludedActor;
	}

	ALODActor* LODActor = Cast<ALODActor>(Actor);
	if (LODActor)
	{
		return EClusterGenerationError::LODActor;
	}

	FVector Origin, Extent;
	Actor->GetActorBounds(false, Origin, Extent);
	if (Extent.SizeSquared() <= 0.1)
	{
		return EClusterGenerationError::ActorTooSmall;
	}

	// for now only consider staticmesh - I don't think skel mesh would work with simplygon merge right now @fixme
	TArray<UStaticMeshComponent*> Components;
	Actor->GetComponents<UStaticMeshComponent>(Components);
	// TODO: support instanced static meshes
	Components.RemoveAll([](UStaticMeshComponent* Val){ return Val->IsA(UInstancedStaticMeshComponent::StaticClass()); });

	int32 ValidComponentCount = 0;
	// now make sure you check parent primitive, so that we don't build for the actor that already has built. 

	EClusterGenerationError ErrorType = EClusterGenerationError::None;

	if (Components.Num() > 0)
	{
		for (auto& ComponentIter : Components)
		{
			if (ComponentIter->GetLODParentPrimitive())
			{
				return EClusterGenerationError::AlreadyClustered;
			}

			if (ComponentIter->bHiddenInGame)
			{
				return EClusterGenerationError::ComponentHiddenInGame;
			}

			// see if we should generate it
			if (ComponentIter->ShouldGenerateAutoLOD())
			{
				++ValidComponentCount;
				ErrorType |= EClusterGenerationError::ValidActor;
			}
			else
			{
				ErrorType |= (ComponentIter->bEnableAutoLODGeneration ? EClusterGenerationError::MoveableComponent: EClusterGenerationError::ExcludedComponent);
			}
		}
	}

	return ErrorType;
}

ALODActor* FHierarchicalLODUtilities::GetParentLODActor(const AActor* InActor)
{
	ALODActor* ParentActor = nullptr;
	if (InActor)
	{
		TArray<UStaticMeshComponent*> ComponentArray;
		InActor->GetComponents<UStaticMeshComponent>(ComponentArray);
		for (auto Component : ComponentArray)
		{
			UPrimitiveComponent* ParentComponent = Component->GetLODParentPrimitive();
			if (ParentComponent)
			{
				ParentActor = CastChecked<ALODActor>(ParentComponent->GetOwner());
				break;
			}
		}
	}

	return ParentActor;
}

void FHierarchicalLODUtilities::DestroyCluster(ALODActor* InActor)
{
	// Find if it has a parent ALODActor
	AActor* Actor = InActor;
	UWorld* World = Actor->GetWorld();
	ALODActor* ParentLOD = GetParentLODActor(InActor);

	const FScopedTransaction Transaction(LOCTEXT("UndoAction_DeleteCluster", "Deleting a (invalid) Cluster"));
	Actor->Modify();
	World->Modify();
	if (ParentLOD != nullptr)
	{
		ParentLOD->Modify();
		ParentLOD->RemoveSubActor(Actor);
	}

	// Clean out sub actors and update their LODParent
	while (InActor->SubActors.Num())
	{
		AActor* SubActor = InActor->SubActors[0];
		SubActor->Modify();
		InActor->RemoveSubActor(SubActor);
	}

	// Also destroy the cluster's data
	DestroyClusterData(InActor);

	World->DestroyActor(InActor);

	if (ParentLOD != nullptr && !ParentLOD->HasAnySubActors())
	{
		DestroyCluster(ParentLOD);
	}
}

void FHierarchicalLODUtilities::DestroyClusterData(ALODActor* InActor)
{
	TArray<UObject*> AssetsToDelete;
	AssetsToDelete.Append(InActor->SubObjects);
	InActor->SubObjects.Empty();

	for (UObject* AssetObject : AssetsToDelete)
	{
		if (AssetObject)
		{
#if WITH_EDITOR
			// Close possible open editors using this asset	
			FAssetEditorManager::Get().CloseAllEditorsForAsset(AssetObject);
			InActor->PreviousSubObjects.Add(AssetObject);
#endif // WITH_EDITOR
		}
	}

	// Set Static Mesh to null since there isn't a mesh anymore
	InActor->GetStaticMeshComponent()->SetStaticMesh(nullptr);
}

ALODActor* FHierarchicalLODUtilities::CreateNewClusterActor(UWorld* InWorld, const int32 InLODLevel, AWorldSettings* WorldSettings)
{
	// Check incoming data
	check(InWorld != nullptr && WorldSettings != nullptr && InLODLevel >= 0);
	if (!WorldSettings->bEnableHierarchicalLODSystem || WorldSettings->HierarchicalLODSetup.Num() == 0 || WorldSettings->HierarchicalLODSetup.Num() < InLODLevel)
	{
		return nullptr;
	}

	// Spawn and setup actor
	ALODActor* NewActor = InWorld->SpawnActor<ALODActor>(ALODActor::StaticClass(), FTransform());
	NewActor->LODLevel = InLODLevel + 1;
	NewActor->LODDrawDistance = 0.0f;
	NewActor->SetStaticMesh(nullptr);
	NewActor->PostEditChange();

	return NewActor;
}

ALODActor* FHierarchicalLODUtilities::CreateNewClusterFromActors(UWorld* InWorld, AWorldSettings* WorldSettings, const TArray<AActor*>& InActors, const int32 InLODLevel /*= 0*/)
{
	checkf(InWorld != nullptr, TEXT("Invalid world"));
	checkf(InActors.Num() > 0, TEXT("Zero number of sub actors"));
	checkf(WorldSettings != nullptr, TEXT("Invalid world settings"));
	checkf(WorldSettings->bEnableHierarchicalLODSystem, TEXT("Hierarchical LOD system is disabled"));

	const FScopedTransaction Transaction(LOCTEXT("UndoAction_CreateNewCluster", "Create new Cluster"));
	InWorld->Modify();

	// Create the cluster
	ALODActor* NewCluster = CreateNewClusterActor(InWorld, InLODLevel, WorldSettings);
	checkf(NewCluster != nullptr, TEXT("Failed to create a new cluster"));

	// Add InActors to new cluster
	for (AActor* Actor : InActors)
	{
		checkf(Actor != nullptr, TEXT("Invalid actor in InActors"));
		
		// Check if Actor is currently part of a different cluster
		ALODActor* ParentActor = GetParentLODActor(Actor);
		if (ParentActor != nullptr)
		{
			// If so remove it first
			ParentActor->Modify();
			ParentActor->RemoveSubActor(Actor);

			// If the parent cluster is now empty (invalid) destroy it
			if (!ParentActor->HasAnySubActors())
			{
				DestroyCluster(ParentActor);
			}
		}

		// Add actor to new cluster
		NewCluster->AddSubActor(Actor);
	}

	// Update sub actor LOD parents to populate 
	NewCluster->UpdateSubActorLODParents();

	return NewCluster;
}

const bool FHierarchicalLODUtilities::RemoveActorFromCluster(AActor* InActor)
{
	checkf(InActor != nullptr, TEXT("Invalid InActor"));
	
	bool bSucces = false;

	ALODActor* ParentActor = GetParentLODActor(InActor);
	if (ParentActor != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("UndoAction_RemoveActorFromCluster", "Remove Actor From Cluster"));
		ParentActor->Modify();
		InActor->Modify();

		bSucces = ParentActor->RemoveSubActor(InActor);

		if (!ParentActor->HasAnySubActors())
		{
			DestroyCluster(ParentActor);
		}
	}
	
	return bSucces;
}

const bool FHierarchicalLODUtilities::AddActorToCluster(AActor* InActor, ALODActor* InParentActor)
{
	checkf(InActor != nullptr, TEXT("Invalid InActor"));
	checkf(InParentActor != nullptr, TEXT("Invalid InParentActor"));

	// First, if it is the case remove the actor from it's current cluster
	const bool bActorWasClustered = RemoveActorFromCluster(InActor);

	// Now add it to the new one
	const FScopedTransaction Transaction(LOCTEXT("UndoAction_AddActorToCluster", "Add Actor To Cluster"));
	InParentActor->Modify();
	InActor->Modify();

	// Add InActor to InParentActor cluster
	InParentActor->AddSubActor(InActor);

#if WITH_EDITOR
	GEditor->BroadcastHLODActorAdded(InActor, InParentActor);
#endif // WITH_EDITOR

	return true;
}

const bool FHierarchicalLODUtilities::MergeClusters(ALODActor* TargetCluster, ALODActor* SourceCluster)
{
	checkf(TargetCluster != nullptr&& TargetCluster->SubActors.Num() > 0, TEXT("Invalid InActor"));
	checkf(SourceCluster != nullptr && SourceCluster->SubActors.Num() > 0, TEXT("Invalid InParentActor"));

	const FScopedTransaction Transaction(LOCTEXT("UndoAction_MergeClusters", "Merge Clusters"));
	TargetCluster->Modify();
	SourceCluster->Modify();

	while (SourceCluster->SubActors.Num())
	{
		AActor* SubActor = SourceCluster->SubActors.Last();
		AddActorToCluster(SubActor, TargetCluster);		
	}

	if (!SourceCluster->HasAnySubActors())
	{
		DestroyCluster(SourceCluster);
	}

	return true;
}

const bool FHierarchicalLODUtilities::AreActorsInSamePersistingLevel(const TArray<AActor*>& InActors)
{
	ULevel* Level = nullptr;
	bool bSameLevelInstance = true;
	for (AActor* Actor : InActors)
	{
		if (Level == nullptr)
		{
			Level = Actor->GetLevel();
		}

		bSameLevelInstance &= (Level == Actor->GetLevel());

		if (!bSameLevelInstance)
		{
			break;
		}
	}

	return bSameLevelInstance;
}

const bool FHierarchicalLODUtilities::AreClustersInSameHLODLevel(const TArray<ALODActor*>& InLODActors)
{
	int32 HLODLevel = -1;
	bool bSameHLODLevel = true;
	for (ALODActor* LODActor : InLODActors)
	{
		if (HLODLevel == -1)
		{
			HLODLevel = LODActor->LODLevel;
		}

		bSameHLODLevel &= (HLODLevel == LODActor->LODLevel);

		if (!bSameHLODLevel)
		{
			break;
		}
	}

	return bSameHLODLevel;
}

const bool FHierarchicalLODUtilities::AreActorsInSameHLODLevel(const TArray<AActor*>& InActors)
{
	int32 HLODLevel = -1;
	bool bSameHLODLevel = true;
	for (AActor* Actor : InActors)
	{
		ALODActor* ParentActor = FHierarchicalLODUtilities::GetParentLODActor(Actor);

		if (ParentActor != nullptr)
		{
			if (HLODLevel == -1)
			{
				HLODLevel = ParentActor->LODLevel;
			}

			bSameHLODLevel &= (HLODLevel == ParentActor->LODLevel);
		}
		else
		{
			bSameHLODLevel = false;
		}

		if (!bSameHLODLevel)
		{
			break;
		}
	}

	return bSameHLODLevel;
}

const bool FHierarchicalLODUtilities::AreActorsClustered(const TArray<AActor*>& InActors)
{	
	bool bClustered = true;
	for (AActor* Actor : InActors)
	{
		bClustered &= (GetParentLODActor(Actor) != nullptr);

		if (!bClustered)
		{
			break;
		}
	}

	return bClustered;
}

const bool FHierarchicalLODUtilities::IsActorClustered(const AActor* InActor)
{
	bool bClustered = (GetParentLODActor(InActor) != nullptr);	
	return bClustered;
}

void FHierarchicalLODUtilities::ExcludeActorFromClusterGeneration(AActor* InActor)
{
	const FScopedTransaction Transaction(LOCTEXT("UndoAction_ExcludeActorFromClusterGeneration", "Exclude Actor From Cluster Generation"));
	InActor->Modify();
	InActor->bEnableAutoLODGeneration = false;
	RemoveActorFromCluster(InActor);
}

void FHierarchicalLODUtilities::DestroyLODActor(ALODActor* InActor)
{
	const FScopedTransaction Transaction(LOCTEXT("UndoAction_DeleteLODActor", "Delete LOD Actor"));
	UWorld* World = InActor->GetWorld();
	World->Modify();
	InActor->Modify();
	
	ALODActor* ParentActor = GetParentLODActor(InActor);

	DestroyCluster(InActor);
	World->DestroyActor(InActor);

	if (ParentActor && !ParentActor->HasAnySubActors())
	{
		ParentActor->Modify();
		DestroyLODActor(ParentActor);
	}
}

void FHierarchicalLODUtilities::ExtractStaticMeshActorsFromLODActor(ALODActor* LODActor, TArray<AActor*> &InOutActors)
{
	for (auto ChildActor : LODActor->SubActors)
	{
		if (ChildActor)
		{
			TArray<AActor*> ChildActors;
			if (ChildActor->IsA<ALODActor>())
			{
				ExtractStaticMeshActorsFromLODActor(Cast<ALODActor>(ChildActor), ChildActors);
			}

			ChildActors.Push(ChildActor);
			InOutActors.Append(ChildActors);
		}
	}	
}

void FHierarchicalLODUtilities::DeleteLODActorsInHLODLevel(UWorld* InWorld, const int32 HLODLevelIndex)
{
	// you still have to delete all objects just in case they had it and didn't want it anymore
	TArray<UObject*> AssetsToDelete;
	for (int32 ActorId = InWorld->PersistentLevel->Actors.Num() - 1; ActorId >= 0; --ActorId)
	{
		ALODActor* LodActor = Cast<ALODActor>(InWorld->PersistentLevel->Actors[ActorId]);
		if (LodActor && LodActor->LODLevel == (HLODLevelIndex + 1))
		{
			DestroyCluster(LodActor);
			InWorld->DestroyActor(LodActor);
		}
	}
}

int32 FHierarchicalLODUtilities::ComputeStaticMeshLODLevel(const TArray<FStaticMeshSourceModel>& SourceModels, const FStaticMeshRenderData* RenderData, const float ScreenSize)
{	
	const int32 NumLODs = SourceModels.Num();
	// Walk backwards and return the first matching LOD
	for (int32 LODIndex = NumLODs - 1; LODIndex >= 0; --LODIndex)
	{
		if (SourceModels[LODIndex].ScreenSize > ScreenSize || ((SourceModels[LODIndex].ScreenSize == 0.0f) && (RenderData->ScreenSize[LODIndex] != SourceModels[LODIndex].ScreenSize) && (RenderData->ScreenSize[LODIndex] > ScreenSize)))
		{
			return FMath::Max(LODIndex, 0);
		}
	}

	return 0;
}

int32 FHierarchicalLODUtilities::GetLODLevelForScreenSize(const UStaticMeshComponent* StaticMeshComponent, const float ScreenSize)
{
	check(StaticMeshComponent != nullptr);
	const FStaticMeshRenderData* RenderData = StaticMeshComponent->GetStaticMesh()->RenderData.Get();
	checkf(RenderData != nullptr, TEXT("StaticMesh in StaticMeshComponent %s contains invalid render data"), *StaticMeshComponent->GetName());
	checkf(StaticMeshComponent->GetStaticMesh()->SourceModels.Num() > 0, TEXT("StaticMesh in StaticMeshComponent %s contains no SourceModels"), *StaticMeshComponent->GetName());

	return ComputeStaticMeshLODLevel(StaticMeshComponent->GetStaticMesh()->SourceModels, RenderData, ScreenSize);
}

AHierarchicalLODVolume* FHierarchicalLODUtilities::CreateVolumeForLODActor(ALODActor* InLODActor, UWorld* InWorld)
{
	FBox BoundingBox = InLODActor->GetComponentsBoundingBox(true);

	AHierarchicalLODVolume* Volume = InWorld->SpawnActor<AHierarchicalLODVolume>(AHierarchicalLODVolume::StaticClass(), FTransform(BoundingBox.GetCenter()));

	// this code builds a brush for the new actor
	Volume->PreEditChange(NULL);

	Volume->PolyFlags = 0;
	Volume->Brush = NewObject<UModel>(Volume, NAME_None, RF_Transactional);
	Volume->Brush->Initialize(nullptr, true);
	Volume->Brush->Polys = NewObject<UPolys>(Volume->Brush, NAME_None, RF_Transactional);
	Volume->GetBrushComponent()->Brush = Volume->Brush;
	Volume->BrushBuilder = NewObject<UCubeBuilder>(Volume, NAME_None, RF_Transactional);

	UCubeBuilder* CubeBuilder = static_cast<UCubeBuilder*>(Volume->BrushBuilder);

	CubeBuilder->X = BoundingBox.GetSize().X * 1.5f;
	CubeBuilder->Y = BoundingBox.GetSize().Y * 1.5f;
	CubeBuilder->Z = BoundingBox.GetSize().Z * 1.5f;

	Volume->BrushBuilder->Build(InWorld, Volume);

	FBSPOps::csgPrepMovingBrush(Volume);

	// Set the texture on all polys to NULL.  This stops invisible textures
	// dependencies from being formed on volumes.
	if (Volume->Brush)
	{
		for (int32 poly = 0; poly < Volume->Brush->Polys->Element.Num(); ++poly)
		{
			FPoly* Poly = &(Volume->Brush->Polys->Element[poly]);
			Poly->Material = NULL;
		}
	}

	Volume->PostEditChange();

	return Volume;
}

void FHierarchicalLODUtilities::HandleActorModified(AActor* InActor)
{
	ALODActor* ParentActor = GetParentLODActor(InActor);

	if (ParentActor != nullptr )
	{
		// So something in the actor changed that require use to flag the cluster as dirty
		ParentActor->Modify();
		ParentActor->SetIsDirty(true);
	}
}

bool FHierarchicalLODUtilities::IsWorldUsedForStreaming(const UWorld* InWorld)
{
	// Find references to the given world's outer package
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetIdentifier> ReferenceNames;
	const UPackage* OuterPackage = InWorld->GetOutermost();
	AssetRegistryModule.Get().GetReferencers(FAssetIdentifier(OuterPackage->GetFName()), ReferenceNames);

	for (const FAssetIdentifier& Identifier : ReferenceNames)
	{
		// Referncers can include things like primary asset virtual packages, we don't want those
		if (Identifier.PackageName != NAME_None)
		{
			const FString PackageName = Identifier.PackageName.ToString();
			UPackage* ReferencingPackage = FindPackage(nullptr, *PackageName);
			if (!ReferencingPackage)
			{
				ReferencingPackage = LoadPackage(nullptr, *PackageName, LOAD_None);
			}

			// Retrieve the referencing UPackage and check if it contains a map asset
			if (ReferencingPackage && ReferencingPackage->ContainsMap())
			{
				TArray<UPackage*> Packages;
				Packages.Add(ReferencingPackage);
				TArray<UObject*> Objects;
				PackageTools::GetObjectsInPackages(&Packages, Objects);

				// Loop over all objects in package and try to find a world
				for (UObject* Object : Objects)
				{
					if (UWorld* World = Cast<UWorld>(Object))
					{
						// Check the world contains InWorld as a streaming level
						if (World->StreamingLevels.FindByPredicate([InWorld](const ULevelStreaming* StreamingLevel)
						{
							return StreamingLevel->GetWorldAsset() == InWorld;
						}))
						{
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
