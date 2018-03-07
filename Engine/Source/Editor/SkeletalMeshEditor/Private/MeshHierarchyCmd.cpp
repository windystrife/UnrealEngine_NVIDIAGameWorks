// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshHierarchyCmd.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "AssetData.h"
#include "EdGraph/EdGraphSchema.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"

static FMeshHierarchyCmd MeshHierarchyCmdExec;

bool FMeshHierarchyCmd::Exec(UWorld*, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bResult = false;
	if (FParse::Command(&Cmd, TEXT("TMH")))
	{
		Ar.Log(TEXT("Starting Mesh Test"));
		FARFilter Filter;
		Filter.ClassNames.Add(USkeletalMesh::StaticClass()->GetFName());

		TArray<FAssetData> SkeletalMeshes;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().GetAssets(Filter, SkeletalMeshes);

		const FText StatusUpdate = NSLOCTEXT("MeshHierarchyCmd", "RemoveUnusedBones_ProcessingAssets", "Processing Skeletal Meshes");
		GWarn->BeginSlowTask(StatusUpdate, true);

		// go through all assets try load
		for (int32 MeshIdx = 0; MeshIdx < SkeletalMeshes.Num(); ++MeshIdx)
		{
			GWarn->StatusUpdate(MeshIdx, SkeletalMeshes.Num(), StatusUpdate);

			USkeletalMesh* Mesh = Cast<USkeletalMesh>(SkeletalMeshes[MeshIdx].GetAsset());
			if (Mesh->Skeleton)
			{
				FName MeshRoot = Mesh->RefSkeleton.GetBoneName(0);
				FName SkelRoot = Mesh->Skeleton->GetReferenceSkeleton().GetBoneName(0);

				if (MeshRoot != SkelRoot)
				{
					Ar.Logf(TEXT("Mesh Found '%s' %s->%s"), *Mesh->GetName(), *MeshRoot.ToString(), *SkelRoot.ToString());
				}
			}
		}

		GWarn->EndSlowTask();
		Ar.Log(TEXT("Mesh Test Finished"));
	}
	return bResult;
}
