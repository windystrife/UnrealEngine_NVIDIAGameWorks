// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/PrimaryAssetLabel.h"
#include "AssetData.h"
#include "ARFilter.h"
#include "Engine/Engine.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectHash.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/Paths.h"
#include "AssetRegistryState.h"
#include "HAL/PlatformFilemanager.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Commandlets/ChunkDependencyInfo.h"
#endif

#define LOCTEXT_NAMESPACE "AssetManager"

DEFINE_LOG_CATEGORY_STATIC(LogAssetManager, Log, All);

/** Structure defining the current loading state of an asset */
struct FPrimaryAssetLoadState
{
	/** The handle to the streamable state for this asset, this keeps the objects in memory. If handle is invalid, not in memory at all */
	TSharedPtr<FStreamableHandle> Handle;

	/** The set of bundles to be loaded by the handle */
	TArray<FName> BundleNames;

	/** If this state is keeping things in memory */
	bool IsValid() const { return Handle.IsValid() && Handle->IsActive(); }

	/** Reset this state */
	void Reset(bool bCancelHandle)
	{
		if (Handle.IsValid())
		{
			if (Handle->IsActive() && bCancelHandle)
			{
				// This will call the cancel callback if set
				Handle->CancelHandle();
			}
			
			Handle = nullptr;
		}
		BundleNames.Reset();
	}
};

/** Structure representing data about a specific asset */
struct FPrimaryAssetData
{
	/** Path used to look up cached asset data in the asset registry. This will be missing the _C for blueprint classes */
	FName AssetDataPath;

	/** Path to this asset on disk */
	FSoftObjectPtr AssetPtr;

	/** Current state of this asset */
	FPrimaryAssetLoadState CurrentState;

	/** Pending state of this asset, will be copied to CurrentState when load finishes */
	FPrimaryAssetLoadState PendingState;

	FPrimaryAssetData() {}

	/** Asset is considered loaded at all if there is an active handle for it */
	bool IsLoaded() const { return CurrentState.IsValid(); }
};

/** Structure representing all items of a specific asset type */
struct FPrimaryAssetTypeData
{
	/** The public info struct */
	FPrimaryAssetTypeInfo Info;

	/** Map of scanned assets */
	TMap<FName, FPrimaryAssetData> AssetMap;

	/** In the editor, paths that we need to scan once asset registry is done loading */
	TArray<FString> DeferredAssetScanPaths;

	FPrimaryAssetTypeData() {}

	FPrimaryAssetTypeData(FName InPrimaryAssetType, UClass* InAssetBaseClass, bool bInHasBlueprintClasses, bool bInIsEditorOnly)
		: Info(InPrimaryAssetType, InAssetBaseClass, bInHasBlueprintClasses, bInIsEditorOnly)
		{}
};

const FPrimaryAssetType UAssetManager::MapType = FName(TEXT("Map"));
const FPrimaryAssetType UAssetManager::PrimaryAssetLabelType = FName(TEXT("PrimaryAssetLabel"));

UAssetManager::UAssetManager()
{
	bIsGlobalAsyncScanEnvironment = false;
	bShouldGuessTypeAndName = false;
	bShouldUseSynchronousLoad = false;
	bIsLoadingFromPakFiles = false;
	bShouldAcquireMissingChunksOnLoad = false;
	bIsBulkScanning = false;
	bIsManagementDatabaseCurrent = false;
	bUpdateManagementDatabaseAfterScan = false;
	bIncludeOnlyOnDiskAssets = true;
	NumberOfSpawnedNotifications = 0;
}

void UAssetManager::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		const UAssetManagerSettings& Settings = GetSettings();
#if WITH_EDITOR
		bIsGlobalAsyncScanEnvironment = GIsEditor && !IsRunningCommandlet();

		if (bIsGlobalAsyncScanEnvironment)
		{
			// Listen for when the asset registry has finished discovering files
			IAssetRegistry& AssetRegistry = GetAssetRegistry();

			AssetRegistry.OnFilesLoaded().AddUObject(this, &UAssetManager::OnAssetRegistryFilesLoaded);
			AssetRegistry.OnInMemoryAssetCreated().AddUObject(this, &UAssetManager::OnInMemoryAssetCreated);
			AssetRegistry.OnInMemoryAssetDeleted().AddUObject(this, &UAssetManager::OnInMemoryAssetDeleted);
			AssetRegistry.OnAssetRenamed().AddUObject(this, &UAssetManager::OnAssetRenamed);
		}

		FEditorDelegates::PreBeginPIE.AddUObject(this, &UAssetManager::PreBeginPIE);
		FEditorDelegates::EndPIE.AddUObject(this, &UAssetManager::EndPIE);

		// In editor builds guess the type/name if allowed
		bShouldGuessTypeAndName = Settings.bShouldGuessTypeAndNameInEditor;
		bOnlyCookProductionAssets = Settings.bOnlyCookProductionAssets;

		// In editor builds, always allow asset registry searches for in-memory asset data, as that data can change when propagating AssetBundle tags post load.
		bIncludeOnlyOnDiskAssets = false;
#else 
		// Never guess type in cooked builds
		bShouldGuessTypeAndName = false;

		// Only cooked builds supoprt pak files and chunk download
		bIsLoadingFromPakFiles = FPlatformFileManager::Get().FindPlatformFile(TEXT("PakFile")) != nullptr;
		bShouldAcquireMissingChunksOnLoad = Settings.bShouldAcquireMissingChunksOnLoad;
#endif
		
		bShouldUseSynchronousLoad = IsRunningCommandlet();

		LoadRedirectorMaps();
	}
}

bool UAssetManager::IsValid()
{
	if (GEngine && GEngine->AssetManager)
	{
		return true;
	}

	return false;
}

UAssetManager& UAssetManager::Get()
{
	UAssetManager* Singleton = GEngine->AssetManager;

	if (Singleton)
	{
		return *Singleton;
	}
	else
	{
		UE_LOG(LogAssetManager, Fatal, TEXT("Cannot use AssetManager if no AssetManagerClassName is defined!"));
		return *NewObject<UAssetManager>(); // never calls this
	}
}

UAssetManager* UAssetManager::GetIfValid()
{
	if (GEngine && GEngine->AssetManager)
	{
		return GEngine->AssetManager;
	}

	return nullptr;
}

IAssetRegistry& UAssetManager::GetAssetRegistry() const
{
	if (!CachedAssetRegistry)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		CachedAssetRegistry = &AssetRegistryModule.Get();
	}

	return *CachedAssetRegistry;
}

const UAssetManagerSettings& UAssetManager::GetSettings() const
{
	if (!CachedSettings)
	{
		CachedSettings = GetDefault<UAssetManagerSettings>();
	}
	return *CachedSettings;
}

int32 UAssetManager::ScanPathsForPrimaryAssets(FPrimaryAssetType PrimaryAssetType, const TArray<FString>& Paths, UClass* BaseClass, bool bHasBlueprintClasses, bool bIsEditorOnly, bool bForceSynchronousScan)
{
	TArray<FString> Directories, PackageNames;
	TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(PrimaryAssetType);

	if (bIsEditorOnly && !GIsEditor)
	{
		return 0;
	}

	check(BaseClass);

	if (!FoundType)
	{
		TSharedPtr<FPrimaryAssetTypeData> NewAsset = MakeShareable(new FPrimaryAssetTypeData(PrimaryAssetType, BaseClass, bHasBlueprintClasses, bIsEditorOnly));

		FoundType = &AssetTypeMap.Add(PrimaryAssetType, NewAsset.ToSharedRef());
	}

	// Should always be valid
	check(FoundType);

	FPrimaryAssetTypeData& TypeData = FoundType->Get();

	// Make sure types match
	if (!ensure(TypeData.Info.AssetBaseClassLoaded == BaseClass && TypeData.Info.bHasBlueprintClasses == bHasBlueprintClasses && TypeData.Info.bIsEditorOnly == bIsEditorOnly))
	{
		return 0;
	}

	// Add path info
	for (const FString& Path : Paths)
	{
		TypeData.Info.AssetScanPaths.AddUnique(Path);

		int32 DotIndex = INDEX_NONE;
		if (Path.FindChar('.', DotIndex))
		{
			FString PackageName = FPackageName::ObjectPathToPackageName(Path);

			PackageNames.AddUnique(PackageName);
		}
		else
		{
			Directories.AddUnique(Path);
		}
	}

	IAssetRegistry& AssetRegistry = GetAssetRegistry();

#if WITH_EDITOR
	// Cooked data has the asset data already set up
	const bool bShouldDoSynchronousScan = !bIsGlobalAsyncScanEnvironment || bForceSynchronousScan;
	if (bShouldDoSynchronousScan)
	{
		ScanPathsSynchronous(Paths);
	}
	else
	{
		if (AssetRegistry.IsLoadingAssets())
		{
			// Keep track of the paths we asked for so once assets are discovered we will refresh the list
			for (const FString& Path : Paths)
			{
				TypeData.DeferredAssetScanPaths.AddUnique(Path);
			}
		}
	}
#endif

	FARFilter ARFilter;

	TSet<FName> DerivedClassNames;
	TArray<FName> ClassNames;

	if (BaseClass)
	{
		if (!bShouldGuessTypeAndName)
		{
			// Primary type check
			ARFilter.TagsAndValues.Add(FPrimaryAssetId::PrimaryAssetTypeTag, PrimaryAssetType.ToString());
		}

		// Class check
		if (!bHasBlueprintClasses)
		{
			// For base classes, can do the filter before hand
			ARFilter.ClassNames.Add(BaseClass->GetFName());

#if WITH_EDITOR
			// Add any old names to the list in case things haven't been resaved
			TArray<FName> OldNames = FLinkerLoad::FindPreviousNamesForClass(BaseClass->GetPathName(), false);
			ARFilter.ClassNames.Append(OldNames);
#endif

			ARFilter.bRecursiveClasses = true;
		}
		else
		{
			TArray<UClass*> BlueprintCoreDerivedClasses;
			GetDerivedClasses(UBlueprintCore::StaticClass(), BlueprintCoreDerivedClasses);
			for (UClass* BPCoreClass : BlueprintCoreDerivedClasses)
			{
				ARFilter.ClassNames.Add(BPCoreClass->GetFName());
			}

			// Make sure this works, if it does remove post load check
			ClassNames.Add(BaseClass->GetFName());
			AssetRegistry.GetDerivedClassNames(ClassNames, TSet<FName>(), DerivedClassNames);

			/* DerivedClassNames are short names, GeneratedClass is a long name
			const FName GeneratedClassName = FName(TEXT("GeneratedClass"));
			for (const FName& DerivedClassName : DerivedClassNames)
			{
			ARFilter.TagsAndValues.Add(GeneratedClassName, DerivedClassName.ToString());

			// Add any old names to the list in case things haven't been resaved
			TArray<FName> OldNames = FLinkerLoad::FindPreviousNamesForClass(BaseClass->GetPathName(), false);

			for (const FName& OldClassName : OldNames)
			{
			ARFilter.TagsAndValues.Add(GeneratedClassName, OldClassName.ToString());
			}
			}
			*/
		}
	}

	for (const FString& Directory : Directories)
	{
		ARFilter.PackagePaths.Add(FName(*Directory));
	}

	for (const FString& PackageName : PackageNames)
	{
		ARFilter.PackageNames.Add(FName(*PackageName));
	}

	ARFilter.bRecursivePaths = true;
	ARFilter.bIncludeOnlyOnDiskAssets = !GIsEditor; // In editor check in memory, otherwise don't

	TArray<FAssetData> AssetDataList;

	AssetRegistry.GetAssets(ARFilter, AssetDataList);

	int32 NumAdded = 0;
	// Now add to map or update as needed
	for (FAssetData& Data : AssetDataList)
	{
		// Check exclusion path
		if (IsPathExcludedFromScan(Data.PackageName.ToString()))
		{
			continue;
		}

		// Verify blueprint class
		if (bHasBlueprintClasses)
		{
			bool bShouldRemove = true;
			const FString ParentClassFromData = Data.GetTagValueRef<FString>("ParentClass");
			if (!ParentClassFromData.IsEmpty())
			{
				const FString ClassObjectPath = FPackageName::ExportTextPathToObjectPath(ParentClassFromData);
				const FString ClassName = FPackageName::ObjectPathToObjectName(ClassObjectPath);

				TArray<FName> ValidNames;
#if WITH_EDITOR
				// Also check old names
				ValidNames = FLinkerLoad::FindPreviousNamesForClass(BaseClass->GetPathName(), false);
#endif
				ValidNames.Add(FName(*ClassName));		
				for (const FName& ValidName : ValidNames)
				{
					if (DerivedClassNames.Contains(ValidName))
					{
						// This asset is derived from ObjectBaseClass. Keep it.
						bShouldRemove = false;
						break;
					}
				}
			}

			if (bShouldRemove)
			{
				continue;
			}
		}

		FPrimaryAssetId PrimaryAssetId = ExtractPrimaryAssetIdFromData(Data, PrimaryAssetType);

		// Remove invalid or wrong type assets
		if (!PrimaryAssetId.IsValid() || PrimaryAssetId.PrimaryAssetType != PrimaryAssetType)
		{
			continue;
		}

		NumAdded++;

		UpdateCachedAssetData(PrimaryAssetId, Data, false);
	}

	if (!bIsBulkScanning)
	{
		RebuildObjectReferenceList();
	}

	return NumAdded;
}

void UAssetManager::StartBulkScanning()
{
	if (ensure(!bIsBulkScanning))
	{
		bIsBulkScanning = true;
		NumberOfSpawnedNotifications = 0;
	}
}

