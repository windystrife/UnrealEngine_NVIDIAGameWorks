// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PackageReload.h"
#include "StrongObjectPtr.h"
#include "ReferenceChainSearch.h"
#include "UObjectHash.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/Package.h"
#include "UObject/Linker.h"
#include "Templates/Casts.h"
#include "UObject/UObjectIterator.h"

namespace PackageReloadInternal
{

/**
 * Strong reference to an existing package that prevents it being GC'd while we're still using it.
 * Once we're done with it, we clear out the strong reference and use the weak reference to verify that it was purged correctly via GC.
 */
struct FExistingPackageReference
{
	FExistingPackageReference(UPackage* InPackage)
		: RawRef(InPackage)
		, WeakRef(InPackage)
		, StrongRef(InPackage)
	{
	}

	UPackage* RawRef;
	TWeakObjectPtr<UPackage> WeakRef;
	TStrongObjectPtr<UPackage> StrongRef;
};

/**
 * Strong reference to an replacement package that prevents it being GC'd while we're still using it.
 * This also includes the event data used when broadcasting package reload events for this package.
 */
struct FNewPackageReference
{
	FNewPackageReference(UPackage* InPackage)
		: Package(InPackage)
		, EventData()
	{
	}

	TStrongObjectPtr<UPackage> Package;
	TSharedPtr<FPackageReloadedEvent> EventData;
};

/**
 * Used to map objects from the old package, to objects in the new package, including the index of the package being reloaded.
 */
struct FObjectAndPackageIndex
{
	FObjectAndPackageIndex(UObject* InObject, const int32 InPackageIndex)
		: Object(InObject)
		, PackageIndex(InPackageIndex)
	{
	}

	UObject* Object;
	int32 PackageIndex;
};

/**
 * Custom archive type used to re-point any in-memory references to objects in the old package to objects in the new package, or null if there is no replacement object.
 */
class FReplaceObjectReferencesArchive : public FArchiveUObject, public FReferenceCollector
{
public:
	FReplaceObjectReferencesArchive(UObject* InPotentialReferencer, const TMap<UObject*, FObjectAndPackageIndex>& InOldObjectToNewData, const TArray<FExistingPackageReference>& InExistingPackages, const TArray<FNewPackageReference>& InNewPackages)
		: PotentialReferencer(InPotentialReferencer)
		, OldObjectToNewData(InOldObjectToNewData)
		, ExistingPackages(InExistingPackages)
		, NewPackages(InNewPackages)
	{
		ArIsObjectReferenceCollector = true;
		ArIsModifyingWeakAndStrongReferences = true;
		ArIgnoreOuterRef = true;
		ArNoDelta = true;
	}

	virtual ~FReplaceObjectReferencesArchive()
	{
	}

	virtual FString GetArchiveName() const override
	{
		return TEXT("FReplaceObjectReferencesArchive");
	}

	virtual void HandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const UProperty* ReferencingProperty) override
	{
		(*this) << Object;
	}

	virtual void HandleObjectReferences(UObject** InObjects, const int32 ObjectNum, const UObject* InReferencingObject, const UProperty* InReferencingProperty) override
	{
		for (int32 ObjectIndex = 0; ObjectIndex < ObjectNum; ++ObjectIndex)
		{
			UObject*& Object = InObjects[ObjectIndex];
			(*this) << Object;
		}
	}

	virtual bool IsIgnoringArchetypeRef() const override
	{
		return false;
	}

	virtual bool IsIgnoringTransient() const override
	{
		return false;
	}

	FArchive& operator<<(UObject*& ObjRef)
	{
		if (ObjRef && ObjRef != PotentialReferencer)
		{
			UObject* NewObject = nullptr;
			TSharedPtr<FPackageReloadedEvent> PackageEventData;
			if (GetNewObjectAndEventData(ObjRef, NewObject, PackageEventData))
			{
				ObjRef = NewObject;
				PackageEventData->AddObjectReferencer(PotentialReferencer);
			}
		}

		return *this;
	}

