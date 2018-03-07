// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "AssetFixUpRedirectors.h"
#include "UObject/ObjectRedirector.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/MetaData.h"
#include "Misc/PackageName.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlModule.h"
#include "FileHelpers.h"
#include "SDiscoveringAssetsDialog.h"
#include "AssetRenameManager.h"
#include "AssetRegistryModule.h"
#include "ICollectionManager.h"
#include "CollectionManagerModule.h"
#include "ObjectTools.h"
#include "Logging/MessageLog.h"
#include "AssetTools.h"

#define LOCTEXT_NAMESPACE "AssetFixUpRedirectors"


struct FRedirectorRefs
{
	UObjectRedirector* Redirector;
	FName RedirectorPackageName;
	TArray<FName> ReferencingPackageNames;
	FText FailureReason;
	bool bRedirectorValidForFixup;

	FRedirectorRefs(UObjectRedirector* InRedirector)
		: Redirector(InRedirector)
		, RedirectorPackageName(InRedirector->GetOutermost()->GetFName())
		, bRedirectorValidForFixup(true)
	{}
};


void FAssetFixUpRedirectors::FixupReferencers(const TArray<UObjectRedirector*>& Objects) const
{
	// Transform array into TWeakObjectPtr array
	TArray<TWeakObjectPtr<UObjectRedirector>> ObjectWeakPtrs;
	for (auto Object : Objects)
	{
		ObjectWeakPtrs.Add(Object);
	}

	if (ObjectWeakPtrs.Num() > 0)
	{
		// If the asset registry is still loading assets, we cant check for referencers, so we must open the Discovering Assets dialog until it is done
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule.Get().IsLoadingAssets())
		{
			// Open a dialog asking the user to wait while assets are being discovered
			SDiscoveringAssetsDialog::OpenDiscoveringAssetsDialog(
				SDiscoveringAssetsDialog::FOnAssetsDiscovered::CreateSP(this, &FAssetFixUpRedirectors::ExecuteFixUp, ObjectWeakPtrs)
				);
		}
		else
		{
			// No need to wait, attempt to fix references now.
			ExecuteFixUp(ObjectWeakPtrs);
		}
	}
}

void FAssetFixUpRedirectors::ExecuteFixUp(TArray<TWeakObjectPtr<UObjectRedirector>> Objects) const
{
	TArray<FRedirectorRefs> RedirectorRefsList;
	for (auto Object : Objects)
	{
		auto ObjectRedirector = Object.Get();

		if (ObjectRedirector)
		{
			RedirectorRefsList.Emplace(ObjectRedirector);
		}
	}

	if ( RedirectorRefsList.Num() > 0 )
	{
		// Gather all referencing packages for all redirectors that are being fixed.
		PopulateRedirectorReferencers(RedirectorRefsList);

		// Update Package Status for all selected redirectors if SCC is enabled
		if ( UpdatePackageStatus(RedirectorRefsList) )
		{
			// Load all referencing packages.
			TArray<UPackage*> ReferencingPackagesToSave;
			LoadReferencingPackages(RedirectorRefsList, ReferencingPackagesToSave);

			// Prompt to check out all referencing packages, leave redirectors for assets referenced by packages that are not checked out and remove those packages from the save list.
			const bool bUserAcceptedCheckout = CheckOutReferencingPackages(RedirectorRefsList, ReferencingPackagesToSave);
			if ( bUserAcceptedCheckout )
			{
				// If any referencing packages are left read-only, the checkout failed or SCC was not enabled. Trim them from the save list and leave redirectors.
				DetectReadOnlyPackages(RedirectorRefsList, ReferencingPackagesToSave);

				// Fix up referencing FSoftObjectPaths
				FixUpSoftObjectPaths(RedirectorRefsList, ReferencingPackagesToSave);

				// Save all packages that were referencing any of the assets that were moved without redirectors
				TArray<UPackage*> FailedToSave;
				SaveReferencingPackages(ReferencingPackagesToSave, FailedToSave);

				// Save any collections that were referencing any of the redirectors
				SaveReferencingCollections(RedirectorRefsList);

				// Wait for package referencers to be updated
				UpdateAssetReferencers(RedirectorRefsList);

				// Delete any redirectors that are no longer referenced
				DeleteRedirectors(RedirectorRefsList, FailedToSave);

				// Finally, report any failures that happened during the rename
				ReportFailures(RedirectorRefsList);
			}
		}
	}
}

