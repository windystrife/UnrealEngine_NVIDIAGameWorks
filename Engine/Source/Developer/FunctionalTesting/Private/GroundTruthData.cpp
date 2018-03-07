// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GroundTruthData.h"

#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "AssetData.h"

#if WITH_EDITOR
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlOperation.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(GroundTruthLog, Log, Log)

UGroundTruthData::UGroundTruthData()
	: bResetGroundTruth(false)
{
}

bool UGroundTruthData::CanModify() const
{
	return ObjectData == nullptr;
}

UObject* UGroundTruthData::LoadObject()
{
	UE_LOG(GroundTruthLog, Log, TEXT("Loaded Ground Truth, '%s'."), *GetPathName());
	
	return ObjectData;
}

void UGroundTruthData::SaveObject(UObject* GroundTruth)
{
	FAssetData GroundTruthAssetData(this);

	UPackage* GroundTruthPackage = GetOutermost();
	FString GroundTruthPackageName = GroundTruthAssetData.PackageName.ToString();

#if WITH_EDITOR
	if (!CanModify())
	{
		UE_LOG(GroundTruthLog, Warning, TEXT("Ground Truth, '%s' already set, unable to save changes.  Open and use bResetGroundTruth to reset the ground truth."), *GroundTruthPackageName);
		return;
	}

	if (GroundTruth == nullptr)
	{
		UE_LOG(GroundTruthLog, Error, TEXT("Ground Truth, '%s' can not store a null object."), *GroundTruthPackageName);
		return;
	}

	if (GIsBuildMachine)
	{
		UE_LOG(GroundTruthLog, Error, TEXT("Ground Truth, '%s' can not be modified on the build machine."), *GroundTruthPackageName);
		return;
	}

	if (ISourceControlModule::Get().IsEnabled())
	{
		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
		SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), GroundTruthPackage);
		SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), GroundTruthPackage);
	}

	ObjectData = GroundTruth;
	GroundTruth->Rename(nullptr, this);
	MarkPackageDirty();

	if (!UPackage::SavePackage(GroundTruthPackage, NULL, RF_Standalone, *FPackageName::LongPackageNameToFilename(GroundTruthPackageName, FPackageName::GetAssetPackageExtension()), GError, nullptr, false, true, SAVE_NoError))
	{
		UE_LOG(GroundTruthLog, Error, TEXT("Failed to save ground truth data! %s"), *GroundTruthPackageName);
	}

	UE_LOG(GroundTruthLog, Log, TEXT("Saved Ground Truth, '%s'."), *GroundTruthPackageName);

#else
	UE_LOG(GroundTruthLog, Error, TEXT("Can't save ground truth data outside of the editor, '%s'."), *GroundTruthPackageName);
#endif
}

#if WITH_EDITOR

void UGroundTruthData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UGroundTruthData, bResetGroundTruth))
	{
		bResetGroundTruth = false;

		if (ObjectData)
		{
			ObjectData->Rename(nullptr, GetTransientPackage());
			ObjectData = nullptr;
		}

		MarkPackageDirty();
	}
}

#endif