	bool GetNewObjectAndEventData(UObject* InOldObject, UObject*& OutNewObject, TSharedPtr<FPackageReloadedEvent>& OutEventData) const
	{
		const FObjectAndPackageIndex* ObjectAndPackageIndexPtr = OldObjectToNewData.Find(InOldObject);

		// Only fix-up references to objects outside of the potential referencer package, as internal object references will be orphaned automatically
		if (ObjectAndPackageIndexPtr && PotentialReferencer->GetOutermost() != ExistingPackages[ObjectAndPackageIndexPtr->PackageIndex].RawRef)
		{
			OutNewObject = ObjectAndPackageIndexPtr->Object;
			OutEventData = NewPackages[ObjectAndPackageIndexPtr->PackageIndex].EventData;
			return true;
		}

		return false;
	}

	UObject* PotentialReferencer;
	const TMap<UObject*, FObjectAndPackageIndex>& OldObjectToNewData;
	const TArray<FExistingPackageReference>& ExistingPackages;
	const TArray<FNewPackageReference>& NewPackages;
};

/**
 * Given a package, mark it and all its sub-objects with the RF_NewerVersionExists flag so that other systems can detect that they're being replaced.
 */
void MarkPackageReplaced(UPackage* InPackage)
{
	InPackage->SetFlags(RF_NewerVersionExists);
	ForEachObjectWithOuter(InPackage, [](UObject* InSubObject)
	{
		InSubObject->SetFlags(RF_NewerVersionExists);
	});
}

/**
 * Given a package, remove the RF_NewerVersionExists flag from it and all its sub-objects.
 */
void ClearPackageReplaced(UPackage* InPackage)
{
	InPackage->ClearFlags(RF_NewerVersionExists);
	ForEachObjectWithOuter(InPackage, [](UObject* InSubObject)
	{
		InSubObject->ClearFlags(RF_NewerVersionExists);
	});
}

/**
 * Given an object, put it into a state where a GC may purge it (assuming there are no external references).
 */
void MakeObjectPurgeable(UObject* InObject)
{
	if (InObject->IsRooted())
	{
		InObject->RemoveFromRoot();
	}
	InObject->ClearFlags(RF_Public | RF_Standalone);
}

/**
 * Given an object, dump anything that is still externally referencing it to the log.
 */
void DumpExternalReferences(UObject* InObject, UPackage* InPackage)
{
	TArray<FString> ExternalRefDumps;

	{
		FReferenceChainSearch ObjectRefChains(InObject, FReferenceChainSearch::ESearchMode::Default);
		for (const FReferenceChainSearch::FReferenceChain& ObjectRefChain : ObjectRefChains.GetReferenceChains())
		{
			for (const FReferenceChainSearch::FReferenceChainLink& ObjectRefChainLink : ObjectRefChain.RefChain)
			{
				const bool bIsExternalRef = !ObjectRefChainLink.ReferencedBy || ObjectRefChainLink.ReferencedBy->GetOutermost() != InPackage;
				if (bIsExternalRef)
				{
					ExternalRefDumps.Emplace(ObjectRefChainLink.ToString());
				}
			}
		}
	}

	if (ExternalRefDumps.Num() > 0)
	{
		UE_LOG(LogUObjectGlobals, Display, TEXT("ReloadPackage external references for '%s'."), *InObject->GetPathName());
		for (const FString& ExternalRefDump : ExternalRefDumps)
		{
			UE_LOG(LogUObjectGlobals, Display, TEXT("    %s"), *ExternalRefDump);
		}
	}
}

/**
 * Given a package, validate and prepare it for reload.
 * @return The package to be reloaded, or null if the given package isn't valid to be reloaded.
 */
UPackage* ValidateAndPreparePackageForReload(UPackage* InExistingPackage)
{
	// We can't reload memory-only packages
	if (InExistingPackage->HasAnyPackageFlags(PKG_InMemoryOnly))
	{
		UE_LOG(LogUObjectGlobals, Warning, TEXT("ReloadPackage cannot reload '%s' as it is marked PKG_InMemoryOnly."), *InExistingPackage->GetName());
		return nullptr;
	}

	// Make sure the package has finished loading before we try and unload it again
	if (!InExistingPackage->IsFullyLoaded())
	{
		FlushAsyncLoading();
		InExistingPackage->FullyLoad();
	}
	ResetLoaders(InExistingPackage);

	return InExistingPackage;
}

/**
 * Given a package, reload it from disk.
 * @return The package that was reloaded, or null if the given package couldn't be reloaded.
 */