void FAssetFixUpRedirectors::PopulateRedirectorReferencers(TArray<FRedirectorRefs>& RedirectorsToPopulate) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	for ( auto RedirectorRefsIt = RedirectorsToPopulate.CreateIterator(); RedirectorRefsIt; ++RedirectorRefsIt )
	{
		FRedirectorRefs& RedirectorRefs = *RedirectorRefsIt;
		AssetRegistryModule.Get().GetReferencers(RedirectorRefs.RedirectorPackageName, RedirectorRefs.ReferencingPackageNames);
	}
}

bool FAssetFixUpRedirectors::UpdatePackageStatus(const TArray<FRedirectorRefs>& RedirectorsToFix) const
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if ( ISourceControlModule::Get().IsEnabled() )
	{
		// Update the source control server availability to make sure we can do the rename operation
		SourceControlProvider.Login();
		if ( !SourceControlProvider.IsAvailable() )
		{
			// We have failed to update source control even though it is enabled. This is critical and we can not continue
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "SourceControl_ServerUnresponsive", "Source Control is unresponsive. Please check your connection and try again.") );
			return false;
		}

		TArray<UPackage*> PackagesToAddToSCCUpdate;

		for ( auto RedirectorRefsIt = RedirectorsToFix.CreateConstIterator(); RedirectorRefsIt; ++RedirectorRefsIt )
		{
			const FRedirectorRefs& RedirectorRefs = *RedirectorRefsIt;
			PackagesToAddToSCCUpdate.Add(RedirectorRefs.Redirector->GetOutermost());
		}

		SourceControlProvider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), PackagesToAddToSCCUpdate);
	}

	return true;
}

void FAssetFixUpRedirectors::LoadReferencingPackages(TArray<FRedirectorRefs>& RedirectorsToFix, TArray<UPackage*>& OutReferencingPackagesToSave) const
{
	FScopedSlowTask SlowTask( RedirectorsToFix.Num(), LOCTEXT( "LoadingReferencingPackages", "Loading Referencing Packages..." ) );
	SlowTask.MakeDialog();

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	// Load all packages that reference each redirector, if possible
	for ( auto RedirectorRefsIt = RedirectorsToFix.CreateIterator(); RedirectorRefsIt; ++RedirectorRefsIt )
	{
		SlowTask.EnterProgressFrame(1);

		FRedirectorRefs& RedirectorRefs = *RedirectorRefsIt;
		if ( ISourceControlModule::Get().IsEnabled() )
		{
			FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(RedirectorRefs.Redirector->GetOutermost(), EStateCacheUsage::Use);
			const bool bValidSCCState = !SourceControlState.IsValid() || SourceControlState->IsAdded() || SourceControlState->IsCheckedOut() || SourceControlState->CanCheckout() || !SourceControlState->IsSourceControlled() || SourceControlState->IsIgnored();

			if ( !bValidSCCState )
			{
				RedirectorRefs.bRedirectorValidForFixup = false;
				RedirectorRefs.FailureReason = LOCTEXT("RedirectorFixupFailed_BadSCC", "Redirector could not be checked out or marked for delete");
			}
		}

		// Load all referencers
		for ( auto PackageNameIt = RedirectorRefs.ReferencingPackageNames.CreateConstIterator(); PackageNameIt; ++PackageNameIt )
		{
			const FString PackageName = (*PackageNameIt).ToString();

			// Find the package in memory. If it is not in memory, try to load it
			UPackage* Package = FindPackage(NULL, *PackageName);
			if ( !Package )
			{
				Package = LoadPackage(NULL, *PackageName, LOAD_None);
			}

			if ( Package )
			{
				if ( Package->HasAnyPackageFlags(PKG_CompiledIn) )
				{
					// This is a script reference
					RedirectorRefs.bRedirectorValidForFixup = false;
					RedirectorRefs.FailureReason = FText::Format(LOCTEXT("RedirectorFixupFailed_CodeReference", "Redirector is referenced by code. Package: {0}"), FText::FromString(PackageName));
				}
				else
				{
					// If we found a valid package, mark it for save
					OutReferencingPackagesToSave.AddUnique(Package);
				}
			}
		}
	}
}

