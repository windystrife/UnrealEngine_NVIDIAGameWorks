// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MergeUtils.h"
#include "IAssetTypeActions.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "MergeToolUtils"

//------------------------------------------------------------------------------
FSourceControlStatePtr FMergeToolUtils::GetSourceControlState(const FString& PackageName)
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateStatusOperation->SetUpdateHistory(true);
	SourceControlProvider.Execute(UpdateStatusOperation, SourceControlHelpers::PackageFilename(PackageName));

	FSourceControlStatePtr State = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(PackageName), EStateCacheUsage::Use);
	if (!State.IsValid() || !State->IsSourceControlled() || !FPackageName::DoesPackageExist(PackageName))
	{
		return FSourceControlStatePtr();
	}
	else
	{
		return State;
	}
}

//------------------------------------------------------------------------------
UObject const* FMergeToolUtils::LoadRevision(const FString& AssetName, const ISourceControlRevision& DesiredRevision)
{
	FSlateNotificationManager& NotificationManager = FSlateNotificationManager::Get();

	// Get the head revision of this package from source control
	FString TempFileName;
	if (DesiredRevision.Get(TempFileName))
	{
		// Try and load that package
		UPackage* TempPackage = LoadPackage(NULL, *TempFileName, LOAD_DisableCompileOnLoad);
		if (TempPackage != NULL)
		{
			// Grab the old asset from that old package
			UObject* OldObject = FindObject<UObject>(TempPackage, *AssetName);
			if (OldObject)
			{
				return OldObject;
			}
			else
			{
				NotificationManager.AddNotification(
					FText::Format(
						LOCTEXT("MergedFailedToFindObject", "Aborted Load of {0} because we could not find an object named {1}" )
						, FText::FromString(TempFileName)
						, FText::FromString(AssetName) 
					)
				);
			}
		}
		else
		{
			NotificationManager.AddNotification(
				FText::Format(
					LOCTEXT("MergedFailedToLoadPackage", "Aborted Load of {0} because we could not load the package")
					, FText::FromString(TempFileName)
				)
			);
		}
	}
	else
	{
		NotificationManager.AddNotification(
			FText::Format(
				LOCTEXT("MergedFailedToFindRevision", "Aborted Load of {0} because we could not get the requested revision")
				, FText::FromString(TempFileName)
			)
		);
	}
	return NULL;
}

//------------------------------------------------------------------------------
UObject const* FMergeToolUtils::LoadRevision(const FString& PackageName, const FRevisionInfo& DesiredRevision)
{
	const UObject* AssetRevision = nullptr;
	if (UPackage* AssetPackage = LoadPackage(/*Outer =*/nullptr, *PackageName, LOAD_None))
	{
		TArray<UObject*> PackageObjs;
		GetObjectsWithOuter(AssetPackage, PackageObjs, /*bIncludeNestedObjects =*/false);

		for (UObject* PackageObj : PackageObjs)
		{
			if (PackageObj->IsAsset())
			{
				AssetRevision = LoadRevision(PackageObj, DesiredRevision);
				break;
			}
		}
	}
	return AssetRevision;
}

//------------------------------------------------------------------------------
UObject const* FMergeToolUtils::LoadRevision(const UObject* AssetObject, const FRevisionInfo& DesiredRevision)
{
	check(AssetObject->IsAsset());
	
	const UObject* AssetRevision = nullptr;
	if (DesiredRevision.Revision.IsEmpty())
	{
		// an invalid revision number represents the local copy
		AssetRevision = AssetObject;
	}
	else
	{
		FString const PackageName = AssetObject->GetOutermost()->GetName();

		FSourceControlStatePtr SourceControlState = GetSourceControlState(PackageName);
		if (SourceControlState.IsValid())
		{
			TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision = SourceControlState->FindHistoryRevision(DesiredRevision.Revision);
			if (Revision.IsValid())
			{
				FString const AssetName = AssetObject->GetName();
				AssetRevision = LoadRevision(AssetName, *Revision);
			}
		}
	}

	return AssetRevision;
}

#undef LOCTEXT_NAMESPACE