UPackage* LoadReplacementPackage(UPackage* InExistingPackage, const uint32 InLoadFlags)
{
	if (!InExistingPackage)
	{
		return nullptr;
	}

	const FString ExistingPackageName = InExistingPackage->GetName();

	// Rename the old package, and then load the new one in its place
	const ERenameFlags PkgRenameFlags = REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional | REN_SkipGeneratedClasses;
	InExistingPackage->Rename(*MakeUniqueObjectName(Cast<UPackage>(InExistingPackage->GetOuter()), UPackage::StaticClass(), *FString::Printf(TEXT("%s_DEADPACKAGE"), *InExistingPackage->GetName())).ToString(), nullptr, PkgRenameFlags);
	MarkPackageReplaced(InExistingPackage);
	UPackage* NewPackage = LoadPackage(Cast<UPackage>(InExistingPackage->GetOuter()), *ExistingPackageName, InLoadFlags);
	if (!NewPackage)
	{
		UE_LOG(LogUObjectGlobals, Warning, TEXT("ReloadPackage cannot reload '%s' as the new package failed to load. The old package will be restored."), *ExistingPackageName);

		// Failed to load the new package, give the old package its original name and bail!
		InExistingPackage->Rename(*ExistingPackageName, nullptr, PkgRenameFlags);
		ClearPackageReplaced(InExistingPackage);
		return nullptr;
	}

	// Make sure the package has finished loading before we try and find things from inside it
	if (!NewPackage->IsFullyLoaded())
	{
		FlushAsyncLoading();
		NewPackage->FullyLoad();
	}

	return NewPackage;
}

/**
 * Given an old and new package, generate the event payload data needed to fix-up references to objects from the old package to the corresponding objects in the new package.
 * @return The event payload data, or null if either given package is invalid.
 */
TSharedPtr<FPackageReloadedEvent> GeneratePackageReloadEvent(UPackage* InExistingPackage, UPackage* InNewPackage)
{
	TSharedPtr<FPackageReloadedEvent> PackageReloadedEvent;

	if (InExistingPackage && InNewPackage)
	{
		TMap<UObject*, UObject*> RedirectedObjectsMap;
		RedirectedObjectsMap.Emplace(InExistingPackage, InNewPackage);
		InExistingPackage->BuildSubobjectMapping(InNewPackage, RedirectedObjectsMap);

		for (const auto& ObjectMappingPair : RedirectedObjectsMap)
		{
			UObject* ExistingObject = ObjectMappingPair.Key;
			UObject* NewObject = ObjectMappingPair.Value;

			if (NewObject)
			{
				// Pass on the root-set state from the old object to the new one
				if (ExistingObject->IsRooted())
				{
					NewObject->AddToRoot();
				}

				// Pass on some important flags to the new object
				{
					const EObjectFlags FlagsToPassToNewObject = ExistingObject->GetMaskedFlags(RF_Public | RF_Standalone | RF_Transactional);
					NewObject->SetFlags(FlagsToPassToNewObject);
				}
			}
			else if (ExistingObject->HasAnyFlags(RF_Transient))
			{
				UE_LOG(LogUObjectGlobals, Display, TEXT("ReloadPackage failed to find a replacement object for '%s' (transient) in the new package '%s'. Any existing references to this object will be nulled out."), *ExistingObject->GetPathName(InExistingPackage), *InNewPackage->GetName());
			}
			else
			{
				UE_LOG(LogUObjectGlobals, Warning, TEXT("ReloadPackage failed to find a replacement object for '%s' in the new package '%s'. Any existing references to this object will be nulled out."), *ExistingObject->GetPathName(InExistingPackage), *InNewPackage->GetName());
			}
		}

		PackageReloadedEvent = MakeShared<FPackageReloadedEvent>(InExistingPackage, InNewPackage, MoveTemp(RedirectedObjectsMap));
	}

	return PackageReloadedEvent;
}