bool FAssetFixUpRedirectors::CheckOutReferencingPackages(TArray<FRedirectorRefs>& RedirectorsToFix, TArray<UPackage*>& InOutReferencingPackagesToSave) const
{
	// Prompt to check out all successfully loaded packages
	bool bUserAcceptedCheckout = true;

	if ( InOutReferencingPackagesToSave.Num() > 0 )
	{
		if ( ISourceControlModule::Get().IsEnabled() )
		{
			TArray<UPackage*> PackagesCheckedOutOrMadeWritable;
			TArray<UPackage*> PackagesNotNeedingCheckout;
			bUserAcceptedCheckout = FEditorFileUtils::PromptToCheckoutPackages( false, InOutReferencingPackagesToSave, &PackagesCheckedOutOrMadeWritable, &PackagesNotNeedingCheckout );
			if ( bUserAcceptedCheckout )
			{
				TArray<UPackage*> PackagesThatCouldNotBeCheckedOut = InOutReferencingPackagesToSave;

				for ( auto PackageIt = PackagesCheckedOutOrMadeWritable.CreateConstIterator(); PackageIt; ++PackageIt )
				{
					PackagesThatCouldNotBeCheckedOut.Remove(*PackageIt);
				}

				for ( auto PackageIt = PackagesNotNeedingCheckout.CreateConstIterator(); PackageIt; ++PackageIt )
				{
					PackagesThatCouldNotBeCheckedOut.Remove(*PackageIt);
				}

				for ( auto PackageIt = PackagesThatCouldNotBeCheckedOut.CreateConstIterator(); PackageIt; ++PackageIt )
				{
					const FName NonCheckedOutPackageName = (*PackageIt)->GetFName();

					for ( auto RedirectorRefsIt = RedirectorsToFix.CreateIterator(); RedirectorRefsIt; ++RedirectorRefsIt )
					{
						FRedirectorRefs& RedirectorRefs = *RedirectorRefsIt;
						if ( RedirectorRefs.ReferencingPackageNames.Contains(NonCheckedOutPackageName) )
						{
							// We did not check out at least one of the packages we needed to. This redirector can not be fixed up.
							RedirectorRefs.FailureReason = FText::Format(LOCTEXT("RedirectorFixupFailed_NotCheckedOut", "Referencing package {0} was not checked out"), FText::FromName(NonCheckedOutPackageName));
							RedirectorRefs.bRedirectorValidForFixup = false;
						}
					}

					InOutReferencingPackagesToSave.Remove(*PackageIt);
				}
			}
		}
	}

	return bUserAcceptedCheckout;
}

void FAssetFixUpRedirectors::DetectReadOnlyPackages(TArray<FRedirectorRefs>& RedirectorsToFix, TArray<UPackage*>& InOutReferencingPackagesToSave) const
{
	// For each valid package...
	for ( int32 PackageIdx = InOutReferencingPackagesToSave.Num() - 1; PackageIdx >= 0; --PackageIdx )
	{
		UPackage* Package = InOutReferencingPackagesToSave[PackageIdx];

		if ( Package )
		{
			// Find the package filename
			FString Filename;
			if ( FPackageName::DoesPackageExist(Package->GetName(), NULL, &Filename) )
			{
				// If the file is read only
				if ( IFileManager::Get().IsReadOnly(*Filename) )
				{
					FName PackageName = Package->GetFName();

					// Find all assets that were referenced by this package to create a redirector when named
					for ( auto RedirectorIt = RedirectorsToFix.CreateIterator(); RedirectorIt; ++RedirectorIt )
					{
						FRedirectorRefs& RedirectorRefs = *RedirectorIt;
						if ( RedirectorRefs.ReferencingPackageNames.Contains(PackageName) )
						{
							RedirectorRefs.FailureReason = FText::Format(LOCTEXT("RedirectorFixupFailed_ReadOnly", "Referencing package {0} was read-only"), FText::FromName(PackageName));
							RedirectorRefs.bRedirectorValidForFixup = false;
						}
					}

					// Remove the package from the save list
					InOutReferencingPackagesToSave.RemoveAt(PackageIdx);
				}
			}
		}
	}
}