void UAssetManager::StopBulkScanning()
{
	if (ensure(bIsBulkScanning))
	{
		bIsBulkScanning = false;
	}
	RebuildObjectReferenceList();
}

void UAssetManager::UpdateCachedAssetData(const FPrimaryAssetId& PrimaryAssetId, const FAssetData& NewAssetData, bool bAllowDuplicates)
{
	const TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(PrimaryAssetId.PrimaryAssetType);

	if (ensure(FoundType))
	{
		FPrimaryAssetTypeData& TypeData = FoundType->Get();

		FPrimaryAssetData* OldData = TypeData.AssetMap.Find(PrimaryAssetId.PrimaryAssetName);

		FSoftObjectPath NewAssetPath = GetAssetPathForData(NewAssetData);

		ensure(NewAssetPath.IsAsset());

		if (OldData && OldData->AssetPtr.ToSoftObjectPath() != NewAssetPath)
		{
			UE_LOG(LogAssetManager, Warning, TEXT("Found Duplicate PrimaryAssetID %s, this must be resolved before saving. Path %s is replacing path %s"), *PrimaryAssetId.ToString(), *OldData->AssetPtr.ToString(), *NewAssetPath.ToString());
			if (!bAllowDuplicates)
			{
				ensureMsgf(!OldData, TEXT("Found Duplicate PrimaryAssetID %s! Path %s is replacing path %s"), *PrimaryAssetId.ToString(), *OldData->AssetPtr.ToString(), *NewAssetPath.ToString());
			}

#if WITH_EDITOR
			if (GIsEditor)
			{
				const int MaxNotificationsPerFrame = 5;
				if (NumberOfSpawnedNotifications++ < MaxNotificationsPerFrame)
				{
					FNotificationInfo Info(FText::Format(LOCTEXT("DuplicateAssetId", "Duplicate Asset ID {0} used by {1} and {2}, you must delete or rename one!"),
						FText::FromString(PrimaryAssetId.ToString()), FText::FromString(OldData->AssetPtr.ToSoftObjectPath().GetLongPackageName()), FText::FromString(NewAssetPath.GetLongPackageName())));
					Info.ExpireDuration = 30.0f;

					TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
					if (Notification.IsValid())
					{
						Notification->SetCompletionState(SNotificationItem::CS_Fail);
					}
				}
			}
#endif
		}

		FPrimaryAssetData& NameData = TypeData.AssetMap.FindOrAdd(PrimaryAssetId.PrimaryAssetName);

		// Update data and path, don't touch state or references
		NameData.AssetDataPath = NewAssetData.ObjectPath; // This will not have _C
		NameData.AssetPtr = FSoftObjectPtr(NewAssetPath); // This will have _C

		// If the types don't match, update the registry
		FPrimaryAssetId SavedId = NewAssetData.GetPrimaryAssetId();

		if (SavedId != PrimaryAssetId)
		{
			GetAssetRegistry().SetPrimaryAssetIdForObjectPath(NameData.AssetDataPath, PrimaryAssetId);
		}

		if (bIsBulkScanning)
		{
			// Do a partial update, add to the path->asset map
			AssetPathMap.Add(NewAssetPath.GetAssetPathName(), PrimaryAssetId);
		}

		if (OldData)
		{
			CachedAssetBundles.Remove(PrimaryAssetId);
		}
		
		// Mark these as editor only if our type is editor only
		FSoftObjectPathSerializationScope SerializationScope(NAME_None, NAME_None, TypeData.Info.bIsEditorOnly ? ESoftObjectPathCollectType::EditorOnlyCollect : ESoftObjectPathCollectType::AlwaysCollect);

		FAssetBundleData BundleData;
		if (BundleData.SetFromAssetData(NewAssetData))
		{
			for (FAssetBundleEntry& Entry : BundleData.Bundles)
			{
				if (Entry.BundleScope.IsValid() && Entry.BundleScope == PrimaryAssetId)
				{
					TMap<FName, FAssetBundleEntry>& BundleMap = CachedAssetBundles.FindOrAdd(PrimaryAssetId);

					BundleMap.Emplace(Entry.BundleName, Entry);
				}
			}
		}
	}
}

int32 UAssetManager::ScanPathForPrimaryAssets(FPrimaryAssetType PrimaryAssetType, const FString& Path, UClass* BaseClass, bool bHasBlueprintClasses, bool bIsEditorOnly, bool bForceSynchronousScan)
{
	return ScanPathsForPrimaryAssets(PrimaryAssetType, TArray<FString>{Path}, BaseClass, bHasBlueprintClasses, bIsEditorOnly, bForceSynchronousScan);
}

bool UAssetManager::AddDynamicAsset(const FPrimaryAssetId& PrimaryAssetId, const FSoftObjectPath& AssetPath, const FAssetBundleData& BundleData)
{
	if (!ensure(PrimaryAssetId.IsValid()))
	{
		return false;
	}

	if (!ensure(AssetPath.IsNull() || AssetPath.IsAsset()))
	{
		return false;
	}

	FPrimaryAssetType PrimaryAssetType = PrimaryAssetId.PrimaryAssetType;
	TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(PrimaryAssetType);

	if (!FoundType)
	{
		TSharedPtr<FPrimaryAssetTypeData> NewAsset = MakeShareable(new FPrimaryAssetTypeData());
		NewAsset->Info.PrimaryAssetType = PrimaryAssetType;
		NewAsset->Info.bIsDynamicAsset = true;

		FoundType = &AssetTypeMap.Add(PrimaryAssetType, NewAsset.ToSharedRef());
	}

	// Should always be valid
	check(FoundType);

	FPrimaryAssetTypeData& TypeData = FoundType->Get();

	// This needs to be a dynamic type, types cannot be both dynamic and loaded off disk
	if (!ensure(TypeData.Info.bIsDynamicAsset))
	{
		return false;
	}

	FPrimaryAssetData* OldData = TypeData.AssetMap.Find(PrimaryAssetId.PrimaryAssetName);
	FPrimaryAssetData& NameData = TypeData.AssetMap.FindOrAdd(PrimaryAssetId.PrimaryAssetName);

	if (OldData && OldData->AssetPtr.ToSoftObjectPath() != AssetPath)
	{
		UE_LOG(LogAssetManager, Warning, TEXT("AddDynamicAsset on %s called with conflicting path. Path %s is replacing path %s"), *PrimaryAssetId.ToString(), *OldData->AssetPtr.ToString(), *AssetPath.ToString());
	}

	NameData.AssetPtr = FSoftObjectPtr(AssetPath);

	if (bIsBulkScanning && AssetPath.IsValid())
	{
		// Do a partial update, add to the path->asset map
		AssetPathMap.Add(AssetPath.GetAssetPathName(), PrimaryAssetId);
	}

	if (OldData)
	{
		CachedAssetBundles.Remove(PrimaryAssetId);
	}

	TMap<FName, FAssetBundleEntry>& BundleMap = CachedAssetBundles.FindOrAdd(PrimaryAssetId);

	for (const FAssetBundleEntry& Entry : BundleData.Bundles)
	{
		FAssetBundleEntry NewEntry(Entry);
		NewEntry.BundleScope = PrimaryAssetId;

		BundleMap.Emplace(Entry.BundleName, NewEntry);
	}
	return true;
}

void UAssetManager::RecursivelyExpandBundleData(FAssetBundleData& BundleData)
{
	TArray<FSoftObjectPath> ReferencesToExpand;
	TSet<FName> FoundBundleNames;

	for (const FAssetBundleEntry& Entry : BundleData.Bundles)
	{
		FoundBundleNames.Add(Entry.BundleName);
		for (const FSoftObjectPath& Reference : Entry.BundleAssets)
		{
			ReferencesToExpand.AddUnique(Reference);
		}
	}

	// Expandable references can increase recursively
	for (int32 i = 0; i < ReferencesToExpand.Num(); i++)
	{
		FPrimaryAssetId FoundId = GetPrimaryAssetIdForPath(ReferencesToExpand[i]);
		TArray<FAssetBundleEntry> FoundEntries;

		if (FoundId.IsValid() && GetAssetBundleEntries(FoundId, FoundEntries))
		{
			for (const FAssetBundleEntry& FoundEntry : FoundEntries)
			{
				// Make sure the bundle name matches
				if (FoundBundleNames.Contains(FoundEntry.BundleName))
				{
					BundleData.AddBundleAssets(FoundEntry.BundleName, FoundEntry.BundleAssets);

					for (const FSoftObjectPath& FoundReference : FoundEntry.BundleAssets)
					{
						// Keep recursing
						ReferencesToExpand.AddUnique(FoundReference);
					}
				}
			}
		}
	}
}

void UAssetManager::SetPrimaryAssetTypeRules(FPrimaryAssetType PrimaryAssetType, const FPrimaryAssetRules& Rules)
{
	// Can't set until it's been scanned at least once
	TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(PrimaryAssetType);

	if (ensure(FoundType))
	{
		(*FoundType)->Info.Rules = Rules;
	}
}

void UAssetManager::SetPrimaryAssetRules(FPrimaryAssetId PrimaryAssetId, const FPrimaryAssetRules& Rules)
{
	if (Rules.IsDefault())
	{
		AssetRuleOverrides.Remove(PrimaryAssetId);
	}
	else
	{
		if (!GIsEditor && AssetRuleOverrides.Find(PrimaryAssetId))
		{
			UE_LOG(LogAssetManager, Error, TEXT("Duplicate Rule overrides found for asset %s!"), *PrimaryAssetId.ToString());
		}

		AssetRuleOverrides.Add(PrimaryAssetId, Rules);
	}

	bIsManagementDatabaseCurrent = false;
}

FPrimaryAssetRules UAssetManager::GetPrimaryAssetRules(FPrimaryAssetId PrimaryAssetId) const
{
	FPrimaryAssetRules Result;

	// Allow setting management rules before scanning
	const TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(PrimaryAssetId.PrimaryAssetType);

	if (FoundType)
	{
		Result = (*FoundType)->Info.Rules;

		// Selectively override
		const FPrimaryAssetRules* FoundRules = AssetRuleOverrides.Find(PrimaryAssetId);

		if (FoundRules)
		{
			Result.OverrideRules(*FoundRules);
		}

		if (Result.Priority < 0)
		{
			// Make sure it's at least 1
			Result.Priority = 1;
		}
	}

	return Result;
}

bool UAssetManager::GetPrimaryAssetData(const FPrimaryAssetId& PrimaryAssetId, FAssetData& AssetData) const
{
	const FPrimaryAssetData* NameData = GetNameData(PrimaryAssetId);

	if (NameData)
	{
		const FAssetData* CachedAssetData = GetAssetRegistry().GetCachedAssetDataForObjectPath(NameData->AssetDataPath);

		if (CachedAssetData && CachedAssetData->IsValid())
		{
			AssetData = *CachedAssetData;
			return true;
		}
	}
	return false;
}

bool UAssetManager::GetPrimaryAssetDataList(FPrimaryAssetType PrimaryAssetType, TArray<FAssetData>& AssetDataList) const
{
	IAssetRegistry& Registry = GetAssetRegistry();
	bool bAdded = false;
	const TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(PrimaryAssetType);

	if (FoundType)
	{
		const FPrimaryAssetTypeData& TypeData = FoundType->Get();

		for (const TPair<FName, FPrimaryAssetData>& Pair : TypeData.AssetMap)
		{
			const FAssetData* CachedAssetData = Registry.GetCachedAssetDataForObjectPath(Pair.Value.AssetDataPath);

			if (CachedAssetData && CachedAssetData->IsValid())
			{
				bAdded = true;
				AssetDataList.Add(*CachedAssetData);
			}
		}
	}

	return bAdded;
}

UObject* UAssetManager::GetPrimaryAssetObject(const FPrimaryAssetId& PrimaryAssetId) const
{
	const FPrimaryAssetData* NameData = GetNameData(PrimaryAssetId);

	if (NameData)
	{
		return NameData->AssetPtr.Get();
	}

	return nullptr;
}

bool UAssetManager::GetPrimaryAssetObjectList(FPrimaryAssetType PrimaryAssetType, TArray<UObject*>& ObjectList) const
{
	bool bAdded = false;
	const TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(PrimaryAssetType);

	if (FoundType)
	{
		const FPrimaryAssetTypeData& TypeData = FoundType->Get();

		for (const TPair<FName, FPrimaryAssetData>& Pair : TypeData.AssetMap)
		{
			UObject* FoundObject = Pair.Value.AssetPtr.Get();

			if (FoundObject)
			{
				ObjectList.Add(FoundObject);
				bAdded = true;
			}
		}
	}

	return bAdded;
}

FSoftObjectPath UAssetManager::GetPrimaryAssetPath(const FPrimaryAssetId& PrimaryAssetId) const
{
	const FPrimaryAssetData* NameData = GetNameData(PrimaryAssetId);

	if (NameData)
	{
		return NameData->AssetPtr.ToSoftObjectPath();
	}
	return FSoftObjectPath();
}

bool UAssetManager::GetPrimaryAssetPathList(FPrimaryAssetType PrimaryAssetType, TArray<FSoftObjectPath>& AssetPathList) const
{
	const TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(PrimaryAssetType);

	if (FoundType)
	{
		const FPrimaryAssetTypeData& TypeData = FoundType->Get();

		for (const TPair<FName, FPrimaryAssetData>& Pair : TypeData.AssetMap)
		{
			if (!Pair.Value.AssetPtr.IsNull())
			{
				AssetPathList.AddUnique(Pair.Value.AssetPtr.ToSoftObjectPath());
			}
		}
	}

	return AssetPathList.Num() > 0;
}