void SortPackagesForReload(const FName PackageName, TSet<FName>& ProcessedPackages, TArray<UPackage*>& SortedPackagesToReload, const TMap<FName, UPackage*>& AllPackagesToReload, IAssetRegistryInterface& InAssetRegistry)
{
	ProcessedPackages.Add(PackageName);

	TArray<FName> PackageDependencies;
	InAssetRegistry.GetDependencies(PackageName, PackageDependencies, EAssetRegistryDependencyType::Hard);

	// Recursively go through processing each new dependency until we run out
	for (const FName Dependency : PackageDependencies)
	{
		if (!ProcessedPackages.Contains(Dependency))
		{
			SortPackagesForReload(Dependency, ProcessedPackages, SortedPackagesToReload, AllPackagesToReload, InAssetRegistry);
		}
	}

	// Add this package to the sorted array now that its dependencies have been processed
	if (AllPackagesToReload.Contains(PackageName))
	{
		SortedPackagesToReload.Emplace(AllPackagesToReload[PackageName]);
	}
}

} // namespace PackageReloadInternal

void SortPackagesForReload(TArray<UPackage*>& PackagesToReload)
{
	// We need to sort the packages to reload so that dependencies are reloaded before the assets that depend on them
	if (PackagesToReload.Num() > 1)
	{
		IAssetRegistryInterface* AssetRegistry = IAssetRegistryInterface::GetPtr();
		checkf(AssetRegistry, TEXT("SortPackagesForReload requires the asset registry to perform dependency analysis, but no asset registry is available."));

		TSet<FName> ProcessedPackages;
		ProcessedPackages.Reserve(PackagesToReload.Num());

		TArray<UPackage*> SortedPackagesToReload;
		SortedPackagesToReload.Reserve(PackagesToReload.Num());

		TMap<FName, UPackage*> AllPackagesToReload;
		AllPackagesToReload.Reserve(PackagesToReload.Num());
		for (UPackage* PackageToReload : PackagesToReload)
		{
			AllPackagesToReload.Emplace(PackageToReload->GetFName(), PackageToReload);
		}

		for (UPackage* PackageToReload : PackagesToReload)
		{
			if (!ProcessedPackages.Contains(PackageToReload->GetFName()))
			{
				PackageReloadInternal::SortPackagesForReload(PackageToReload->GetFName(), ProcessedPackages, SortedPackagesToReload, AllPackagesToReload, *AssetRegistry);
			}
		}

		PackagesToReload = MoveTemp(SortedPackagesToReload);
	}
}

UPackage* ReloadPackage(UPackage* InPackageToReload, const uint32 InLoadFlags)
{
	TArray<UPackage*> ReloadedPackages;
	
	FReloadPackageData ReloadPackageData(InPackageToReload, InLoadFlags);
	ReloadPackages(TArrayView<FReloadPackageData>(&ReloadPackageData, 1), ReloadedPackages, 1);

	return ReloadedPackages[0];
}

