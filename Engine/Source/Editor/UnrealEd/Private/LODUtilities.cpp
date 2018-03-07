// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LODUtilities.h"
#include "Misc/MessageDialog.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/MorphTarget.h"


#include "MeshUtilities.h"

#if WITH_APEX_CLOTHING
	#include "ApexClothingUtils.h"
#endif // #if WITH_APEX_CLOTHING

#include "ComponentReregisterContext.h"
#include "IMeshReductionManagerModule.h"

void FLODUtilities::RemoveLOD(FSkeletalMeshUpdateContext& UpdateContext, int32 DesiredLOD )
{
	USkeletalMesh* SkeletalMesh = UpdateContext.SkeletalMesh;
	FSkeletalMeshResource* SkelMeshResource = SkeletalMesh->GetImportedResource();

	if( SkelMeshResource->LODModels.Num() == 1 )
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "NoLODToRemove", "No LODs to remove!") );
		return;
	}

	// Now display combo to choose which LOD to remove.
	TArray<FString> LODStrings;
	LODStrings.AddZeroed( SkelMeshResource->LODModels.Num()-1 );
	for(int32 i=0; i<SkelMeshResource->LODModels.Num()-1; i++)
	{
		LODStrings[i] = FString::Printf( TEXT("%d"), i+1 );
	}

	check( SkeletalMesh->LODInfo.Num() == SkelMeshResource->LODModels.Num() );

	// If its a valid LOD, kill it.
	if( DesiredLOD > 0 && DesiredLOD < SkelMeshResource->LODModels.Num() )
	{
		//We'll be modifying the skel mesh data so reregister

		//TODO - do we need to reregister something else instead?
		FMultiComponentReregisterContext ReregisterContext(UpdateContext.AssociatedComponents);

		// Release rendering resources before deleting LOD
		SkelMeshResource->ReleaseResources();

		// Block until this is done
		FlushRenderingCommands();

		SkelMeshResource->LODModels.RemoveAt(DesiredLOD);
		SkeletalMesh->LODInfo.RemoveAt(DesiredLOD);
		SkeletalMesh->InitResources();

		RefreshLODChange(SkeletalMesh);

		// Set the forced LOD to Auto.
		for(auto Iter = UpdateContext.AssociatedComponents.CreateIterator(); Iter; ++Iter)
		{
			USkinnedMeshComponent* SkinnedComponent = Cast<USkinnedMeshComponent>(*Iter);
			if(SkinnedComponent)
			{
				SkinnedComponent->ForcedLodModel = 0;
			}
		}

		//remove all Morph target data for this LOD
		for (UMorphTarget* MorphTarget : SkeletalMesh->MorphTargets)
		{
			if (MorphTarget->HasDataForLOD(DesiredLOD))
			{
				MorphTarget->MorphLODModels.RemoveAt(DesiredLOD);
			}
		}
		
		//Notify calling system of change
		UpdateContext.OnLODChanged.ExecuteIfBound();

		// Mark things for saving.
		SkeletalMesh->MarkPackageDirty();
	}
}