FPrimaryAssetId UAssetManager::GetPrimaryAssetIdForObject(UObject* Object) const
{
	// Use path instead of calling on Object, we only want it if it's registered
	return GetPrimaryAssetIdForPath(FName(*Object->GetPathName()));
}

FPrimaryAssetId UAssetManager::GetPrimaryAssetIdForData(const FAssetData& AssetData) const
{
	return GetPrimaryAssetIdForPath(GetAssetPathForData(AssetData));
}

FPrimaryAssetId UAssetManager::GetPrimaryAssetIdForPath(const FSoftObjectPath& ObjectPath) const
{
	return GetPrimaryAssetIdForPath(ObjectPath.GetAssetPathName());
}

FPrimaryAssetId UAssetManager::GetPrimaryAssetIdForPath(FName ObjectPath) const
{
	const FPrimaryAssetId* FoundIdentifier = AssetPathMap.Find(ObjectPath);

	// Check redirector list
	if (!FoundIdentifier)
	{
		FName RedirectedPath = GetRedirectedAssetPath(ObjectPath);

		if (RedirectedPath != NAME_None)
		{
			FoundIdentifier = AssetPathMap.Find(RedirectedPath);
		}
	}

	if (FoundIdentifier)
	{
		return *FoundIdentifier;
	}

	return FPrimaryAssetId();
}

FPrimaryAssetId UAssetManager::GetPrimaryAssetIdForPackage(FName PackagePath) const
{
	FString PackageString = PackagePath.ToString();
	FString AssetName = FPackageName::GetShortName(PackageString);

	FPrimaryAssetId FoundId;
	FName PossibleAssetPath = FName(*FString::Printf(TEXT("%s.%s"), *PackageString, *AssetName), FNAME_Find);

	// Try without _C first
	if (PossibleAssetPath != NAME_None)
	{
		FoundId = GetPrimaryAssetIdForPath(PossibleAssetPath);

		if (FoundId.IsValid())
		{
			return FoundId;
		}
	}

	// Then try _C
	PossibleAssetPath = FName(*FString::Printf(TEXT("%s.%s_C"), *PackageString, *AssetName), FNAME_Find);

	if (PossibleAssetPath != NAME_None)
	{
		FoundId = GetPrimaryAssetIdForPath(PossibleAssetPath);
	}

	return FoundId;
}

FPrimaryAssetId UAssetManager::ExtractPrimaryAssetIdFromData(const FAssetData& AssetData, FPrimaryAssetType SuggestedType) const
{
	FPrimaryAssetId FoundId = AssetData.GetPrimaryAssetId();

	if (!FoundId.IsValid() && bShouldGuessTypeAndName && SuggestedType != NAME_None)
	{
		const TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(SuggestedType);

		if (ensure(FoundType))
		{
			// If asset at this path is already known about return that
			FPrimaryAssetId OldID = GetPrimaryAssetIdForPath(GetAssetPathForData(AssetData));

			if (OldID.IsValid())
			{
				return OldID;
			}

			return FPrimaryAssetId(SuggestedType, SuggestedType == MapType ? AssetData.PackageName : AssetData.AssetName);
		}
	}

	return FoundId;
}

bool UAssetManager::GetPrimaryAssetIdList(FPrimaryAssetType PrimaryAssetType, TArray<FPrimaryAssetId>& PrimaryAssetIdList) const
{
	const TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(PrimaryAssetType);

	if (FoundType)
	{
		const FPrimaryAssetTypeData& TypeData = FoundType->Get();

		for (const TPair<FName, FPrimaryAssetData>& Pair : TypeData.AssetMap)
		{
			PrimaryAssetIdList.Add(FPrimaryAssetId(PrimaryAssetType, Pair.Key));
		}
	}

	return PrimaryAssetIdList.Num() > 0;
}

bool UAssetManager::GetPrimaryAssetTypeInfo(FPrimaryAssetType PrimaryAssetType, FPrimaryAssetTypeInfo& AssetTypeInfo) const
{
	const TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(PrimaryAssetType);

	if (FoundType)
	{
		AssetTypeInfo = (*FoundType)->Info;

		return true;
	}

	return false;
}

void UAssetManager::GetPrimaryAssetTypeInfoList(TArray<FPrimaryAssetTypeInfo>& AssetTypeInfoList) const
{
	for (const TPair<FName, TSharedRef<FPrimaryAssetTypeData>>& TypePair : AssetTypeMap)
	{
		const FPrimaryAssetTypeData& TypeData = TypePair.Value.Get();

		AssetTypeInfoList.Add(TypeData.Info);
	}
}

TSharedPtr<FStreamableHandle> UAssetManager::ChangeBundleStateForPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToChange, const TArray<FName>& AddBundles, const TArray<FName>& RemoveBundles, bool bRemoveAllBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority)
{
	TArray<TSharedPtr<FStreamableHandle> > NewHandles, ExistingHandles;
	TArray<FPrimaryAssetId> NewAssets;
	TSharedPtr<FStreamableHandle> ReturnHandle;

	for (const FPrimaryAssetId& PrimaryAssetId : AssetsToChange)
	{
		FPrimaryAssetData* NameData = GetNameData(PrimaryAssetId);

		if (NameData)
		{
			// Iterate list of changes, compute new bundle set
			bool bLoadIfNeeded = false;
			
			// Use pending state if valid
			TArray<FName> CurrentBundleState = NameData->PendingState.IsValid() ? NameData->PendingState.BundleNames : NameData->CurrentState.BundleNames;
			TArray<FName> NewBundleState;

			if (!bRemoveAllBundles)
			{
				NewBundleState = CurrentBundleState;

				for (const FName& RemoveBundle : RemoveBundles)
				{
					NewBundleState.Remove(RemoveBundle);
				}
			}

			for (const FName& AddBundle : AddBundles)
			{
				NewBundleState.AddUnique(AddBundle);
			}

			NewBundleState.Sort();

			// If the pending state is valid, check if it is different
			if (NameData->PendingState.IsValid())
			{
				if (NameData->PendingState.BundleNames == NewBundleState)
				{
					// This will wait on any existing handles to finish
					ExistingHandles.Add(NameData->PendingState.Handle);
					continue;
				}
				// Clear pending state
				NameData->PendingState.Reset(true);
			}
			else if (NameData->CurrentState.IsValid() && NameData->CurrentState.BundleNames == NewBundleState)
			{
				// If no pending, compare with current
				continue;
			}

			TSet<FSoftObjectPath> PathsToLoad;

			// Gather asset refs
			const FSoftObjectPath& AssetPath = NameData->AssetPtr.ToSoftObjectPath();

			if (!AssetPath.IsNull())
			{
				// Dynamic types can have no base asset path
				PathsToLoad.Add(AssetPath);
			}
			
			for (const FName& BundleName : NewBundleState)
			{
				FAssetBundleEntry Entry = GetAssetBundleEntry(PrimaryAssetId, BundleName);

				if (Entry.IsValid())
				{
					PathsToLoad.Append(Entry.BundleAssets);
				}
				else
				{
					UE_LOG(LogAssetManager, Verbose, TEXT("ChangeBundleStateForPrimaryAssets: No assets for bundle %s::%s"), *PrimaryAssetId.ToString(), *BundleName.ToString());
				}
			}

			TSharedPtr<FStreamableHandle> NewHandle;

			FString DebugName = PrimaryAssetId.ToString();

			if (NewBundleState.Num() > 0)
			{
				DebugName += TEXT(" (");

				for (int32 i = 0; i < NewBundleState.Num(); i++)
				{
					if (i != 0)
					{
						DebugName += TEXT(", ");
					}
					DebugName += NewBundleState[i].ToString();
				}

				DebugName += TEXT(")");
			}

			NewHandle = LoadAssetList(PathsToLoad.Array(), FStreamableDelegate(), Priority, DebugName);

			if (!ensureMsgf(NewHandle.IsValid(), TEXT("Requested load of Primary Asset with no referenced assets!")))
			{
				return nullptr;
			}

			if (NewHandle->HasLoadCompleted())
			{
				// Copy right into active
				NameData->CurrentState.BundleNames = NewBundleState;
				NameData->CurrentState.Handle = NewHandle;
			}
			else
			{
				// Copy into pending and set delegate
				NameData->PendingState.BundleNames = NewBundleState;
				NameData->PendingState.Handle = NewHandle;

				NewHandle->BindCompleteDelegate(FStreamableDelegate::CreateUObject(this, &UAssetManager::OnAssetStateChangeCompleted, PrimaryAssetId, NewHandle, FStreamableDelegate()));
			}

			NewHandles.Add(NewHandle);
			NewAssets.Add(PrimaryAssetId);
		}
	}

	if (NewHandles.Num() > 1 || ExistingHandles.Num() > 0)
	{
		// If multiple handles or we have an old handle, need to make wrapper handle
		NewHandles.Append(ExistingHandles);

		ReturnHandle = StreamableManager.CreateCombinedHandle(NewHandles, FString::Printf(TEXT("%s CreateCombinedHandle"), *GetName()));

		// Call delegate or bind to meta handle
		if (ReturnHandle->HasLoadCompleted())
		{
			FStreamableHandle::ExecuteDelegate(DelegateToCall);
		}
		else
		{
			// Call external callback when completed
			ReturnHandle->BindCompleteDelegate(DelegateToCall);
		}
	}
	else if (NewHandles.Num() == 1)
	{
		ReturnHandle = NewHandles[0];
		ensure(NewAssets.Num() == 1);

		// If only one handle, return it and add callback
		if (ReturnHandle->HasLoadCompleted())
		{
			FStreamableHandle::ExecuteDelegate(DelegateToCall);
		}
		else
		{
			// Call internal callback and external callback when it finishes
			ReturnHandle->BindCompleteDelegate(FStreamableDelegate::CreateUObject(this, &UAssetManager::OnAssetStateChangeCompleted, NewAssets[0], ReturnHandle, DelegateToCall));
		}
	}
	else
	{
		// Call completion callback, nothing to do
		FStreamableHandle::ExecuteDelegate(DelegateToCall);
	}

	return ReturnHandle;
}

TSharedPtr<FStreamableHandle> UAssetManager::ChangeBundleStateForMatchingPrimaryAssets(const TArray<FName>& NewBundles, const TArray<FName>& OldBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority)
{
	TArray<FPrimaryAssetId> AssetsToChange;

	if (GetPrimaryAssetsWithBundleState(AssetsToChange, TArray<FPrimaryAssetType>(), OldBundles))
	{
		// This will call delegate when done
		return ChangeBundleStateForPrimaryAssets(AssetsToChange, NewBundles, OldBundles, false, DelegateToCall, Priority);
	}

	// Nothing to transition, call delegate now
	DelegateToCall.ExecuteIfBound();
	return nullptr;
}

TSharedPtr<FStreamableHandle> UAssetManager::PreloadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToLoad, const TArray<FName>& LoadBundles, bool bLoadRecursive, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority)
{
	TSet<FSoftObjectPath> PathsToLoad;
	FString DebugName;
	TSharedPtr<FStreamableHandle> ReturnHandle;

	for (const FPrimaryAssetId& PrimaryAssetId : AssetsToLoad)
	{
		FPrimaryAssetData* NameData = GetNameData(PrimaryAssetId);

		if (NameData)
		{
			// Gather asset refs
			const FSoftObjectPath& AssetPath = NameData->AssetPtr.ToSoftObjectPath();

			if (!AssetPath.IsNull())
			{
				// Dynamic types can have no base asset path
				PathsToLoad.Add(AssetPath);
			}

			// Construct a temporary bundle data with the bundles specified
			FAssetBundleData TempBundleData;
			for (const FName& BundleName : LoadBundles)
			{
				FAssetBundleEntry Entry = GetAssetBundleEntry(PrimaryAssetId, BundleName);

				if (Entry.IsValid())
				{
					TempBundleData.Bundles.Add(Entry);
				}
			}

			if (bLoadRecursive)
			{
				RecursivelyExpandBundleData(TempBundleData);
			}

			for (const FAssetBundleEntry& Entry : TempBundleData.Bundles)
			{
				PathsToLoad.Append(Entry.BundleAssets);
			}

			if (DebugName.IsEmpty())
			{
				DebugName += TEXT("Preloading ");
			}
			else
			{
				DebugName += TEXT(", ");
			}
			DebugName += PrimaryAssetId.ToString();
		}
	}

	ReturnHandle = LoadAssetList(PathsToLoad.Array(), DelegateToCall, Priority, DebugName);

	if (!ensureMsgf(ReturnHandle.IsValid(), TEXT("Requested preload of Primary Asset with no referenced assets!")))
	{
		return nullptr;
	}

	return ReturnHandle;
}

void UAssetManager::OnAssetStateChangeCompleted(FPrimaryAssetId PrimaryAssetId, TSharedPtr<FStreamableHandle> BoundHandle, FStreamableDelegate WrappedDelegate)
{
	FPrimaryAssetData* NameData = GetNameData(PrimaryAssetId);

	if (NameData)
	{
		if (NameData->PendingState.Handle == BoundHandle)
		{
			NameData->CurrentState.Handle = NameData->PendingState.Handle;
			NameData->CurrentState.BundleNames = NameData->PendingState.BundleNames;

			WrappedDelegate.ExecuteIfBound();

			// Clear old state, but don't cancel handle as we just copied it into current
			NameData->PendingState.Reset(false);
		}
		else
		{
			UE_LOG(LogAssetManager, Verbose, TEXT("OnAssetStateChangeCompleted: Received after pending data changed, ignoring (%s)"), *PrimaryAssetId.ToString());
		}
	}
	else
	{
		UE_LOG(LogAssetManager, Error, TEXT("OnAssetStateChangeCompleted: Received for invalid asset! (%s)"), *PrimaryAssetId.ToString());
	}
}