void FAssetFixUpRedirectors::SaveReferencingPackages(const TArray<UPackage*>& ReferencingPackagesToSave, TArray<UPackage*>& OutFailedToSave) const
{
	if ( ReferencingPackagesToSave.Num() > 0 )
	{
		const bool bCheckDirty = false;
		const bool bPromptToSave = false;
		FEditorFileUtils::PromptForCheckoutAndSave(ReferencingPackagesToSave, bCheckDirty, bPromptToSave, &OutFailedToSave);

		ISourceControlModule::Get().QueueStatusUpdate(ReferencingPackagesToSave);
	}
}

void FAssetFixUpRedirectors::SaveReferencingCollections(TArray<FRedirectorRefs>& RedirectorsToFix) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	// Find all collections that were referenced by any of the redirectors that are potentially going to be removed and attempt to re-save them
	// The redirectors themselves will have already been fixed up, as collections do that once the asset registry has been populated, 
	// however collections lazily re-save redirector fix-up to avoid SCC issues, so we need to force that now
	for (FRedirectorRefs& RedirectorRefs : RedirectorsToFix)
	{
		// Follow each link in the redirector, and notify the collections manager that it is going to be removed - this will force it to re-save any required collections
		for (UObjectRedirector* Redirector = RedirectorRefs.Redirector; Redirector; Redirector = Cast<UObjectRedirector>(Redirector->DestinationObject))
		{
			const FName RedirectorObjectPath = *Redirector->GetPathName();
			if (!CollectionManagerModule.Get().HandleRedirectorDeleted(RedirectorObjectPath))
			{
				RedirectorRefs.FailureReason = FText::Format(LOCTEXT("RedirectorFixupFailed_CollectionsFailedToSave", "Referencing collection(s) failed to save: {0}"), CollectionManagerModule.Get().GetLastError());
				RedirectorRefs.bRedirectorValidForFixup = false;
			}
		}
	}
}

void FAssetFixUpRedirectors::UpdateAssetReferencers(const TArray<FRedirectorRefs>& RedirectorsToFix) const
{
	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FString> AssetPaths;
	for (const auto& Redirector : RedirectorsToFix)
	{
		AssetPaths.AddUnique(FPackageName::GetLongPackagePath(Redirector.RedirectorPackageName.ToString()) / TEXT("")); // Ensure trailing slash

		for (const auto& Referencer : Redirector.ReferencingPackageNames)
		{
			AssetPaths.AddUnique(FPackageName::GetLongPackagePath(Referencer.ToString()) / TEXT("")); // Ensure trailing slash
		}
	}
	AssetRegistryModule.Get().ScanPathsSynchronous(AssetPaths, true);
}

void FAssetFixUpRedirectors::DeleteRedirectors(TArray<FRedirectorRefs>& RedirectorsToFix, const TArray<UPackage*>& FailedToSave) const
{
	TArray<UObject*> ObjectsToDelete;
	for ( auto RedirectorIt = RedirectorsToFix.CreateIterator(); RedirectorIt; ++RedirectorIt )
	{
		FRedirectorRefs& RedirectorRefs = *RedirectorIt;
		if ( RedirectorRefs.bRedirectorValidForFixup )
		{
			check(RedirectorRefs.Redirector);

			bool bAllReferencersFixedUp = true;
			for (const auto& ReferencingPackageName : RedirectorRefs.ReferencingPackageNames)
			{
				if (FailedToSave.ContainsByPredicate([&](UPackage* Package) { return Package->GetFName() == ReferencingPackageName; }))
				{
					bAllReferencersFixedUp = false;
					break;
				}
			}

			if (!bAllReferencersFixedUp)
			{
				continue;
			}

			// Add all redirectors found in this package to the redirectors to delete list.
			// All redirectors in this package should be fixed up.
			UPackage* RedirectorPackage = RedirectorRefs.Redirector->GetOutermost();
			TArray<UObject*> AssetsInRedirectorPackage;
			GetObjectsWithOuter(RedirectorPackage, AssetsInRedirectorPackage, /*bIncludeNestedObjects=*/false);
			UMetaData* PackageMetaData = NULL;
			bool bContainsAtLeastOneOtherAsset = false;
			for ( auto ObjIt = AssetsInRedirectorPackage.CreateConstIterator(); ObjIt; ++ObjIt )
			{
				if ( UObjectRedirector* Redirector = Cast<UObjectRedirector>(*ObjIt) )
				{
					Redirector->RemoveFromRoot();
					ObjectsToDelete.Add(Redirector);
				}
				else if ( UMetaData* MetaData = Cast<UMetaData>(*ObjIt) )
				{
					PackageMetaData = MetaData;
				}
				else
				{
					bContainsAtLeastOneOtherAsset = true;
				}
			}

			if ( !bContainsAtLeastOneOtherAsset )
			{
				RedirectorPackage->RemoveFromRoot();
				ObjectsToDelete.Add(RedirectorPackage);

				// @todo we shouldnt be worrying about metadata objects here, ObjectTools::CleanUpAfterSuccessfulDelete should
				if ( PackageMetaData )
				{
					PackageMetaData->RemoveFromRoot();
					ObjectsToDelete.Add(PackageMetaData);
				}
			}

			// This redirector will be deleted, NULL the reference here
			RedirectorRefs.Redirector = NULL;
		}
	}

	if ( ObjectsToDelete.Num() > 0 )
	{
		ObjectTools::DeleteObjects(ObjectsToDelete, false);
	}
}

