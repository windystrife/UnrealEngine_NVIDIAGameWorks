// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshMergingTool/MeshMergingTool.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "Dialogs/Dialogs.h"
#include "MeshUtilities.h"
#include "MeshMergingTool/SMeshMergingDialog.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "ScopedTransaction.h"
#include "MeshMergeModule.h"


#define LOCTEXT_NAMESPACE "MeshMergingTool"

FMeshMergingTool::FMeshMergingTool()
	: bReplaceSourceActors(false)
{
	SettingsObject = UMeshMergingSettingsObject::Get();
}

TSharedRef<SWidget> FMeshMergingTool::GetWidget()
{
	SAssignNew(MergingDialog, SMeshMergingDialog, this);
	return MergingDialog.ToSharedRef();
}

FText FMeshMergingTool::GetTooltipText() const
{
	return LOCTEXT("MeshMergingToolTooltip", "Harvest geometry from selected actors and merge grouping them by materials.");
}

FString FMeshMergingTool::GetDefaultPackageName() const
{
	FString PackageName = FPackageName::FilenameToLongPackageName(FPaths::ProjectContentDir() + TEXT("SM_MERGED"));

	USelection* SelectedActors = GEditor->GetSelectedActors();
	// Iterate through selected actors and find first static mesh asset
	// Use this static mesh path as destination package name for a merged mesh
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			FString ActorName = Actor->GetName();
			PackageName = FString::Printf(TEXT("%s_%s"), *PackageName, *ActorName);
			break;
		}
	}

	if (PackageName.IsEmpty())
	{
		PackageName = MakeUniqueObjectName(NULL, UPackage::StaticClass(), *PackageName).ToString();
	}

	return PackageName;
}

bool FMeshMergingTool::RunMerge(const FString& PackageName)
{
	const IMeshMergeUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<AActor*> Actors;
	TArray<ULevel*> UniqueLevels;
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			Actors.Add(Actor);
			UniqueLevels.AddUnique(Actor->GetLevel());
		}
	}

	// This restriction is only for replacement of selected actors with merged mesh actor
	if (UniqueLevels.Num() > 1 && bReplaceSourceActors)
	{
		FText Message = NSLOCTEXT("UnrealEd", "FailedToMergeActorsSublevels_Msg", "The selected actors should be in the same level");
		OpenMsgDlgInt(EAppMsgType::Ok, Message, NSLOCTEXT("UnrealEd", "FailedToMergeActors_Title", "Unable to merge actors"));
		return false;
	}

	FVector MergedActorLocation;
	TArray<UObject*> AssetsToSync;
	// Merge...
	{
		FScopedSlowTask SlowTask(0, LOCTEXT("MergingActorsSlowTask", "Merging actors..."));
		SlowTask.MakeDialog();

		// Extracting static mesh components from the selected mesh components in the dialog
		const TArray<TSharedPtr<FMergeComponentData>>& SelectedComponents = MergingDialog->GetSelectedComponents();
		TArray<UPrimitiveComponent*> ComponentsToMerge;

		for ( const TSharedPtr<FMergeComponentData>& SelectedComponent : SelectedComponents)
		{
			// Determine whether or not this component should be incorporated according the user settings
			if (SelectedComponent->bShouldIncorporate)
			{
				ComponentsToMerge.Add(SelectedComponent->PrimComponent.Get());
			}
		}
		
		UWorld* World = ComponentsToMerge[0]->GetWorld();	
		checkf(World != nullptr, TEXT("Invalid World retrieved from Mesh components"));
		const float ScreenAreaSize = TNumericLimits<float>::Max();
		MeshUtilities.MergeComponentsToStaticMesh(ComponentsToMerge, World, SettingsObject->Settings, nullptr, PackageName, AssetsToSync, MergedActorLocation, ScreenAreaSize, true);
	}

	if (AssetsToSync.Num())
	{
		FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		int32 AssetCount = AssetsToSync.Num();
		for (int32 AssetIndex = 0; AssetIndex < AssetCount; AssetIndex++)
		{
			AssetRegistry.AssetCreated(AssetsToSync[AssetIndex]);
			GEditor->BroadcastObjectReimported(AssetsToSync[AssetIndex]);
		}

		//Also notify the content browser that the new assets exists
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync, true);

		// Place new mesh in the world
		if (bReplaceSourceActors)
		{
			UStaticMesh* MergedMesh = nullptr;
			if (AssetsToSync.FindItemByClass(&MergedMesh))
			{
				const FScopedTransaction Transaction(LOCTEXT("PlaceMergedActor", "Place Merged Actor"));
				UniqueLevels[0]->Modify();

				UWorld* World = UniqueLevels[0]->OwningWorld;
				FActorSpawnParameters Params;
				Params.OverrideLevel = UniqueLevels[0];
				FRotator MergedActorRotation(ForceInit);
								
				AStaticMeshActor* MergedActor = World->SpawnActor<AStaticMeshActor>(MergedActorLocation, MergedActorRotation, Params);
				MergedActor->GetStaticMeshComponent()->SetStaticMesh(MergedMesh);
				MergedActor->SetActorLabel(AssetsToSync[0]->GetName());
				World->UpdateCullDistanceVolumes(MergedActor, MergedActor->GetStaticMeshComponent());
				// Remove source actors
				for (AActor* Actor : Actors)
				{
					Actor->Destroy();
				}
			}
		}
	}

	MergingDialog->Reset();

	return true;
}

bool FMeshMergingTool::CanMerge() const
{	
	return MergingDialog->GetNumSelectedMeshComponents() >= 1;
}

#undef LOCTEXT_NAMESPACE