TSharedPtr<FStreamableHandle> UAssetManager::LoadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority)
{
	return ChangeBundleStateForPrimaryAssets(AssetsToLoad, LoadBundles, TArray<FName>(), true, DelegateToCall, Priority);
}

TSharedPtr<FStreamableHandle> UAssetManager::LoadPrimaryAsset(const FPrimaryAssetId& AssetToLoad, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority)
{
	return LoadPrimaryAssets(TArray<FPrimaryAssetId>{AssetToLoad}, LoadBundles, DelegateToCall, Priority);
}

TSharedPtr<FStreamableHandle> UAssetManager::LoadPrimaryAssetsWithType(FPrimaryAssetType PrimaryAssetType, const TArray<FName>& LoadBundles, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority)
{
	TArray<FPrimaryAssetId> Assets;
	GetPrimaryAssetIdList(PrimaryAssetType, Assets);
	return LoadPrimaryAssets(Assets, LoadBundles, DelegateToCall, Priority);
}

TSharedPtr<FStreamableHandle> UAssetManager::GetPrimaryAssetHandle(const FPrimaryAssetId& PrimaryAssetId, bool bForceCurrent, TArray<FName>* Bundles) const
{
	const FPrimaryAssetData* NameData = GetNameData(PrimaryAssetId);

	if (!NameData)
	{
		return nullptr;
	}

	const FPrimaryAssetLoadState& LoadState = (bForceCurrent || !NameData->PendingState.IsValid()) ? NameData->CurrentState : NameData->PendingState;

	if (Bundles)
	{
		*Bundles = LoadState.BundleNames;
	}
	return LoadState.Handle;
}

bool UAssetManager::GetPrimaryAssetsWithBundleState(TArray<FPrimaryAssetId>& PrimaryAssetList, const TArray<FPrimaryAssetType>& ValidTypes, const TArray<FName>& RequiredBundles, const TArray<FName>& ExcludedBundles, bool bForceCurrent) const
{
	bool bFoundAny = false;

	for (const TPair<FName, TSharedRef<FPrimaryAssetTypeData>>& TypePair : AssetTypeMap)
	{
		if (ValidTypes.Num() > 0 && !ValidTypes.Contains(FPrimaryAssetType(TypePair.Key)))
		{
			// Skip this type
			continue;
		}

		const FPrimaryAssetTypeData& TypeData = TypePair.Value.Get();

		for (const TPair<FName, FPrimaryAssetData>& NamePair : TypeData.AssetMap)
		{
			const FPrimaryAssetData& NameData = NamePair.Value;

			const FPrimaryAssetLoadState& LoadState = (bForceCurrent || !NameData.PendingState.IsValid()) ? NameData.CurrentState : NameData.PendingState;

			if (!LoadState.IsValid())
			{
				// Only allow loaded assets
				continue;
			}

			bool bFailedTest = false;

			// Check bundle requirements
			for (const FName& RequiredName : RequiredBundles)
			{
				if (!LoadState.BundleNames.Contains(RequiredName))
				{
					bFailedTest = true;
					break;
				}
			}

			for (const FName& ExcludedName : ExcludedBundles)
			{
				if (LoadState.BundleNames.Contains(ExcludedName))
				{
					bFailedTest = true;
					break;
				}
			}

			if (!bFailedTest)
			{
				PrimaryAssetList.Add(FPrimaryAssetId(TypePair.Key, NamePair.Key));
				bFoundAny = true;
			}
		}
	}

	return bFoundAny;
}

void UAssetManager::GetPrimaryAssetBundleStateMap(TMap<FPrimaryAssetId, TArray<FName>>& BundleStateMap, bool bForceCurrent) const
{
	BundleStateMap.Reset();

	for (const TPair<FName, TSharedRef<FPrimaryAssetTypeData>>& TypePair : AssetTypeMap)
	{
		const FPrimaryAssetTypeData& TypeData = TypePair.Value.Get();

		for (const TPair<FName, FPrimaryAssetData>& NamePair : TypeData.AssetMap)
		{
			const FPrimaryAssetData& NameData = NamePair.Value;

			const FPrimaryAssetLoadState& LoadState = (bForceCurrent || !NameData.PendingState.IsValid()) ? NameData.CurrentState : NameData.PendingState;

			if (!LoadState.IsValid())
			{
				continue;
			}

			FPrimaryAssetId AssetID(TypePair.Key, NamePair.Key);

			BundleStateMap.Add(AssetID, LoadState.BundleNames);
		}
	}
}

int32 UAssetManager::UnloadPrimaryAssets(const TArray<FPrimaryAssetId>& AssetsToUnload)
{
	int32 NumUnloaded = 0;

	for (const FPrimaryAssetId& PrimaryAssetId : AssetsToUnload)
	{
		FPrimaryAssetData* NameData = GetNameData(PrimaryAssetId);

		if (NameData)
		{
			// Undo current and pending
			if (NameData->CurrentState.IsValid() || NameData->PendingState.IsValid())
			{
				NumUnloaded++;
				NameData->CurrentState.Reset(true);
				NameData->PendingState.Reset(true);
			}
		}
	}

	return NumUnloaded;
}

int32 UAssetManager::UnloadPrimaryAsset(const FPrimaryAssetId& AssetToUnload)
{
	return UnloadPrimaryAssets(TArray<FPrimaryAssetId>{AssetToUnload});
}

int32 UAssetManager::UnloadPrimaryAssetsWithType(FPrimaryAssetType PrimaryAssetType)
{
	TArray<FPrimaryAssetId> Assets;
	GetPrimaryAssetIdList(PrimaryAssetType, Assets);
	return UnloadPrimaryAssets(Assets);
}

TSharedPtr<FStreamableHandle> UAssetManager::LoadAssetList(const TArray<FSoftObjectPath>& AssetList, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority, const FString& DebugName)
{
	TSharedPtr<FStreamableHandle> NewHandle;
	TArray<int32> MissingChunks, ErrorChunks;

	if (bShouldAcquireMissingChunksOnLoad)
	{
		FindMissingChunkList(AssetList, MissingChunks, ErrorChunks);

		if (ErrorChunks.Num() > 0)
		{
			// At least one chunk doesn't exist, fail
			UE_LOG(LogAssetManager, Error, TEXT("Failure loading %s, Required chunk %d does not exist!"), *DebugName, ErrorChunks[0]);
			return nullptr;
		}
	}

	// SynchronousLoad doesn't make sense if chunks are missing
	if (bShouldUseSynchronousLoad && MissingChunks.Num() > 0)
	{
		NewHandle = StreamableManager.RequestSyncLoad(AssetList, false, DebugName);
	}
	else
	{
		NewHandle = StreamableManager.RequestAsyncLoad(AssetList, DelegateToCall, Priority, false, MissingChunks.Num() > 0, DebugName);

		if (MissingChunks.Num() > 0 && NewHandle.IsValid())
		{
			AcquireChunkList(MissingChunks, FAssetManagerAcquireResourceDelegate(), EChunkPriority::Immediate, NewHandle);
		}
	}

	return NewHandle;
}

FAssetBundleEntry UAssetManager::GetAssetBundleEntry(const FPrimaryAssetId& BundleScope, FName BundleName) const
{
	FAssetBundleEntry InvalidEntry;

	const TMap<FName, FAssetBundleEntry>* FoundMap = CachedAssetBundles.Find(BundleScope);

	if (FoundMap)
	{
		const FAssetBundleEntry* FoundEntry = FoundMap->Find(BundleName);
		if (FoundEntry)
		{
			return *FoundEntry;
		}
	}
	return InvalidEntry;
}

bool UAssetManager::GetAssetBundleEntries(const FPrimaryAssetId& BundleScope, TArray<FAssetBundleEntry>& OutEntries) const
{
	bool bFoundAny = false;

	const TMap<FName, FAssetBundleEntry>* FoundMap = CachedAssetBundles.Find(BundleScope);

	if (FoundMap)
	{
		for (const TPair<FName, FAssetBundleEntry>& BundlePair : *FoundMap)
		{
			bFoundAny = true;

			OutEntries.Add(BundlePair.Value);
		}
	}
	return bFoundAny;
}

bool UAssetManager::FindMissingChunkList(const TArray<FSoftObjectPath>& AssetList, TArray<int32>& OutMissingChunkList, TArray<int32>& OutErrorChunkList) const
{
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();

	if (!ChunkInstall || !bIsLoadingFromPakFiles)
	{
		return false;
	}

	for (const FSoftObjectPath& Asset : AssetList)
	{
		FAssetData FoundData;
		GetAssetDataForPath(Asset, FoundData);
		
		TSet<int32> FoundChunks, MissingChunks, ErrorChunks;

		for (int32 ChunkId : FoundData.ChunkIDs)
		{
			EChunkLocation::Type ChunkLocation = ChunkInstall->GetChunkLocation(ChunkId);

			switch (ChunkLocation)
			{			
			case EChunkLocation::DoesNotExist:
				ErrorChunks.Add(ChunkId);
				break;
			case EChunkLocation::NotAvailable:
				MissingChunks.Add(ChunkId);
				break;
			case EChunkLocation::LocalSlow:
			case EChunkLocation::LocalFast:
				FoundChunks.Add(ChunkId);
				break;
			}
		}

		// Assets may be redundantly in multiple chunks, if we have any of the chunks then we have the asset
		if (FoundChunks.Num() == 0)
		{
			if (MissingChunks.Num() > 0)
			{
				int32 MissingChunkToAdd = -1;

				for (int32 MissingChunkId : MissingChunks)
				{
					if (OutMissingChunkList.Contains(MissingChunkId))
					{
						// This chunk is already scheduled, don't add a new one
						MissingChunkToAdd = -1;
						break;
					}
					else if (MissingChunkToAdd == -1)
					{
						// Add the first mentioned missing chunk
						MissingChunkToAdd = MissingChunkId;
					}
				}

				if (MissingChunkToAdd != -1)
				{
					OutMissingChunkList.Add(MissingChunkToAdd);
				}
			}
			else if (ErrorChunks.Num() > 0)
			{
				// Only have error chunks, report the errors
				for (int32 ErrorChunkId : ErrorChunks)
				{
					OutErrorChunkList.Add(ErrorChunkId);
				}
			}
		}
	}

	return OutMissingChunkList.Num() > 0 || OutErrorChunkList.Num() > 0;
}

void UAssetManager::AcquireChunkList(const TArray<int32>& ChunkList, FAssetManagerAcquireResourceDelegate CompleteDelegate, EChunkPriority::Type Priority, TSharedPtr<FStreamableHandle> StalledHandle)
{
	FPendingChunkInstall* PendingChunkInstall = new(PendingChunkInstalls) FPendingChunkInstall;
	PendingChunkInstall->ManualCallback = CompleteDelegate;
	PendingChunkInstall->RequestedChunks = ChunkList;
	PendingChunkInstall->PendingChunks = ChunkList;
	PendingChunkInstall->StalledStreamableHandle = StalledHandle;

	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();

	if (!ChunkInstallDelegateHandle.IsValid())
	{
		ChunkInstallDelegateHandle = ChunkInstall->AddChunkInstallDelegate(FPlatformChunkInstallDelegate::CreateUObject(this, &ThisClass::OnChunkDownloaded));
	}

	for (int32 MissingChunk : PendingChunkInstall->PendingChunks)
	{
		ChunkInstall->PrioritizeChunk(MissingChunk, Priority);
	}
}

void UAssetManager::AcquireResourcesForAssetList(const TArray<FSoftObjectPath>& AssetList, FAssetManagerAcquireResourceDelegate CompleteDelegate, EChunkPriority::Type Priority)
{
	TArray<int32> MissingChunks, ErrorChunks;

	FindMissingChunkList(AssetList, MissingChunks, ErrorChunks);

	if (ErrorChunks.Num() > 0)
	{
		// At least one chunk doesn't exist, fail
		FStreamableDelegate TempDelegate = FStreamableDelegate::CreateLambda([CompleteDelegate]() {CompleteDelegate.ExecuteIfBound(false); });
		FStreamableHandle::ExecuteDelegate(TempDelegate);

		return;
	}

	if (MissingChunks.Num() == 0)
	{
		// All here, schedule the callback
		FStreamableDelegate TempDelegate = FStreamableDelegate::CreateLambda([CompleteDelegate]() {CompleteDelegate.ExecuteIfBound(true); });
		FStreamableHandle::ExecuteDelegate(TempDelegate);

		return;
	}
	
	AcquireChunkList(MissingChunks, CompleteDelegate, Priority, nullptr);
}

void UAssetManager::AcquireResourcesForPrimaryAssetList(const TArray<FPrimaryAssetId>& PrimaryAssetList, FAssetManagerAcquireResourceDelegate CompleteDelegate, EChunkPriority::Type Priority)
{
	TSet<FSoftObjectPath> PathsToLoad;
	TSharedPtr<FStreamableHandle> ReturnHandle;

	for (const FPrimaryAssetId& PrimaryAssetId : PrimaryAssetList)
	{
		FPrimaryAssetData* NameData = GetNameData(PrimaryAssetId);

		if (NameData)
		{
			// Gather asset refs
			const FSoftObjectPath& AssetPath = NameData->AssetPtr.ToSoftObjectPath();

			if (!AssetPath.IsNull())
			{
				// Dynamic types can have no base asset path
				PathsToLoad.Add(AssetPath);
			}

			TArray<FAssetBundleEntry> BundleEntries;
			GetAssetBundleEntries(PrimaryAssetId, BundleEntries);
			for (const FAssetBundleEntry& Entry : BundleEntries)
			{
				if (Entry.IsValid())
				{
					PathsToLoad.Append(Entry.BundleAssets);
				}
			}
		}
	}

	AcquireResourcesForAssetList(PathsToLoad.Array(), CompleteDelegate, Priority);
}