void FAssetFixUpRedirectors::ReportFailures(const TArray<FRedirectorRefs>& RedirectorsToFix) const
{
	FMessageLog EditorErrors("EditorErrors");
	bool bTitleOutput = false;

	for ( auto RedirectorIt = RedirectorsToFix.CreateConstIterator(); RedirectorIt; ++RedirectorIt )
	{
		const FRedirectorRefs& RedirectorRefs = *RedirectorIt;
		if ( !RedirectorRefs.bRedirectorValidForFixup )
		{
			if(!bTitleOutput)
			{
				EditorErrors.Info(LOCTEXT("RedirectorFixupFailedMessage", "The following redirectors could not be completely fixed up"));
				bTitleOutput = true;
			}
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("PackageName"), FText::FromName(RedirectorRefs.RedirectorPackageName));
			Arguments.Add(TEXT("FailureReason"), FText::FromString(RedirectorRefs.FailureReason.ToString()));
			EditorErrors.Warning(FText::Format(LOCTEXT("RedirectorFixupFailedReason", "{PackageName} - {FailureReason}"), Arguments ));
		}
	}

	EditorErrors.Open();
}

void FAssetFixUpRedirectors::FixUpSoftObjectPaths(const TArray<FRedirectorRefs>& RedirectorsToFix, const TArray<UPackage*>& InReferencingPackagesToSave) const
{
	TArray<UPackage *> PackagesToCheck(InReferencingPackagesToSave);

	FEditorFileUtils::GetDirtyWorldPackages(PackagesToCheck);
	FEditorFileUtils::GetDirtyContentPackages(PackagesToCheck);

	TMap<FSoftObjectPath, FSoftObjectPath> RedirectorMap;

	for (const FRedirectorRefs& RedirectorRef : RedirectorsToFix)
	{
		UObjectRedirector* Redirector = RedirectorRef.Redirector;
		FSoftObjectPath OldPath = FSoftObjectPath(Redirector);
		FSoftObjectPath NewPath = FSoftObjectPath(Redirector->DestinationObject);

		RedirectorMap.Add(OldPath, NewPath);

		if (UBlueprint* Blueprint = Cast<UBlueprint>(Redirector->DestinationObject))
		{
			// Add redirect for class and default as well
			RedirectorMap.Add(FString::Printf(TEXT("%s_C"), *OldPath.ToString()), FString::Printf(TEXT("%s_C"), *NewPath.ToString()));
			RedirectorMap.Add(FString::Printf(TEXT("%s.Default__%s_C"), *OldPath.GetLongPackageName(), *OldPath.GetAssetName()), FString::Printf(TEXT("%s.Default__%s_C"), *NewPath.GetLongPackageName(), *NewPath.GetAssetName()));
		}
	}

	UAssetToolsImpl::Get().AssetRenameManager->RenameReferencingSoftObjectPaths(PackagesToCheck, RedirectorMap);
}

#undef LOCTEXT_NAMESPACE