void ReloadPackages(const TArrayView<FReloadPackageData>& InPackagesToReload, TArray<UPackage*>& OutReloadedPackages, const int32 InNumPackagesPerBatch)
{
	FScopedSlowTask ReloadingPackagesSlowTask(InPackagesToReload.Num(), NSLOCTEXT("CoreUObject", "ReloadingPackages", "Reloading Packages"));
	ReloadingPackagesSlowTask.MakeDialog();

	// Cache the current dirty state of all packages so we can restore it after the reload
	TSet<FName> DirtyPackages;
	ForEachObjectOfClass(UPackage::StaticClass(), [&](UObject* InPackageObj)
	{
		UPackage* Package = CastChecked<UPackage>(InPackageObj);
		if (Package->IsDirty())
		{
			DirtyPackages.Add(Package->GetFName());
		}
	}, false);

	// Gather up the list of all packages to reload (note: this array may include null packages!)
	TArray<PackageReloadInternal::FExistingPackageReference> ExistingPackages;
	ExistingPackages.Reserve(InPackagesToReload.Num());
	{
		FScopedSlowTask PreparingPackagesForReloadSlowTask(InPackagesToReload.Num(), NSLOCTEXT("CoreUObject", "PreparingPackagesForReload", "Preparing Packages for Reload"));

		for (const FReloadPackageData& PackageToReloadData : InPackagesToReload)
		{
			PreparingPackagesForReloadSlowTask.EnterProgressFrame(1.0f);
			ExistingPackages.Emplace(PackageReloadInternal::ValidateAndPreparePackageForReload(PackageToReloadData.PackageToReload));
		}
	}

	// Rename the existing packages, load the new packages, then fix-up any references
	TArray<PackageReloadInternal::FNewPackageReference> NewPackages;
	NewPackages.Reserve(ExistingPackages.Num());
	{
		// Process the packages in batches to avoid consuming too much memory due to a lack of GC
		int32 PackageIndex = 0;
		while (PackageIndex < ExistingPackages.Num())
		{
			FCoreUObjectDelegates::OnPackageReloaded.Broadcast(EPackageReloadPhase::PreBatch, nullptr);

			const int32 BatchStartIndex = PackageIndex;
			for (; PackageIndex < ExistingPackages.Num(); ++PackageIndex)
			{
				UPackage* ExistingPackage = ExistingPackages[PackageIndex].RawRef;

				const FText ProgressText = ExistingPackage 
					? FText::Format(NSLOCTEXT("CoreUObject", "ReloadingPackagef", "Reloading {0}..."), FText::FromName(ExistingPackage->GetFName()))
					: NSLOCTEXT("CoreUObject", "ReloadingPackages", "Reloading Packages");
				ReloadingPackagesSlowTask.EnterProgressFrame(1, ProgressText);

				check(NewPackages.Num() == PackageIndex);
				NewPackages.Emplace(PackageReloadInternal::LoadReplacementPackage(ExistingPackage, InPackagesToReload[PackageIndex].LoadFlags));

				UPackage* NewPackage = NewPackages[PackageIndex].Package.Get();
				NewPackages[PackageIndex].EventData = PackageReloadInternal::GeneratePackageReloadEvent(ExistingPackage, NewPackage);

				const bool bEndBatch = (PackageIndex == (BatchStartIndex + InNumPackagesPerBatch)) || (ExistingPackage && ExistingPackage->ContainsMap());
				if (bEndBatch)
				{
					++PackageIndex; // We still need to move on-to the next package for the next batch
					break;
				}
			}

			const int32 NumPackagesInBatch = PackageIndex - BatchStartIndex;

			FScopedSlowTask FixingUpReferencesSlowTask((NumPackagesInBatch * 4) + GUObjectArray.GetObjectArrayNum(), NSLOCTEXT("CoreUObject", "FixingUpReferences", "Fixing-Up References"));

			// Pre-pass to notify things that the package old package is about to be fixed-up
			TMap<UObject*, PackageReloadInternal::FObjectAndPackageIndex> OldObjectToNewData;
			for (int32 BatchPackageIndex = BatchStartIndex; BatchPackageIndex < PackageIndex; ++BatchPackageIndex)
			{
				FixingUpReferencesSlowTask.EnterProgressFrame(1.0f);

				PackageReloadInternal::FNewPackageReference& NewPackageData = NewPackages[BatchPackageIndex];
				if (NewPackageData.EventData.IsValid())
				{
					FCoreUObjectDelegates::OnPackageReloaded.Broadcast(EPackageReloadPhase::PrePackageFixup, NewPackageData.EventData.Get());
					FCoreUObjectDelegates::OnPackageReloaded.Broadcast(EPackageReloadPhase::OnPackageFixup, NewPackageData.EventData.Get());

					// Build up the mapping of old objects to the package index that contains them; this is needed to track per-package references correctly
					OldObjectToNewData.Reserve(OldObjectToNewData.Num() + NewPackageData.EventData->GetRepointedObjects().Num());
					for (const auto& ObjectMappingPair : NewPackageData.EventData->GetRepointedObjects())
					{
						OldObjectToNewData.Add(ObjectMappingPair.Key, PackageReloadInternal::FObjectAndPackageIndex(ObjectMappingPair.Value, BatchPackageIndex));
					}
				}
			}

			// Main pass to go through and fix-up any references pointing to data from the old package to point to data from the new package
			// todo: multi-thread this like FHotReloadModule::ReplaceReferencesToReconstructedCDOs?
			for (FObjectIterator ObjIter(UObject::StaticClass(), false, RF_NoFlags, EInternalObjectFlags::PendingKill); ObjIter; ++ObjIter)
			{
				UObject* PotentialReferencer = *ObjIter;

				FixingUpReferencesSlowTask.EnterProgressFrame(1.0f);

				PackageReloadInternal::FReplaceObjectReferencesArchive ReplaceRefsArchive(PotentialReferencer, OldObjectToNewData, ExistingPackages, NewPackages);
				PotentialReferencer->Serialize(ReplaceRefsArchive); // Deal with direct references during Serialization
				PotentialReferencer->GetClass()->CallAddReferencedObjects(PotentialReferencer, ReplaceRefsArchive); // Deal with indirect references via AddReferencedObjects
			}

			// The above fix-up also repoints the StrongRef in FExistingPackageReference, so we'll fix that up again now to prevent the old package from being GC'd
			for (int32 BatchPackageIndex = BatchStartIndex; BatchPackageIndex < PackageIndex; ++BatchPackageIndex)
			{
				FixingUpReferencesSlowTask.EnterProgressFrame(1.0f);

				ExistingPackages[BatchPackageIndex].StrongRef.Reset(ExistingPackages[BatchPackageIndex].RawRef);
			}

			// Final pass to clean-up any remaining references prior to GC
			// Note: We do this as a separate pass to preparing the objects for GC as this callback may prematurely invoke a GC that invalidates some data we're working with
			for (int32 BatchPackageIndex = BatchStartIndex; BatchPackageIndex < PackageIndex; ++BatchPackageIndex)
			{
				FixingUpReferencesSlowTask.EnterProgressFrame(1.0f);

				PackageReloadInternal::FNewPackageReference& NewPackageData = NewPackages[BatchPackageIndex];
				if (NewPackageData.EventData.IsValid())
				{
					FCoreUObjectDelegates::OnPackageReloaded.Broadcast(EPackageReloadPhase::PostPackageFixup, NewPackageData.EventData.Get());
				}
			}

			FCoreUObjectDelegates::OnPackageReloaded.Broadcast(EPackageReloadPhase::PostBatchPreGC, nullptr);

			// Purge old packages
			for (int32 BatchPackageIndex = BatchStartIndex; BatchPackageIndex < PackageIndex; ++BatchPackageIndex)
			{
				FixingUpReferencesSlowTask.EnterProgressFrame(1.0f);

				UPackage* ExistingPackage = ExistingPackages[BatchPackageIndex].RawRef;
				if (ExistingPackage)
				{
					// Allow the old package to be GC'd
					PackageReloadInternal::MakeObjectPurgeable(ExistingPackage);
					ForEachObjectWithOuter(ExistingPackage, [&](UObject* InExistingObject)
					{
						PackageReloadInternal::MakeObjectPurgeable(InExistingObject);
					});
					ExistingPackages[BatchPackageIndex].StrongRef.Reset();
				}
			}
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			FCoreUObjectDelegates::OnPackageReloaded.Broadcast(EPackageReloadPhase::PostBatchPostGC, nullptr);
		}
	}

	// Clean any packages that we dirtied as part of the replacement process
	ForEachObjectOfClass(UPackage::StaticClass(), [&](UObject* InPackageObj)
	{
		UPackage* Package = CastChecked<UPackage>(InPackageObj);
		if (Package->IsDirty() && !DirtyPackages.Contains(Package->GetFName()))
		{
			Package->SetDirtyFlag(false);
		}
	}, false);

	// Finalization and error reporting
	OutReloadedPackages.Reserve(ExistingPackages.Num());
	for (int32 PackageIndex = 0; PackageIndex < ExistingPackages.Num(); ++PackageIndex)
	{
		UPackage* ExistingPackage = ExistingPackages[PackageIndex].WeakRef.Get();
		UPackage* NewPackage = NewPackages[PackageIndex].Package.Get();

		check(OutReloadedPackages.Num() == PackageIndex);
		OutReloadedPackages.Emplace(NewPackage);

		// Report any old packages that failed to purge
		if (ExistingPackage && NewPackage)
		{
			UE_LOG(LogUObjectGlobals, Warning, TEXT("ReloadPackage failed to purge the old package '%s'. This is unexpected, and likely means that it was still externally referenced."), *ExistingPackage->GetName());

			const bool bDumpExternalReferences = DO_GUARD_SLOW || (WITH_EDITOR && GIsEditor);
			if (bDumpExternalReferences)
			{
				PackageReloadInternal::DumpExternalReferences(ExistingPackage, ExistingPackage);
				//ForEachObjectWithOuter(ExistingPackage, [&](UObject* InExistingObject)
				//{
				//	PackageReloadInternal::DumpExternalReferences(InExistingObject, ExistingPackage);
				//}, true, RF_NoFlags, EInternalObjectFlags::PendingKill);
			}
		}
	}
}