bool UAssetManager::GetResourceAcquireProgress(int32& OutAcquiredCount, int32& OutRequestedCount) const
{
	OutAcquiredCount = OutRequestedCount = 0;
	// Iterate pending callbacks, in order they were added
	for (const FPendingChunkInstall& PendingChunkInstall : PendingChunkInstalls)
	{
		OutRequestedCount += PendingChunkInstall.RequestedChunks.Num();
		OutAcquiredCount += (PendingChunkInstall.RequestedChunks.Num() - PendingChunkInstall.PendingChunks.Num());
	}

	return PendingChunkInstalls.Num() > 0;
}

void UAssetManager::OnChunkDownloaded(uint32 ChunkId, bool bSuccess)
{
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();

	// Iterate pending callbacks, in order they were added
	for (int32 i = 0; i < PendingChunkInstalls.Num(); i++)
	{
		// Make a copy so if we resize the array it's safe
		FPendingChunkInstall PendingChunkInstall = PendingChunkInstalls[i];
		if (PendingChunkInstall.PendingChunks.Contains(ChunkId))
		{
			bool bFailed = !bSuccess;
			TArray<int32> NewPendingList;
			
			// Check all chunks if they are done or failed
			for (int32 PendingChunkId : PendingChunkInstall.PendingChunks)
			{
				EChunkLocation::Type ChunkLocation = ChunkInstall->GetChunkLocation(PendingChunkId);

				switch (ChunkLocation)
				{
				case EChunkLocation::DoesNotExist:
					bFailed = true;
					break;
				case EChunkLocation::NotAvailable:
					NewPendingList.Add(PendingChunkId);
					break;
				}
			}

			if (bFailed)
			{
				// Resize array first
				PendingChunkInstalls.RemoveAt(i);
				i--;

				if (PendingChunkInstall.StalledStreamableHandle.IsValid())
				{
					PendingChunkInstall.StalledStreamableHandle->CancelHandle();
				}

				PendingChunkInstall.ManualCallback.ExecuteIfBound(false);
			}
			else if (NewPendingList.Num() == 0)
			{
				// Resize array first
				PendingChunkInstalls.RemoveAt(i);
				i--;

				if (PendingChunkInstall.StalledStreamableHandle.IsValid())
				{
					PendingChunkInstall.StalledStreamableHandle->StartStalledHandle();
				}

				PendingChunkInstall.ManualCallback.ExecuteIfBound(true);
			}
			else
			{
				PendingChunkInstalls[i].PendingChunks = NewPendingList;
			}
		}
	}
}

FPrimaryAssetData* UAssetManager::GetNameData(const FPrimaryAssetId& PrimaryAssetId, bool bCheckRedirector)
{
	return const_cast<FPrimaryAssetData*>(const_cast<const UAssetManager*>(this)->GetNameData(PrimaryAssetId));
}

const FPrimaryAssetData* UAssetManager::GetNameData(const FPrimaryAssetId& PrimaryAssetId, bool bCheckRedirector) const
{
	const TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(PrimaryAssetId.PrimaryAssetType);

	// Try redirected name
	if (FoundType)
	{
		const FPrimaryAssetData* FoundName = (*FoundType)->AssetMap.Find(PrimaryAssetId.PrimaryAssetName);
		
		if (FoundName)
		{
			return FoundName;
		}
	}

	if (bCheckRedirector)
	{
		FPrimaryAssetId RedirectedId = GetRedirectedPrimaryAssetId(PrimaryAssetId);

		if (RedirectedId.IsValid())
		{
			// Recursively call self, but turn off recursion flag
			return GetNameData(RedirectedId, false);
		}
	}

	return nullptr;
}

void UAssetManager::RebuildObjectReferenceList()
{
	AssetPathMap.Reset();
	ObjectReferenceList.Reset();

	// Iterate primary asset map
	for (TPair<FName, TSharedRef<FPrimaryAssetTypeData>>& TypePair : AssetTypeMap)
	{
		FPrimaryAssetTypeData& TypeData = TypePair.Value.Get();

		// Add base class in case it's a blueprint
		if (!TypeData.Info.bIsDynamicAsset)
		{
			ObjectReferenceList.AddUnique(TypeData.Info.AssetBaseClassLoaded);
		}

		TypeData.Info.NumberOfAssets = TypeData.AssetMap.Num();

		for (TPair<FName, FPrimaryAssetData>& NamePair : TypeData.AssetMap)
		{
			FPrimaryAssetData& NameData = NamePair.Value;
			
			const FSoftObjectPath& AssetRef = NameData.AssetPtr.ToSoftObjectPath();

			// Dynamic types can have null asset refs
			if (!AssetRef.IsNull())
			{
				AssetPathMap.Add(AssetRef.GetAssetPathName(), FPrimaryAssetId(TypePair.Key, NamePair.Key));
			}
		}
	}

	bIsManagementDatabaseCurrent = false;
}

void UAssetManager::LoadRedirectorMaps()
{
	AssetPathRedirects.Reset();
	PrimaryAssetIdRedirects.Reset();
	PrimaryAssetTypeRedirects.Reset();

	const UAssetManagerSettings& Settings = GetSettings();

	for (const FAssetManagerRedirect& Redirect : Settings.PrimaryAssetTypeRedirects)
	{
		PrimaryAssetTypeRedirects.Add(FName(*Redirect.Old), FName(*Redirect.New));
	}

	for (const FAssetManagerRedirect& Redirect : Settings.PrimaryAssetIdRedirects)
	{
		PrimaryAssetIdRedirects.Add(Redirect.Old, Redirect.New);
	}

	for (const FAssetManagerRedirect& Redirect : Settings.AssetPathRedirects)
	{
		AssetPathRedirects.Add(FName(*Redirect.Old), FName(*Redirect.New));
	}
}

FPrimaryAssetId UAssetManager::GetRedirectedPrimaryAssetId(const FPrimaryAssetId& OldId) const
{
	FString OldIdString = OldId.ToString();

	const FString* FoundId = PrimaryAssetIdRedirects.Find(OldIdString);

	if (FoundId)
	{
		return FPrimaryAssetId(*FoundId);
	}

	// Now look for type redirect
	const FName* FoundType = PrimaryAssetTypeRedirects.Find(OldId.PrimaryAssetType);

	if (FoundType)
	{
		return FPrimaryAssetId(*FoundType, OldId.PrimaryAssetName);
	}

	return FPrimaryAssetId();
}

void UAssetManager::GetPreviousPrimaryAssetIds(const FPrimaryAssetId& NewId, TArray<FPrimaryAssetId>& OutOldIds) const
{
	FString NewIdString = NewId.ToString();

	for (const TPair<FString, FString>& Redirect : PrimaryAssetIdRedirects)
	{
		if (Redirect.Value == NewIdString)
		{
			OutOldIds.AddUnique(FPrimaryAssetId(Redirect.Key));
		}
	}

	// Also look for type redirects
	for (const TPair<FName, FName>& Redirect : PrimaryAssetTypeRedirects)
	{
		if (Redirect.Value == NewId.PrimaryAssetType)
		{
			OutOldIds.AddUnique(FPrimaryAssetId(Redirect.Key, NewId.PrimaryAssetName));
		}
	}
}

FName UAssetManager::GetRedirectedAssetPath(FName OldPath) const
{
	const FName* FoundPath = AssetPathRedirects.Find(OldPath);

	if (FoundPath)
	{
		return *FoundPath;
	}

	return NAME_None;
}

FSoftObjectPath UAssetManager::GetRedirectedAssetPath(const FSoftObjectPath& ObjectPath) const
{
	FName PossibleAssetPath = ObjectPath.GetAssetPathName();

	if (PossibleAssetPath == NAME_None)
	{
		return FSoftObjectPath();
	}

	FName RedirectedName = GetRedirectedAssetPath(PossibleAssetPath);

	if (RedirectedName == NAME_None)
	{
		return FSoftObjectPath();
	}
	return FSoftObjectPath(RedirectedName, ObjectPath.GetSubPathString());
}

void UAssetManager::ExtractSoftObjectPaths(const UStruct* Struct, const void* StructValue, TArray<FSoftObjectPath>& FoundAssetReferences, const TArray<FName>& PropertiesToSkip) const
{
	if (!ensure(Struct && StructValue))
	{
		return;
	}

	for (TPropertyValueIterator<const UProperty> It(Struct, StructValue); It; ++It)
	{
		const UProperty* Property = It.Key();
		const void* PropertyValue = It.Value();
		
		if (PropertiesToSkip.Contains(Property->GetFName()))
		{
			It.SkipRecursiveProperty();
			continue;
		}

		FSoftObjectPath FoundRef;
		if (const USoftClassProperty* AssetClassProp = Cast<USoftClassProperty>(Property))
		{
			const TSoftClassPtr<UObject>* AssetClassPtr = reinterpret_cast<const TSoftClassPtr<UObject>*>(PropertyValue);
			if (AssetClassPtr)
			{
				FoundRef = AssetClassPtr->ToSoftObjectPath();
			}
		}
		else if (const USoftObjectProperty* AssetProp = Cast<USoftObjectProperty>(Property))
		{
			const TSoftObjectPtr<UObject>* AssetPtr = reinterpret_cast<const TSoftObjectPtr<UObject>*>(PropertyValue);
			if (AssetPtr)
			{
				FoundRef = AssetPtr->ToSoftObjectPath();
			}
		}
		else if (const UStructProperty* StructProperty = Cast<UStructProperty>(Property))
		{
			// SoftClassPath is binary identical with SoftObjectPath
			if (StructProperty->Struct == TBaseStructure<FSoftObjectPath>::Get() || StructProperty->Struct == TBaseStructure<FSoftClassPath>::Get())
			{
				const FSoftObjectPath* AssetRefPtr = reinterpret_cast<const FSoftObjectPath*>(PropertyValue);
				if (AssetRefPtr)
				{
					FoundRef = *AssetRefPtr;
				}

				// Skip recursion, we don't care about the raw string property
				It.SkipRecursiveProperty();
			}
		}
		if (!FoundRef.IsNull())
		{
			FoundAssetReferences.AddUnique(FoundRef);
		}
	}
}

bool UAssetManager::GetAssetDataForPath(const FSoftObjectPath& ObjectPath, FAssetData& AssetData) const
{
	if (ObjectPath.IsNull())
	{
		return false;
	}

	IAssetRegistry& AssetRegistry = GetAssetRegistry();

	FString AssetPath = ObjectPath.ToString();

	// First check local redirector
	FSoftObjectPath RedirectedPath = GetRedirectedAssetPath(ObjectPath);

	if (RedirectedPath.IsValid())
	{
		AssetPath = RedirectedPath.ToString();
	}

	GetAssetDataForPathInternal(AssetRegistry, AssetPath, AssetData);

#if WITH_EDITOR
	// Cooked data has the asset data already set up. Uncooked builds may need to manually scan for this file
	if (!AssetData.IsValid())
	{
		ScanPathsSynchronous(TArray<FString>{AssetPath});

		GetAssetDataForPathInternal(AssetRegistry, AssetPath, AssetData);
	}

	// Handle redirector chains
	const FString* DestinationObjectStrPtr = AssetData.TagsAndValues.Find("DestinationObject");

	while (DestinationObjectStrPtr)
	{
		FString DestinationObjectPath = *DestinationObjectStrPtr;
		ConstructorHelpers::StripObjectClass(DestinationObjectPath);
		AssetData = AssetRegistry.GetAssetByObjectPath(*DestinationObjectPath);
		DestinationObjectStrPtr = AssetData.TagsAndValues.Find("DestinationObject");
	}
#endif

	return AssetData.IsValid();
}

FSoftObjectPath UAssetManager::GetAssetPathForData(const FAssetData& AssetData) const
{
	FString AssetPath = AssetData.IsValid() ? AssetData.ObjectPath.ToString() : FString();

	// All blueprint types end with blueprint as the class, there is no better test unfortunately
	if (AssetData.AssetClass.ToString().EndsWith(TEXT("Blueprint")))
	{
		AssetPath += TEXT("_C");
	}

	return FSoftObjectPath(AssetPath);
}

void UAssetManager::GetAssetDataForPathInternal(IAssetRegistry& AssetRegistry, const FString& AssetPath, OUT FAssetData& OutAssetData) const
{
	// We're a class if our path is foo.foo_C
	bool bIsClass = AssetPath.EndsWith(TEXT("_C"), ESearchCase::CaseSensitive) && !AssetPath.Contains(TEXT("_C."), ESearchCase::CaseSensitive);

	// If we're a class, first look for the asset data without the trailing _C
	// We do this first because in cooked builds you have to search the asset registry for the Blueprint, not the class itself
	if (bIsClass)
	{
		// We need to strip the class suffix because the asset registry has it listed by blueprint name
		
		OutAssetData = AssetRegistry.GetAssetByObjectPath(FName(*AssetPath.LeftChop(2)), bIncludeOnlyOnDiskAssets);

		if (OutAssetData.IsValid())
		{
			return;
		}
	}

	OutAssetData = AssetRegistry.GetAssetByObjectPath(FName(*AssetPath), bIncludeOnlyOnDiskAssets);
}