void FLODUtilities::SimplifySkeletalMeshLOD( USkeletalMesh* SkeletalMesh, const FSkeletalMeshOptimizationSettings& InSetting, int32 DesiredLOD, bool bReregisterComponent /*= true*/ )
{
	IMeshReductionModule& ReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionModule>("MeshReductionInterface");
	IMeshReduction* MeshReduction = ReductionModule.GetSkeletalMeshReductionInterface();

	check (MeshReduction && MeshReduction->IsSupported());
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("DesiredLOD"), DesiredLOD);
		Args.Add(TEXT("SkeletalMeshName"), FText::FromString(SkeletalMesh->GetName()));
		const FText StatusUpdate = FText::Format(NSLOCTEXT("UnrealEd", "MeshSimp_GeneratingLOD_F", "Generating LOD{DesiredLOD} for {SkeletalMeshName}..."), Args);
		GWarn->BeginSlowTask(StatusUpdate, true);
	}

	bool bRecalcLOD = ( !SkeletalMesh->LODInfo.IsValidIndex(DesiredLOD) );
	if (MeshReduction->ReduceSkeletalMesh(SkeletalMesh, DesiredLOD, InSetting, bRecalcLOD, bReregisterComponent))
	{
		check(SkeletalMesh->LODInfo.Num() >= 2);
		SkeletalMesh->MarkPackageDirty();
	}
	else
	{
		// Simplification failed! Warn the user.
		FFormatNamedArguments Args;
		Args.Add(TEXT("SkeletalMeshName"), FText::FromString(SkeletalMesh->GetName()));
		const FText Message = FText::Format(NSLOCTEXT("UnrealEd", "MeshSimp_GenerateLODFailed_F", "An error occurred while simplifying the geometry for mesh '{SkeletalMeshName}'.  Consider adjusting simplification parameters and re-simplifying the mesh."), Args);
		FMessageDialog::Open(EAppMsgType::Ok, Message);
	}
	GWarn->EndSlowTask();
}

void FLODUtilities::SimplifySkeletalMesh( FSkeletalMeshUpdateContext& UpdateContext, TArray<FSkeletalMeshOptimizationSettings> &InSettings, bool bForceRegenerate )
{
	USkeletalMesh* SkeletalMesh = UpdateContext.SkeletalMesh;
	IMeshReductionModule& ReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionModule>("MeshReductionInterface");
	IMeshReduction* MeshReduction = ReductionModule.GetSkeletalMeshReductionInterface();

	if ( MeshReduction && MeshReduction->IsSupported() && SkeletalMesh )
	{
		// Simplify each LOD
		for (int32 SettingIndex = 0; SettingIndex < InSettings.Num(); ++SettingIndex)
		{
			uint32 DesiredLOD = SettingIndex + 1;

			// check whether reduction settings are same or not
			if (!bForceRegenerate && SkeletalMesh->LODInfo.IsValidIndex(DesiredLOD) 
			 && SkeletalMesh->LODInfo[DesiredLOD].ReductionSettings == InSettings[SettingIndex])
			{
				continue;
			}

			SimplifySkeletalMeshLOD( SkeletalMesh, InSettings[SettingIndex], DesiredLOD );
		}

		//Notify calling system of change
		UpdateContext.OnLODChanged.ExecuteIfBound();
	}
}

void FLODUtilities::SimplifySkeletalMeshLOD(FSkeletalMeshUpdateContext& UpdateContext, const FSkeletalMeshOptimizationSettings& Setting, int32 DesiredLOD, bool bReregisterComponent /*= true*/)
{
	USkeletalMesh* SkeletalMesh = UpdateContext.SkeletalMesh;
	IMeshReductionModule& ReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionModule>("MeshReductionInterface");
	IMeshReduction* MeshReduction = ReductionModule.GetSkeletalMeshReductionInterface();

	if (MeshReduction && MeshReduction->IsSupported() && SkeletalMesh)
	{
		SimplifySkeletalMeshLOD(SkeletalMesh, Setting, DesiredLOD, bReregisterComponent);

		//Notify calling system of change
		UpdateContext.OnLODChanged.ExecuteIfBound();
	}
}
void FLODUtilities::RefreshLODChange(const USkeletalMesh* SkeletalMesh)
{
	for (FObjectIterator Iter(USkeletalMeshComponent::StaticClass()); Iter; ++Iter)
	{
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(*Iter);
		if  (SkeletalMeshComponent->SkeletalMesh == SkeletalMesh)
		{
			// it needs to recreate IF it already has been created
			if (SkeletalMeshComponent->IsRegistered())
			{
				SkeletalMeshComponent->UpdateLODStatus();
				SkeletalMeshComponent->MarkRenderStateDirty();
			}
		}
	}
}
