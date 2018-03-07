// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EditorTestsUtilityLibrary.h"
#include "CoreMinimal.h"
#include "MaterialOptions.h"
#include "IMeshMergeUtilities.h"
#include "MeshMergeModule.h"

#include "StaticMeshComponentAdapter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/MeshMerging.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"
#include "AutomationStaticMeshComponentAdapter.h"
#include "Containers/Algo/Transform.h"
#include "Materials/Material.h"

void UEditorTestsUtilityLibrary::BakeMaterialsForComponent(UStaticMeshComponent* InStaticMeshComponent, const UMaterialOptions* MaterialOptions, const UMaterialMergeOptions* MaterialMergeOptions)
{
	if (InStaticMeshComponent != nullptr && InStaticMeshComponent->GetStaticMesh() != nullptr)
	{
		FModuleManager::Get().LoadModule("MaterialBaking");
		// Retrieve settings object
		UAssetBakeOptions* AssetOptions = GetMutableDefault<UAssetBakeOptions>();
		TArray<TWeakObjectPtr<UObject>> Objects{ MaterialMergeOptions, AssetOptions, MaterialOptions };

		FAutomationStaticMeshComponentAdapter Adapter(InStaticMeshComponent);
		const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
		MeshMergeUtilities.BakeMaterialsForComponent(Objects, &Adapter);

		InStaticMeshComponent->MarkRenderStateDirty();
		InStaticMeshComponent->MarkRenderTransformDirty();
		InStaticMeshComponent->MarkRenderDynamicDataDirty();

		const int32 NumMaterials = InStaticMeshComponent->GetNumMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
		{
			UMaterialInterface* Material = InStaticMeshComponent->GetMaterial(MaterialIndex);
			TArray<UTexture*> MaterialTextures;
			Material->GetUsedTextures(MaterialTextures, EMaterialQualityLevel::Num, true, GMaxRHIFeatureLevel, true);

			// Force load materials used by the current material
			for (UTexture* Texture : MaterialTextures)
			{
				if (Texture != NULL)
				{
					UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
					if (Texture2D)
					{
						Texture2D->SetForceMipLevelsToBeResident(30.0f);
						Texture2D->WaitForStreaming();
					}
				}
			}
		}
	}
}

void UEditorTestsUtilityLibrary::MergeStaticMeshComponents(TArray<UStaticMeshComponent*> InStaticMeshComponents, const FMeshMergingSettings& MergeSettings, const bool bReplaceActors, TArray<int32>& OutLODIndices)
{
	FModuleManager::Get().LoadModule("MaterialBaking");

	InStaticMeshComponents.RemoveAll([](UStaticMeshComponent* Component) { return Component == nullptr || Component->GetStaticMesh() == nullptr; });
	if (InStaticMeshComponents.Num() && InStaticMeshComponents[0] != nullptr)
	{
		const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
		UWorld* World = InStaticMeshComponents[0]->GetWorld();

		// Convert array of StaticMeshComponents to PrimitiveComponents
		TArray<UPrimitiveComponent*> PrimCompsToMerge;
		Algo::Transform(InStaticMeshComponents, PrimCompsToMerge, [](UStaticMeshComponent* StaticMeshComp) { return StaticMeshComp; });

		TArray<UObject*> Output;
		FVector OutPosition;
		MeshMergeUtilities.MergeComponentsToStaticMesh(PrimCompsToMerge, World, MergeSettings, GetTransientPackage(), InStaticMeshComponents[0]->GetStaticMesh()->GetName(), Output, OutPosition, 1.0f, false);
		
		UObject** MaterialPtr = Output.FindByPredicate([](UObject* Object) { return Object->IsA<UMaterial>(); });
		if (MaterialPtr)
		{
			UMaterial* MergedMaterial = Cast<UMaterial>(*MaterialPtr);
			TArray<UTexture*> MaterialTextures;
			MergedMaterial->GetUsedTextures(MaterialTextures, EMaterialQualityLevel::Num, true, GMaxRHIFeatureLevel, true);

			// Force load materials used by the current material
			for (UTexture* Texture : MaterialTextures)
			{
				if (Texture != NULL)
				{
					UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
					if (Texture2D)
					{
						Texture2D->SetForceMipLevelsToBeResident(30.0f);
						Texture2D->WaitForStreaming();
					}
				}
			}
		}

		// Place new mesh in the world
		if (bReplaceActors)
		{
			UObject** ObjectPtr = Output.FindByPredicate([](const UObject* Object) {return Object->IsA<UStaticMesh>(); });
			if (ObjectPtr != nullptr && *ObjectPtr != nullptr)
			{
				UStaticMesh* MergedMesh = CastChecked<UStaticMesh>(*ObjectPtr);

				if (MergedMesh)
				{
					for (int32 Index = 0; Index < MergedMesh->GetNumLODs(); ++Index)
					{
						OutLODIndices.Add(Index);
					}

					FActorSpawnParameters Params;
					Params.OverrideLevel = World->PersistentLevel;
					FRotator MergedActorRotation(ForceInit);

					AStaticMeshActor* MergedActor = World->SpawnActor<AStaticMeshActor>(OutPosition, MergedActorRotation, Params);
					MergedActor->SetMobility(EComponentMobility::Movable);
					MergedActor->SetActorLabel(Output[0]->GetName());
					MergedActor->GetStaticMeshComponent()->SetStaticMesh(MergedMesh);

					TArray<AActor*> OwningActors;
					for (UStaticMeshComponent* Component : InStaticMeshComponents)
					{
						OwningActors.AddUnique(Component->GetOwner());
					}

					// Remove source actors
					for (AActor* Actor : OwningActors)
					{
						Actor->Destroy();
					}
				}
			}
		}
	}
}