bool UAssetManager::WriteCustomReport(FString FileName, TArray<FString>& FileLines) const
{
	// Has a report been generated
	bool ReportGenerated = false;

	// Ensure we have a log to write
	if (FileLines.Num())
	{
		// Create the file name		
		FString FileLocation = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() + TEXT("Reports/"));
		FString FullPath = FString::Printf(TEXT("%s%s"), *FileLocation, *FileName);

		// save file
		FArchive* LogFile = IFileManager::Get().CreateFileWriter(*FullPath);

		if (LogFile != NULL)
		{
			for (int32 Index = 0; Index < FileLines.Num(); ++Index)
			{
				FString LogEntry = FString::Printf(TEXT("%s"), *FileLines[Index]) + LINE_TERMINATOR;
				LogFile->Serialize(TCHAR_TO_ANSI(*LogEntry), LogEntry.Len());
			}

			LogFile->Close();
			delete LogFile;

			// A report has been generated
			ReportGenerated = true;
		}
	}

	return ReportGenerated;
}

static FAutoConsoleCommand CVarDumpAssetTypeSummary(
	TEXT("AssetManager.DumpTypeSummary"),
	TEXT("Shows a summary of types known about by the asset manager"),
	FConsoleCommandDelegate::CreateStatic(UAssetManager::DumpAssetTypeSummary),
	ECVF_Cheat);

void UAssetManager::DumpAssetTypeSummary()
{
	if (!UAssetManager::IsValid())
	{
		return;
	}

	UAssetManager& Manager = Get();
	TArray<FPrimaryAssetTypeInfo> TypeInfos;

	Manager.GetPrimaryAssetTypeInfoList(TypeInfos);

	TypeInfos.Sort([](const FPrimaryAssetTypeInfo& LHS, const FPrimaryAssetTypeInfo& RHS) { return LHS.PrimaryAssetType < RHS.PrimaryAssetType; });

	UE_LOG(LogAssetManager, Log, TEXT("=========== Asset Manager Type Summary ==========="));

	for (const FPrimaryAssetTypeInfo& TypeInfo : TypeInfos)
	{
		UE_LOG(LogAssetManager, Log, TEXT("  %s: Class %s, Count %d, Paths %s"), *TypeInfo.PrimaryAssetType.ToString(), *TypeInfo.AssetBaseClassLoaded->GetName(), TypeInfo.NumberOfAssets, *FString::Join(TypeInfo.AssetScanPaths, TEXT(", ")));
	}
}

static FAutoConsoleCommand CVarDumpLoadedAssetState(
	TEXT("AssetManager.DumpLoadedAssets"),
	TEXT("Shows a list of all loaded primary assets and bundles"),
	FConsoleCommandDelegate::CreateStatic(UAssetManager::DumpLoadedAssetState),
	ECVF_Cheat);

void UAssetManager::DumpLoadedAssetState()
{
	if (!UAssetManager::IsValid())
	{
		return;
	}

	UAssetManager& Manager = Get();
	TArray<FPrimaryAssetTypeInfo> TypeInfos;

	Manager.GetPrimaryAssetTypeInfoList(TypeInfos);

	TypeInfos.Sort([](const FPrimaryAssetTypeInfo& LHS, const FPrimaryAssetTypeInfo& RHS) { return LHS.PrimaryAssetType < RHS.PrimaryAssetType; });

	UE_LOG(LogAssetManager, Log, TEXT("=========== Asset Manager Loaded Asset State ==========="));

	for (const FPrimaryAssetTypeInfo& TypeInfo : TypeInfos)
	{
		struct FLoadedInfo
		{
			FName AssetName;
			bool bPending;
			FString BundleState;

			FLoadedInfo(FName InAssetName, bool bInPending, const FString& InBundleState) : AssetName(InAssetName), bPending(bInPending), BundleState(InBundleState) {}
		};

		TArray<FLoadedInfo> LoadedInfos;

		FPrimaryAssetTypeData& TypeData = Manager.AssetTypeMap.Find(TypeInfo.PrimaryAssetType)->Get();

		for (const TPair<FName, FPrimaryAssetData>& NamePair : TypeData.AssetMap)
		{
			const FPrimaryAssetData& NameData = NamePair.Value;

			if (NameData.PendingState.IsValid() || NameData.CurrentState.IsValid())
			{
				const FPrimaryAssetLoadState& LoadState = (!NameData.PendingState.IsValid()) ? NameData.CurrentState : NameData.PendingState;

				FString BundleString;

				for (const FName& BundleName : LoadState.BundleNames)
				{
					if (!BundleString.IsEmpty())
					{
						BundleString += TEXT(", ");
					}
					BundleString += BundleName.ToString();
				}

				LoadedInfos.Emplace(NamePair.Key, NameData.PendingState.IsValid(), BundleString);
			}
		}

		if (LoadedInfos.Num() > 0)
		{
			UE_LOG(LogAssetManager, Log, TEXT("  Type %s:"), *TypeInfo.PrimaryAssetType.ToString());

			LoadedInfos.Sort([](const FLoadedInfo& LHS, const FLoadedInfo& RHS) { return LHS.AssetName < RHS.AssetName; });

			for (FLoadedInfo& LoadedInfo : LoadedInfos)
			{
				UE_LOG(LogAssetManager, Log, TEXT("    %s: %s, (%s)"), *LoadedInfo.AssetName.ToString(), LoadedInfo.bPending ? TEXT("pending load") : TEXT("loaded"), *LoadedInfo.BundleState);
			}
		}	
	}
}

static FAutoConsoleCommand CVarDumpAssetRegistryInfo(
	TEXT("AssetManager.DumpAssetRegistryInfo"),
	TEXT("Dumps extended info about asset registry to log"),
	FConsoleCommandDelegate::CreateStatic(UAssetManager::DumpAssetRegistryInfo),
	ECVF_Cheat);

void UAssetManager::DumpAssetRegistryInfo()
{
	UE_LOG(LogAssetManager, Log, TEXT("=========== Asset Registry Summary ==========="));
	UE_LOG(LogAssetManager, Log, TEXT("Current Registry Memory:"));

	UAssetManager& Manager = Get();

	// Output sizes
	Manager.GetAssetRegistry().GetAllocatedSize(true);

#if WITH_EDITOR
	UE_LOG(LogAssetManager, Log, TEXT("Estimated Cooked Registry Memory:"));

	FAssetRegistryState State;
	FAssetRegistrySerializationOptions SaveOptions;

	Manager.GetAssetRegistry().InitializeSerializationOptions(SaveOptions);
	Manager.GetAssetRegistry().InitializeTemporaryAssetRegistryState(State, SaveOptions);

	State.GetAllocatedSize(true);
#endif
}

static FAutoConsoleCommand CVarDumpReferencersForPackage(
	TEXT("AssetManager.DumpReferencersForPackage"),
	TEXT("Generates a graph viz and log file of all references to a specified package"),
	FConsoleCommandWithArgsDelegate::CreateStatic(UAssetManager::DumpReferencersForPackage),
	ECVF_Cheat);

void UAssetManager::DumpReferencersForPackage(const TArray< FString >& PackageNames)
{
	if (!UAssetManager::IsValid() || PackageNames.Num() == 0)
	{
		return;
	}

	UAssetManager& Manager = Get();
	IAssetRegistry& AssetRegistry = Manager.GetAssetRegistry();

	TArray<FString> ReportLines;

	ReportLines.Add(TEXT("digraph { "));

	for (const FString& PackageString : PackageNames)
	{
		TArray<FAssetIdentifier> FoundReferencers;

		AssetRegistry.GetReferencers(FName(*PackageString), FoundReferencers, EAssetRegistryDependencyType::Packages);

		for (const FAssetIdentifier& Identifier : FoundReferencers)
		{
			FString ReferenceString = Identifier.ToString();

			ReportLines.Add(FString::Printf(TEXT("\t\"%s\" -> \"%s\";"), *ReferenceString, *PackageString));

			UE_LOG(LogAssetManager, Log, TEXT("%s depends on %s"), *ReferenceString, *PackageString);
		}
	}

	ReportLines.Add(TEXT("}"));

	Manager.WriteCustomReport(FString::Printf(TEXT("ReferencersForPackage%s%s.gv"), *PackageNames[0], *FDateTime::Now().ToString()), ReportLines);
}

bool UAssetManager::ShouldScanPrimaryAssetType(FPrimaryAssetTypeInfo& TypeInfo) const
{
	if (TypeInfo.bIsEditorOnly && !GIsEditor)
	{
		return false;
	}

	if (!TypeInfo.FillRuntimeData())
	{
		return false;
	}

	return true;
}

void UAssetManager::ScanPrimaryAssetTypesFromConfig()
{
	IAssetRegistry& AssetRegistry = GetAssetRegistry();
	const UAssetManagerSettings& Settings = GetSettings();

	StartBulkScanning();

	for (FPrimaryAssetTypeInfo TypeInfo : Settings.PrimaryAssetTypesToScan)
	{
		// This function also fills out runtime data on the copy
		if (!ShouldScanPrimaryAssetType(TypeInfo))
		{
			continue;
		}

		ScanPathsForPrimaryAssets(TypeInfo.PrimaryAssetType, TypeInfo.AssetScanPaths, TypeInfo.AssetBaseClassLoaded, TypeInfo.bHasBlueprintClasses, TypeInfo.bIsEditorOnly, false);

		SetPrimaryAssetTypeRules(TypeInfo.PrimaryAssetType, TypeInfo.Rules);
	}

	StopBulkScanning();

	// Read primary asset rule overrides
	for (const FPrimaryAssetRulesOverride& Override : Settings.PrimaryAssetRules)
	{
		if (Override.PrimaryAssetId.PrimaryAssetType == PrimaryAssetLabelType)
		{
			UE_LOG(LogAssetManager, Error, TEXT("Cannot specify Rules overrides for Labels in ini! You most modify asset %s!"), *Override.PrimaryAssetId.ToString());
			continue;
		}

		SetPrimaryAssetRules(Override.PrimaryAssetId, Override.Rules);
	}
}

void UAssetManager::PostInitialAssetScan()
{
#if WITH_EDITOR
	if (bUpdateManagementDatabaseAfterScan)
	{
		bUpdateManagementDatabaseAfterScan = false;
		UpdateManagementDatabase(true);
	}
#endif
}

bool UAssetManager::GetManagedPackageList(FPrimaryAssetId PrimaryAssetId, TArray<FName>& PackagePathList) const
{
	bool bFoundAny = false;
	TArray<FAssetIdentifier> FoundDependencies;
	TArray<FString> DependencyStrings;

	IAssetRegistry& AssetRegistry = GetAssetRegistry();
	AssetRegistry.GetDependencies(PrimaryAssetId, FoundDependencies, EAssetRegistryDependencyType::Manage);

	for (const FAssetIdentifier& Identifier : FoundDependencies)
	{
		if (Identifier.PackageName != NAME_None)
		{
			bFoundAny = true;
			PackagePathList.Add(Identifier.PackageName);
		}
	}
	return bFoundAny;
}

bool UAssetManager::GetPackageManagers(FName PackageName, bool bRecurseToParents, TSet<FPrimaryAssetId>& ManagerSet) const
{
	IAssetRegistry& AssetRegistry = GetAssetRegistry();

	bool bFoundAny = false;
	TArray<FAssetIdentifier> ReferencingPrimaryAssets;

	AssetRegistry.GetReferencers(PackageName, ReferencingPrimaryAssets, EAssetRegistryDependencyType::Manage);

	for (int32 IdentifierIndex = 0; IdentifierIndex < ReferencingPrimaryAssets.Num(); IdentifierIndex++)
	{
		FPrimaryAssetId PrimaryAssetId = ReferencingPrimaryAssets[IdentifierIndex].GetPrimaryAssetId();
		if (PrimaryAssetId.IsValid())
		{
			bFoundAny = true;
			ManagerSet.Add(PrimaryAssetId);

			if (bRecurseToParents)
			{
				const TArray<FPrimaryAssetId> *ManagementParents = ManagementParentMap.Find(PrimaryAssetId);

				if (ManagementParents)
				{
					for (const FPrimaryAssetId& Manager : *ManagementParents)
					{
						// Add to end of list, this will recurse again if needed
						ReferencingPrimaryAssets.AddUnique(Manager);
					}
				}
			}
		}
	}
	return bFoundAny;
}

void UAssetManager::StartInitialLoading()
{
	ScanPrimaryAssetTypesFromConfig();
}

void UAssetManager::FinishInitialLoading()
{
	// See if we have pending scans, if so defer result
	bool bWaitingOnDeferredScan = false;

	for (const TPair<FName, TSharedRef<FPrimaryAssetTypeData>>& TypePair : AssetTypeMap)
	{
		const FPrimaryAssetTypeData& TypeData = TypePair.Value.Get();

		if (TypeData.DeferredAssetScanPaths.Num())
		{
			bWaitingOnDeferredScan = true;
		}
	}

	if (!bWaitingOnDeferredScan)
	{
		PostInitialAssetScan();
	}
}

bool UAssetManager::IsPathExcludedFromScan(const FString& Path) const
{
	const UAssetManagerSettings& Settings = GetSettings();

	for (const FDirectoryPath& ExcludedPath : GetSettings().DirectoriesToExclude)
	{
		if (Path.Contains(ExcludedPath.Path))
		{
			return true;
		}
	}

	return false;
}

#if WITH_EDITOR

