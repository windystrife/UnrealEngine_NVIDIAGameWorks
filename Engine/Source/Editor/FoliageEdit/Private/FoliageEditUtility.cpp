// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FoliageEditUtility.h"
#include "FoliageType.h"
#include "InstancedFoliage.h"
#include "LevelUtils.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "InstancedFoliageActor.h"
#include "ScopedTransaction.h"
#include "DlgPickAssetPath.h"
#include "AssetRegistryModule.h"
#include "FoliageEdMode.h"
#include "FileHelpers.h"

#define LOCTEXT_NAMESPACE "FoliageEdMode"

UFoliageType* FFoliageEditUtility::SaveFoliageTypeObject(UFoliageType* InFoliageType)
{
	UFoliageType* TypeToSave = nullptr;

	if (!InFoliageType->IsAsset())
	{
		FString PackageName;
		UStaticMesh* StaticMesh = InFoliageType->GetStaticMesh();
		if (StaticMesh)
		{
			// Build default settings asset name and path
			PackageName = FPackageName::GetLongPackagePath(StaticMesh->GetOutermost()->GetName()) + TEXT("/") + StaticMesh->GetName() + TEXT("_FoliageType");
		}

		TSharedRef<SDlgPickAssetPath> SaveFoliageTypeDialog =
			SNew(SDlgPickAssetPath)
			.Title(LOCTEXT("SaveFoliageTypeDialogTitle", "Choose Location for Foliage Type Asset"))
			.DefaultAssetPath(FText::FromString(PackageName));

		if (SaveFoliageTypeDialog->ShowModal() != EAppReturnType::Cancel)
		{
			PackageName = SaveFoliageTypeDialog->GetFullAssetPath().ToString();
			UPackage* Package = CreatePackage(nullptr, *PackageName);

			// We should not save a copy of this duplicate into the transaction buffer as it's an asset
			InFoliageType->ClearFlags(RF_Transactional);
			TypeToSave = Cast<UFoliageType>(StaticDuplicateObject(InFoliageType, Package, *FPackageName::GetLongPackageAssetName(PackageName)));
			InFoliageType->SetFlags(RF_Transactional);

			TypeToSave->SetFlags(RF_Standalone | RF_Public | RF_Transactional);
			TypeToSave->Modify();

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(TypeToSave);
		}
	}
	else
	{
		TypeToSave = InFoliageType;
	}

	// Save to disk
	if (TypeToSave)
	{
		TArray<UPackage*> PackagesToSave;
		PackagesToSave.Add(TypeToSave->GetOutermost());
		const bool bCheckDirty = false;
		const bool bPromptToSave = false;
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave);
	}

	return TypeToSave;
}

void FFoliageEditUtility::ReplaceFoliageTypeObject(UWorld* InWorld, UFoliageType* OldType, UFoliageType* NewType)
{
	FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "FoliageMode_ReplaceSettingsObject", "Foliage Editing: Replace Settings Object"));

	// Collect set of all available foliage types
	ULevel* CurrentLevel = InWorld->GetCurrentLevel();
	const int32 NumLevels = InWorld->GetNumLevels();

	for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
	{
		ULevel* Level = InWorld->GetLevel(LevelIdx);
		if (Level && Level->bIsVisible)
		{
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level);
			if (IFA)
			{
				IFA->Modify();
				TUniqueObj<FFoliageMeshInfo> OldMeshInfo;
				IFA->FoliageMeshes.RemoveAndCopyValue(OldType, OldMeshInfo);

				// Old component needs to go
				if (OldMeshInfo->Component != nullptr)
				{
					OldMeshInfo->Component->ClearInstances();
					OldMeshInfo->Component->SetFlags(RF_Transactional);
					OldMeshInfo->Component->Modify();
					OldMeshInfo->Component->DestroyComponent();
					OldMeshInfo->Component = nullptr;
				}

				// Append instances if new foliage type is already exists in this actor
				// Otherwise just replace key entry for instances
				TUniqueObj<FFoliageMeshInfo>* NewMeshInfo = IFA->FoliageMeshes.Find(NewType);
				if (NewMeshInfo)
				{
					(*NewMeshInfo)->Instances.Append(OldMeshInfo->Instances);
					(*NewMeshInfo)->ReallocateClusters(IFA, NewType);
				}
				else
				{
					IFA->FoliageMeshes.Add(NewType, MoveTemp(OldMeshInfo))->ReallocateClusters(IFA, NewType);
				}
			}
		}
	}
}


void FFoliageEditUtility::MoveActorFoliageInstancesToLevel(ULevel* InTargetLevel)
{
	// Can't move into a locked level
	if (FLevelUtils::IsLevelLocked(InTargetLevel))
	{
		FNotificationInfo NotificatioInfo(NSLOCTEXT("UnrealEd", "CannotMoveFoliageIntoLockedLevel", "Cannot move the selected foliage into a locked level"));
		NotificatioInfo.bUseThrobber = false;
		FSlateNotificationManager::Get().AddNotification(NotificatioInfo)->SetCompletionState(SNotificationItem::CS_Fail);
		return;
	}

	// Get a world context
	UWorld* World = InTargetLevel->OwningWorld;
	bool PromptToMoveFoliageTypeToAsset = World->StreamingLevels.Num() > 0;
	bool ShouldPopulateMeshList = false;

	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "MoveSelectedFoliageToSelectedLevel", "Move Selected Foliage to Level"), !GEditor->IsTransactionActive());

	// Iterate over all foliage actors in the world and move selected instances to a foliage actor in the target level
	const int32 NumLevels = World->GetNumLevels();
	for (int32 LevelIdx = 0; LevelIdx < NumLevels; ++LevelIdx)
	{
		ULevel* Level = World->GetLevel(LevelIdx);
		if (Level != InTargetLevel)
		{
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level, /*bCreateIfNone*/ false);

			bool CanMoveInstanceType = true;

			TMap<UFoliageType*, FFoliageMeshInfo*> InstancesFoliageType = IFA->GetAllInstancesFoliageType();

			for (auto& MeshPair : InstancesFoliageType)
			{
				if (MeshPair.Key != nullptr && MeshPair.Value != nullptr && !MeshPair.Key->IsAsset())
				{
					// Keep previous selection
					TSet<int32> PreviousSelectionSet = MeshPair.Value->SelectedIndices;
					TArray<int32> PreviousSelectionArray;
					PreviousSelectionArray.Reserve(PreviousSelectionSet.Num());

					for (int32& Value : PreviousSelectionSet)
					{
						PreviousSelectionArray.Add(Value);
					}

					UFoliageType* NewFoliageType = SaveFoliageTypeObject(MeshPair.Key);

					if (NewFoliageType != nullptr && NewFoliageType != MeshPair.Key)
					{
						ReplaceFoliageTypeObject(World, MeshPair.Key, NewFoliageType);
					}

					CanMoveInstanceType = NewFoliageType != nullptr;

					if (NewFoliageType != nullptr)
					{
						// Restore previous selection for move operation
						FFoliageMeshInfo* MeshInfo = IFA->FindMesh(NewFoliageType);
						MeshInfo->SelectInstances(IFA, true, PreviousSelectionArray);
					}
				}
			}

			// Update our actor if we saved some foliage type as asset
			if (CanMoveInstanceType)
			{
				IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(Level, /*bCreateIfNone*/ false);
				ensure(IFA != nullptr);

				IFA->MoveAllInstancesToLevel(InTargetLevel);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