EAssetSetManagerResult::Type UAssetManager::ShouldSetManager(const FAssetIdentifier& Manager, const FAssetIdentifier& Source, const FAssetIdentifier& Target, EAssetRegistryDependencyType::Type DependencyType, EAssetSetManagerFlags::Type Flags) const
{
	FPrimaryAssetId ManagerPrimaryAssetId = Manager.GetPrimaryAssetId();
	FPrimaryAssetId TargetPrimaryAssetId = Target.GetPrimaryAssetId();
	if (TargetPrimaryAssetId.IsValid())
	{
		// Don't recurse Primary Asset Id references
		return EAssetSetManagerResult::SetButDoNotRecurse;
	}

	const FString TargetPackageString = Target.PackageName.ToString();

	// Ignore script references
	if (TargetPackageString.StartsWith(TEXT("/Script/"), ESearchCase::CaseSensitive))
	{
		return EAssetSetManagerResult::DoNotSet;
	}

	if (Flags & EAssetSetManagerFlags::TargetHasExistingManager)
	{
		// If target has a higher priority manager, never recurse and only set manager if direct
		if (Flags & EAssetSetManagerFlags::IsDirectSet)
		{
			return EAssetSetManagerResult::SetButDoNotRecurse;
		}
		else
		{
			return EAssetSetManagerResult::DoNotSet;
		}
	}
	else if (Flags & EAssetSetManagerFlags::TargetHasDirectManager)
	{
		// If target has another direct manager being set in this run, never recurse and set manager if we think this is an "owner" reference and not a back reference

		bool bIsOwnershipReference = Flags & EAssetSetManagerFlags::IsDirectSet;

		if (ManagerPrimaryAssetId.PrimaryAssetType == MapType)
		{
			// References made by maps are ownership references, because there is no way to distinguish between sublevels and top level maps we "include" sublevels in parent maps via reference
			bIsOwnershipReference = true;
		}

		if (bIsOwnershipReference)
		{
			return EAssetSetManagerResult::SetButDoNotRecurse;
		}
		else
		{
			return EAssetSetManagerResult::DoNotSet;
		}
	}
	return EAssetSetManagerResult::SetAndRecurse;
}

void UAssetManager::ScanPathsSynchronous(const TArray<FString>& PathsToScan) const
{
	TArray<FString> Directories;
	TArray<FString> PackageFilenames;

	for (const FString& Path : PathsToScan)
	{
		bool bAlreadyScanned = false;
		int32 DotIndex = INDEX_NONE;
		if (Path.FindChar('.', DotIndex))
		{
			FString PackageName = FPackageName::ObjectPathToPackageName(Path);

			for (const FString& AlreadyScanned : AlreadyScannedDirectories)
			{
				if (PackageName.Contains(AlreadyScanned))
				{
					bAlreadyScanned = true;
					break;
				}
			}

			if (!bAlreadyScanned)
			{
				FString AssetFilename;
				// Try both extensions
				if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, AssetFilename, FPackageName::GetAssetPackageExtension()))
				{
					PackageFilenames.AddUnique(AssetFilename);
				}

				if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, AssetFilename, FPackageName::GetMapPackageExtension()))
				{
					PackageFilenames.AddUnique(AssetFilename);
				}
			}
		}
		else
		{
			for (const FString& AlreadyScanned : AlreadyScannedDirectories)
			{
				if (Path.Contains(AlreadyScanned))
				{
					bAlreadyScanned = true;
					break;
				}
			}

			if (!bAlreadyScanned)
			{
				AlreadyScannedDirectories.Add(Path);
				Directories.AddUnique(Path);
			}
		}
	}

	if (Directories.Num() > 0)
	{
		GetAssetRegistry().ScanPathsSynchronous(Directories);
	}
	if (PackageFilenames.Num() > 0)
	{
		GetAssetRegistry().ScanFilesSynchronous(PackageFilenames);
	}
}

void UAssetManager::OnAssetRegistryFilesLoaded()
{
	StartBulkScanning();

	for (TPair<FName, TSharedRef<FPrimaryAssetTypeData>>& TypePair : AssetTypeMap)
	{
		FPrimaryAssetTypeData& TypeData = TypePair.Value.Get();

		if (TypeData.DeferredAssetScanPaths.Num())
		{
			// File scan finished, now scan for assets. Maps are sorted so this will be in the order of original scan requests
			ScanPathsForPrimaryAssets(TypePair.Key, TypeData.DeferredAssetScanPaths, TypeData.Info.AssetBaseClassLoaded, TypeData.Info.bHasBlueprintClasses, TypeData.Info.bIsEditorOnly, false);

			TypeData.DeferredAssetScanPaths.Empty();
		}
	}

	StopBulkScanning();

	PostInitialAssetScan();
}

void UAssetManager::UpdateManagementDatabase(bool bForceRefresh)
{
	if (!GIsEditor)
	{
		// Doesn't work in standalone game because we haven't scanned all the paths
		UE_LOG(LogAssetManager, Error, TEXT("UpdateManagementDatabase does not work in standalone game because it doesn't load the entire Asset Registry!"));
	}

	// Construct the asset management map and pass it to the asset registry
	IAssetRegistry& AssetRegistry = GetAssetRegistry();

	if (AssetRegistry.IsLoadingAssets())
	{
		bUpdateManagementDatabaseAfterScan = true;
		return;
	}

	if (bIsManagementDatabaseCurrent && !bForceRefresh)
	{
		return;
	}

	ManagementParentMap.Reset();

	// Make sure the asset labels are up to date 
	ApplyPrimaryAssetLabels();

	// Map from Priority to map, then call in order
	TMap<int32, TMultiMap<FAssetIdentifier, FAssetIdentifier> > PriorityManagementMap;

	// List of references to not recurse on, priority doesn't matter
	TMultiMap<FAssetIdentifier, FAssetIdentifier> NoReferenceManagementMap;

	for (const TPair<FName, TSharedRef<FPrimaryAssetTypeData>>& TypePair : AssetTypeMap)
	{
		const FPrimaryAssetTypeData& TypeData = TypePair.Value.Get();

		for (const TPair<FName, FPrimaryAssetData>& NamePair : TypeData.AssetMap)
		{
			const FPrimaryAssetData& NameData = NamePair.Value;
			FPrimaryAssetId PrimaryAssetId(TypePair.Key, NamePair.Key);

			FPrimaryAssetRules Rules = GetPrimaryAssetRules(PrimaryAssetId);

			// Get the list of directly referenced assets, the registry wants it as FNames
			TArray<FName> AssetPackagesReferenced;

			const FSoftObjectPath& AssetRef = NameData.AssetPtr.ToSoftObjectPath();

			if (AssetRef.IsValid())
			{
				FName PackageName = FName(*AssetRef.GetLongPackageName());

				AssetPackagesReferenced.AddUnique(PackageName);
			}

			TMap<FName, FAssetBundleEntry>* BundleMap = CachedAssetBundles.Find(PrimaryAssetId);

			// Add bundle references to manual reference list
			if (BundleMap)
			{
				for (const TPair<FName, FAssetBundleEntry>& BundlePair : *BundleMap)
				{
					for (const FSoftObjectPath& BundleAssetRef : BundlePair.Value.BundleAssets)
					{
						FString PackageName = BundleAssetRef.GetLongPackageName();

						AssetPackagesReferenced.AddUnique(FName(*PackageName));
					}
				}
			}

			for (const FName& AssetPackage : AssetPackagesReferenced)
			{
				TMultiMap<FAssetIdentifier, FAssetIdentifier>& ManagerMap = Rules.bApplyRecursively ? PriorityManagementMap.FindOrAdd(Rules.Priority) : NoReferenceManagementMap;

				ManagerMap.Add(PrimaryAssetId, AssetPackage);
			}
		}
	}

	TArray<int32> PriorityArray;
	PriorityManagementMap.GenerateKeyArray(PriorityArray);

	// Sort to highest priority first
	PriorityArray.Sort([](const int32& LHS, const int32& RHS) { return LHS > RHS; });

	FScopedSlowTask SlowTask(PriorityArray.Num(), LOCTEXT("BuildingManagementDatabase", "Building Asset Management Database"));
	const bool bShowCancelButton = false;
	const bool bAllowInPIE = true;
	SlowTask.MakeDialog(bShowCancelButton, bAllowInPIE);

	auto SetManagerPredicate = [this](const FAssetIdentifier& Manager, const FAssetIdentifier& Source, const FAssetIdentifier& Target, EAssetRegistryDependencyType::Type DependencyType, EAssetSetManagerFlags::Type Flags)
	{
		return this->ShouldSetManager(Manager, Source, Target, DependencyType, Flags);
	};

	for (int32 PriorityIndex = 0; PriorityIndex < PriorityArray.Num(); PriorityIndex++)
	{
		TMultiMap<FAssetIdentifier, FAssetIdentifier>* ManagerMap = PriorityManagementMap.Find(PriorityArray[PriorityIndex]);

		SlowTask.EnterProgressFrame(1);

		AssetRegistry.SetManageReferences(*ManagerMap, PriorityIndex == 0, EAssetRegistryDependencyType::Packages, SetManagerPredicate);
	}

	// Do non recursive set last
	if (NoReferenceManagementMap.Num() > 0)
	{
		AssetRegistry.SetManageReferences(NoReferenceManagementMap, false, EAssetRegistryDependencyType::None);
	}


	TMultiMap<FAssetIdentifier, FAssetIdentifier> PrimaryAssetIdManagementMap;

	// Update management parent list, which is PrimaryAssetId -> PrimaryAssetId
	for (const TPair<FName, TSharedRef<FPrimaryAssetTypeData>>& TypePair : AssetTypeMap)
	{
		const FPrimaryAssetTypeData& TypeData = TypePair.Value.Get();

		for (const TPair<FName, FPrimaryAssetData>& NamePair : TypeData.AssetMap)
		{
			const FPrimaryAssetData& NameData = NamePair.Value;
			FPrimaryAssetId PrimaryAssetId(TypePair.Key, NamePair.Key);
			const FSoftObjectPath& AssetRef = NameData.AssetPtr.ToSoftObjectPath();

			if (AssetRef.IsValid())
			{
				FName PackageName = FName(*AssetRef.GetLongPackageName());

				TSet<FPrimaryAssetId> Managers;

				if (GetPackageManagers(PackageName, false, Managers) && Managers.Num() > 1)
				{
					// Find all managers that aren't this specific asset
					for (const FPrimaryAssetId& Manager : Managers)
					{
						if (Manager != PrimaryAssetId)
						{
							// Update the cached version and the version in registry
							ManagementParentMap.FindOrAdd(PrimaryAssetId).AddUnique(Manager);

							PrimaryAssetIdManagementMap.Add(Manager, PrimaryAssetId);
						}
					}
				}
			}
		}
	}

	if (PrimaryAssetIdManagementMap.Num() > 0)
	{
		AssetRegistry.SetManageReferences(PrimaryAssetIdManagementMap, false, EAssetRegistryDependencyType::None);
	}


	bIsManagementDatabaseCurrent = true;
}

void UAssetManager::ApplyPrimaryAssetLabels()
{
	// Load all of them off disk. Turn off string asset reference tracking to avoid them getting cooked
	FSoftObjectPathSerializationScope SerializationScope(NAME_None, NAME_None, ESoftObjectPathCollectType::NeverCollect);

	TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAssetsWithType(PrimaryAssetLabelType);

	if (Handle.IsValid())
	{
		Handle->WaitUntilComplete();
	}
	
	// PostLoad in PrimaryAssetLabel sets PrimaryAssetRules overrides
}

void UAssetManager::ModifyCook(TArray<FName>& PackagesToCook, TArray<FName>& PackagesToNeverCook)
{
	// Make sure management database is set up
	UpdateManagementDatabase();

	// Cook all non-editor types
	TArray<FPrimaryAssetTypeInfo> TypeList;

	GetPrimaryAssetTypeInfoList(TypeList);

	// Get package names in the libraries that we care about for cooking. Only get ones that are needed in production
	for (const FPrimaryAssetTypeInfo& TypeInfo : TypeList)
	{
		// Cook these types
		TArray<FAssetData> AssetDataList;
		GetPrimaryAssetDataList(TypeInfo.PrimaryAssetType, AssetDataList);

		for (const FAssetData& AssetData : AssetDataList)
		{
			EPrimaryAssetCookRule CookRule = GetPackageCookRule(AssetData.PackageName);

			if (CookRule == EPrimaryAssetCookRule::AlwaysCook && !TypeInfo.bIsEditorOnly)
			{
				// If this is always cook and not editor only, cook it
				PackagesToCook.AddUnique(AssetData.PackageName);
			}
			else if (!VerifyCanCookPackage(AssetData.PackageName, false))
			{
				// If this package cannot be cooked, add to exclusion list
				PackagesToNeverCook.AddUnique(AssetData.PackageName);
			}
		}
	}
}

EPrimaryAssetCookRule UAssetManager::GetPackageCookRule(FName PackageName) const
{
	FPrimaryAssetRules BestRules;
	FPrimaryAssetId BestId;
	TSet<FPrimaryAssetId> Managers;
	GetPackageManagers(PackageName, true, Managers);

	for (const FPrimaryAssetId& PrimaryAssetId : Managers)
	{
		FPrimaryAssetRules Rules = GetPrimaryAssetRules(PrimaryAssetId);

		if (Rules.CookRule != EPrimaryAssetCookRule::Unknown && Rules.CookRule != BestRules.CookRule)
		{
			if (BestRules.CookRule == EPrimaryAssetCookRule::Unknown || Rules.Priority > BestRules.Priority)
			{
				BestRules = Rules;
				BestId = PrimaryAssetId;
			}
			else
			{
				// Lower priority, ignore
				if (BestRules.Priority == Rules.Priority)
				{
					UE_LOG(LogAssetManager, Error, TEXT("GetPackageCookRule: Conflicting Cook Rule for package %s! %s and %s have the same priority and disagree."), *PackageName.ToString(), *PrimaryAssetId.ToString(), *BestId.ToString());
				}
			}
		}
	}

	return BestRules.CookRule;
}

bool UAssetManager::VerifyCanCookPackage(FName PackageName, bool bLogError) const
{
	EPrimaryAssetCookRule CookRule = UAssetManager::Get().GetPackageCookRule(PackageName);
	if (CookRule == EPrimaryAssetCookRule::NeverCook)
	{
		if (bLogError)
		{
			UE_LOG(LogAssetManager, Error, TEXT("Package %s is set to NeverCook, but something is trying to cook it!"), *PackageName.ToString());
		}
		
		return false;
	}
	else if (CookRule == EPrimaryAssetCookRule::DevelopmentCook && bOnlyCookProductionAssets)
	{
		if (bLogError)
		{
			UE_LOG(LogAssetManager, Error, TEXT("Package %s is set to Development, but bOnlyCookProductionAssets is true!"), *PackageName.ToString());
		}

		return false;
	}

	return true;
}

bool UAssetManager::GetPackageChunkIds(FName PackageName, const ITargetPlatform* TargetPlatform, const TArray<int32>& ExistingChunkList, TArray<int32>& OutChunkList) const
{
	// Include preset chunks
	OutChunkList.Append(ExistingChunkList);

	if (PackageName.ToString().StartsWith(TEXT("/Engine/"), ESearchCase::CaseSensitive))
	{
		// Some engine content is only referenced by string, make sure it's all in chunk 0 to avoid issues
		OutChunkList.AddUnique(0);
	}

	// Add all chunk ids from the asset rules of managers. By default priority will not override other chunks
	bool bFoundAny = false;
	TSet<FPrimaryAssetId> Managers;

	GetPackageManagers(PackageName, true, Managers);

	for (const FPrimaryAssetId& PrimaryAssetId : Managers)
	{
		FPrimaryAssetRules Rules = GetPrimaryAssetRules(PrimaryAssetId);

		if (Rules.ChunkId != INDEX_NONE)
		{
			bFoundAny = true;
			OutChunkList.AddUnique(Rules.ChunkId);
		}
	}

	int32 HighestChunk = 0;
	for (int32 ChunkId : OutChunkList)
	{
		if (ChunkId > HighestChunk)
		{
			HighestChunk = ChunkId;
		}
	}

	// Use chunk dependency info to remove redundant chunks
	UChunkDependencyInfo *DependencyInfo = GetMutableDefault<UChunkDependencyInfo>();
	DependencyInfo->GetOrBuildChunkDependencyGraph(HighestChunk);
	DependencyInfo->RemoveRedundantChunks(OutChunkList);

	return bFoundAny;
}

void UAssetManager::PreBeginPIE(bool bStartSimulate)
{
	RefreshPrimaryAssetDirectory();

	// Cache asset state
	GetPrimaryAssetBundleStateMap(PrimaryAssetStateBeforePIE, false);
}

void UAssetManager::EndPIE(bool bStartSimulate)
{
	// Reset asset load state
	for (const TPair<FName, TSharedRef<FPrimaryAssetTypeData>>& TypePair : AssetTypeMap)
	{
		const FPrimaryAssetTypeData& TypeData = TypePair.Value.Get();

		for (const TPair<FName, FPrimaryAssetData>& NamePair : TypeData.AssetMap)
		{
			const FPrimaryAssetData& NameData = NamePair.Value;
			const FPrimaryAssetLoadState& LoadState = (!NameData.PendingState.IsValid()) ? NameData.CurrentState : NameData.PendingState;

			if (!LoadState.IsValid())
			{
				// Don't worry about things that aren't loaded
				continue;
			}

			FPrimaryAssetId AssetID(TypePair.Key, NamePair.Key);

			TArray<FName>* BundleState = PrimaryAssetStateBeforePIE.Find(AssetID);

			if (BundleState)
			{
				// This will reset state to what it was before
				LoadPrimaryAsset(AssetID, *BundleState);
			}
			else
			{
				// Not in map, unload us
				UnloadPrimaryAsset(AssetID);
			}
		}
	}
}

void UAssetManager::RefreshPrimaryAssetDirectory()
{
	StartBulkScanning();

	for (TPair<FName, TSharedRef<FPrimaryAssetTypeData>>& TypePair : AssetTypeMap)
	{
		FPrimaryAssetTypeData& TypeData = TypePair.Value.Get();

		// Rescan the runtime data, the class may have gotten changed by hot reload
		if (!TypeData.Info.FillRuntimeData())
		{
			continue;
		}

		if (TypeData.Info.AssetScanPaths.Num())
		{
			// Clear old data
			TypeData.AssetMap.Reset();

			// Rescan all assets
			ScanPathsForPrimaryAssets(TypePair.Key, TypeData.Info.AssetScanPaths, TypeData.Info.AssetBaseClassLoaded, TypeData.Info.bHasBlueprintClasses, TypeData.Info.bIsEditorOnly, true);
		}
	}

	StopBulkScanning();

	PostInitialAssetScan();
}

void UAssetManager::ReinitializeFromConfig()
{
	// We specifically do not reset AssetRuleOverrides as those can be set by something other than inis
	AssetPathMap.Reset();
	ManagementParentMap.Reset();
	CachedAssetBundles.Reset();
	AlreadyScannedDirectories.Reset();
	AssetTypeMap.Reset();

	// This code is editor only, so reinitialize globals
	const UAssetManagerSettings& Settings = GetSettings();
	bShouldGuessTypeAndName = Settings.bShouldGuessTypeAndNameInEditor;
	bShouldAcquireMissingChunksOnLoad = Settings.bShouldAcquireMissingChunksOnLoad;
	bOnlyCookProductionAssets = Settings.bOnlyCookProductionAssets;

	LoadRedirectorMaps();
	ScanPrimaryAssetTypesFromConfig();
}

void UAssetManager::OnInMemoryAssetCreated(UObject *Object)
{
	// Ignore PIE and CDO changes
	if (GIsPlayInEditorWorld || !Object || Object->HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	FPrimaryAssetId PrimaryAssetId = Object->GetPrimaryAssetId();

	if (PrimaryAssetId.IsValid())
	{
		TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(PrimaryAssetId.PrimaryAssetType);

		if (FoundType)
		{
			IAssetRegistry& AssetRegistry = GetAssetRegistry();

			FPrimaryAssetTypeData& TypeData = FoundType->Get();

			FAssetData NewAssetData;

			GetAssetDataForPathInternal(AssetRegistry, Object->GetPathName(), NewAssetData);

			if (NewAssetData.IsValid())
			{
				// Make sure it's in a valid path
				bool bFoundPath = false;
				for (const FString& Path : TypeData.Info.AssetScanPaths)
				{
					if (NewAssetData.PackagePath.ToString().Contains(Path))
					{
						bFoundPath = true;
						break;
					}
				}

				if (bFoundPath)
				{
					// Add or update asset data
					UpdateCachedAssetData(PrimaryAssetId, NewAssetData, true);

					RebuildObjectReferenceList();
				}
			}
		}
	}
}

void UAssetManager::OnInMemoryAssetDeleted(UObject *Object)
{
	// Ignore PIE changes
	if (GIsPlayInEditorWorld || !Object)
	{
		return;
	}

	FPrimaryAssetId PrimaryAssetId = Object->GetPrimaryAssetId();

	RemovePrimaryAssetId(PrimaryAssetId);
}

void UAssetManager::OnAssetRenamed(const FAssetData& NewData, const FString& OldPath)
{
	// Ignore PIE changes
	if (GIsPlayInEditorWorld || !NewData.IsValid())
	{
		return;
	}

	FPrimaryAssetId OldPrimaryAssetId = GetPrimaryAssetIdForPath(OldPath);

	// This may be a blueprint, try with _C
	if (!OldPrimaryAssetId.IsValid())
	{
		OldPrimaryAssetId = GetPrimaryAssetIdForPath(OldPath + TEXT("_C"));
	}

	RemovePrimaryAssetId(OldPrimaryAssetId);

	// This will always be in memory
	UObject *NewObject = NewData.GetAsset();

	OnInMemoryAssetCreated(NewObject);
}

void UAssetManager::RemovePrimaryAssetId(const FPrimaryAssetId& PrimaryAssetId)
{
	if (PrimaryAssetId.IsValid() && GetNameData(PrimaryAssetId))
	{
		// It's in our dictionary, remove it

		TSharedRef<FPrimaryAssetTypeData>* FoundType = AssetTypeMap.Find(PrimaryAssetId.PrimaryAssetType);
		check(FoundType);
		FPrimaryAssetTypeData& TypeData = FoundType->Get();

		TypeData.AssetMap.Remove(PrimaryAssetId.PrimaryAssetName);

		RebuildObjectReferenceList();
	}
}

void UAssetManager::RefreshAssetData(UObject* ChangedObject)
{
	// If this is a BP CDO, call on class instead
	if (ChangedObject->HasAnyFlags(RF_ClassDefaultObject))
	{
		UBlueprintGeneratedClass* AssetClass = Cast<UBlueprintGeneratedClass>(ChangedObject->GetClass());
		if (AssetClass)
		{
			RefreshAssetData(AssetClass);
		}
		return;
	}

	// Only update things it knows about
	IAssetRegistry& AssetRegistry = GetAssetRegistry();
	FSoftObjectPath ChangedObjectPath(ChangedObject);
	FPrimaryAssetId PrimaryAssetId = ChangedObject->GetPrimaryAssetId();
	FPrimaryAssetId OldPrimaryAssetId = GetPrimaryAssetIdForPath(ChangedObjectPath);
	
	// This may be a blueprint, try with _C
	if (!OldPrimaryAssetId.IsValid())
	{
		OldPrimaryAssetId = GetPrimaryAssetIdForPath(ChangedObjectPath.ToString() + TEXT("_C"));
	}

	if (PrimaryAssetId.IsValid() && OldPrimaryAssetId == PrimaryAssetId)
	{
		// Same AssetId, this will update cache out of the in memory object
		FAssetData NewData;
		GetAssetDataForPathInternal(AssetRegistry, ChangedObjectPath.ToString(), NewData);

		if (ensure(NewData.IsValid()))
		{
			UpdateCachedAssetData(PrimaryAssetId, NewData, false);
		}
	}
	else
	{
		// AssetId changed
		if (OldPrimaryAssetId.IsValid())
		{
			// Remove old id if it was registered
			RemovePrimaryAssetId(OldPrimaryAssetId);
		}

		if (PrimaryAssetId.IsValid())
		{
			// This will add new id
			OnInMemoryAssetCreated(ChangedObject);
		}
	}
}

void UAssetManager::InitializeAssetBundlesFromMetadata(const UStruct* Struct, const void* StructValue, FAssetBundleData& AssetBundle) const
{
	static FName AssetBundlesName = TEXT("AssetBundles");

	if (!ensure(Struct && StructValue))
	{
		return;
	}

	for (TPropertyValueIterator<const UProperty> It(Struct, StructValue); It; ++It)
	{
		const UProperty* Property = It.Key();
		const void* PropertyValue = It.Value();

		FSoftObjectPath FoundRef;
		if (const USoftClassProperty* AssetClassProp = Cast<USoftClassProperty>(Property))
		{
			const TSoftClassPtr<UObject>* AssetClassPtr = reinterpret_cast<const TSoftClassPtr<UObject>*>(PropertyValue);
			if (AssetClassPtr)
			{
				FoundRef = AssetClassPtr->ToSoftObjectPath();
			}
		}
		else if (const USoftObjectProperty* AssetProp = Cast<USoftObjectProperty>(Property))
		{
			const TSoftObjectPtr<UObject>* AssetPtr = reinterpret_cast<const TSoftObjectPtr<UObject>*>(PropertyValue);
			if (AssetPtr)
			{
				FoundRef = AssetPtr->ToSoftObjectPath();
			}
		}
		else if (const UStructProperty* StructProperty = Cast<UStructProperty>(Property))
		{
			// SoftClassPath is binary identical with SoftObjectPath
			if (StructProperty->Struct == TBaseStructure<FSoftObjectPath>::Get() || StructProperty->Struct == TBaseStructure<FSoftClassPath>::Get())
			{
				const FSoftObjectPath* AssetRefPtr = reinterpret_cast<const FSoftObjectPath*>(PropertyValue);
				if (AssetRefPtr)
				{
					FoundRef = *AssetRefPtr;
				}
				// Skip recursion, we don't care about the raw string property
				It.SkipRecursiveProperty();
			}
		}

		if (!FoundRef.IsNull())
		{
			// Compute the intersection of all specified bundle sets in this property and parent properties
			TSet<FName> BundleSet;

			TArray<const UProperty*> PropertyChain;
			It.GetPropertyChain(PropertyChain);

			for (const UProperty* PropertyToSearch : PropertyChain)
			{
				if (PropertyToSearch->HasMetaData(AssetBundlesName))
				{
					TSet<FName> LocalBundleSet;
					TArray<FString> BundleList;
					const FString& BundleString = PropertyToSearch->GetMetaData(AssetBundlesName);
					BundleString.ParseIntoArrayWS(BundleList, TEXT(","));

					for (const FString& BundleNameString : BundleList)
					{
						LocalBundleSet.Add(FName(*BundleNameString));
					}

					// If Set is empty, initialize. Otherwise intersect
					if (BundleSet.Num() == 0)
					{
						BundleSet = LocalBundleSet;
					}
					else
					{
						BundleSet = BundleSet.Intersect(LocalBundleSet);
					}
				}
			}

			for (const FName& BundleName : BundleSet)
			{
				AssetBundle.AddBundleAsset(BundleName, FoundRef);
			}
		}
	}
}

#endif // #if WITH_EDITOR

#undef LOCTEXT_NAMESPACE