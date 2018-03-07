// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetRegistryGenerator.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/ArrayWriter.h"
#include "Misc/App.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Settings/ProjectPackagingSettings.h"
#include "CollectionManagerTypes.h"
#include "ICollectionManager.h"
#include "CollectionManagerModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "AssetRegistryModule.h"
#include "GameDelegates.h"
#include "Commandlets/ChunkDependencyInfo.h"
#include "IPlatformFileSandboxWrapper.h"
#include "Misc/ConfigCacheIni.h"
#include "Stats/StatsMisc.h"
#include "UniquePtr.h"
#include "Engine/AssetManager.h"

#include "JsonWriter.h"
#include "JsonReader.h"
#include "JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogAssetRegistryGenerator, Log, All);

#define LOCTEXT_NAMESPACE "AssetRegistryGenerator"

//////////////////////////////////////////////////////////////////////////
// Static functions
FName GetPackageNameFromDependencyPackageName(const FName RawPackageFName)
{
	FName PackageFName = RawPackageFName;
	if ((FPackageName::IsValidLongPackageName(RawPackageFName.ToString()) == false) &&
		(FPackageName::IsScriptPackage(RawPackageFName.ToString()) == false))
	{
		FText OutReason;
		if (!FPackageName::IsValidLongPackageName(RawPackageFName.ToString(), true, &OutReason))
		{
			const FText FailMessage = FText::Format(LOCTEXT("UnableToGeneratePackageName", "Unable to generate long package name for {0}. {1}"),
				FText::FromString(RawPackageFName.ToString()), OutReason);

			UE_LOG(LogAssetRegistryGenerator, Warning, TEXT("%s"), *(FailMessage.ToString()));
			return NAME_None;
		}


		FString LongPackageName;
		if (FPackageName::SearchForPackageOnDisk(RawPackageFName.ToString(), &LongPackageName) == false)
		{
			return NAME_None;
		}
		PackageFName = FName(*LongPackageName);
	}

	// don't include script packages in dependencies as they are always in memory
	if (FPackageName::IsScriptPackage(PackageFName.ToString()))
	{
		// no one likes script packages
		return NAME_None;
	}
	return PackageFName;
}


//////////////////////////////////////////////////////////////////////////
// FAssetRegistryGenerator

FAssetRegistryGenerator::FAssetRegistryGenerator(const ITargetPlatform* InPlatform)
	: AssetRegistry(FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get())
	, TargetPlatform(InPlatform)
	, bGenerateChunks(false)
	, bUseAssetManager(false)
{
	DependencyInfo = GetMutableDefault<UChunkDependencyInfo>();

	bool bOnlyHardReferences = false;
	const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();
	if (PackagingSettings)
	{
		bOnlyHardReferences = PackagingSettings->bChunkHardReferencesOnly;
	}	

	DependencyType = bOnlyHardReferences ? EAssetRegistryDependencyType::Hard : EAssetRegistryDependencyType::Packages;

	if (UAssetManager::IsValid() && !FGameDelegates::Get().GetAssignStreamingChunkDelegate().IsBound() && !FGameDelegates::Get().GetGetPackageDependenciesForManifestGeneratorDelegate().IsBound())
	{
		bUseAssetManager = true;

		UAssetManager::Get().UpdateManagementDatabase();
	}
}

FAssetRegistryGenerator::~FAssetRegistryGenerator()
{
	for (auto ChunkSet : ChunkManifests)
	{
		delete ChunkSet;
	}
	ChunkManifests.Empty();
	for (auto ChunkSet : FinalChunkManifests)
	{
		delete ChunkSet;
	}
	FinalChunkManifests.Empty();
}

bool FAssetRegistryGenerator::CleanTempPackagingDirectory(const FString& Platform) const
{
	FString TmpPackagingDir = GetTempPackagingDirectoryForPlatform(Platform);
	if (IFileManager::Get().DirectoryExists(*TmpPackagingDir))
	{
		if (!IFileManager::Get().DeleteDirectory(*TmpPackagingDir, false, true))
		{
			UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Failed to delete directory: %s"), *TmpPackagingDir);
			return false;
		}
	}

	FString ChunkListDir = FPaths::Combine(*FPaths::ProjectLogDir(), TEXT("ChunkLists"));
	if (IFileManager::Get().DirectoryExists(*ChunkListDir))
	{
		if (!IFileManager::Get().DeleteDirectory(*ChunkListDir, false, true))
		{
			UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Failed to delete directory: %s"), *ChunkListDir);
			return false;
		}
	}
	return true;
}

bool FAssetRegistryGenerator::ShouldPlatformGenerateStreamingInstallManifest(const ITargetPlatform* Platform) const
{
	if (Platform)
	{
		FConfigFile PlatformIniFile;
		FConfigCacheIni::LoadLocalIniFile(PlatformIniFile, TEXT("Game"), true, *Platform->IniPlatformName());
		FString ConfigString;
		if (PlatformIniFile.GetString(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("bGenerateChunks"), ConfigString))
		{
			return FCString::ToBool(*ConfigString);
		}
	}

	return false;
}


int64 FAssetRegistryGenerator::GetMaxChunkSizePerPlatform(const ITargetPlatform* Platform) const
{
	if ( Platform )
	{
		FConfigFile PlatformIniFile;
		FConfigCacheIni::LoadLocalIniFile(PlatformIniFile, TEXT("Game"), true, *Platform->IniPlatformName());
		FString ConfigString;
		if (PlatformIniFile.GetString(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("MaxChunkSize"), ConfigString))
		{
			return FCString::Atoi64(*ConfigString);
		}
	}

	return -1;
}

bool FAssetRegistryGenerator::GenerateStreamingInstallManifest()
{
	const FString Platform = TargetPlatform->PlatformName();

	// empty out the current paklist directory
	FString TmpPackagingDir = GetTempPackagingDirectoryForPlatform(Platform);

	int64 MaxChunkSize = GetMaxChunkSizePerPlatform( TargetPlatform );

	if (!IFileManager::Get().MakeDirectory(*TmpPackagingDir, true))
	{
		UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Failed to create directory: %s"), *TmpPackagingDir);
		return false;
	}
	
	// open a file for writing the list of pak file lists that we've generated
	FString PakChunkListFilename = TmpPackagingDir / TEXT("pakchunklist.txt");
	TUniquePtr<FArchive> PakChunkListFile(IFileManager::Get().CreateFileWriter(*PakChunkListFilename));

	if (!PakChunkListFile)
	{
		UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Failed to open output pakchunklist file %s"), *PakChunkListFilename);
		return false;
	}

	FString PakChunkLayerInfoFilename = FString::Printf(TEXT("%s/pakchunklayers.txt"), *TmpPackagingDir);
	TUniquePtr<FArchive> ChunkLayerFile(IFileManager::Get().CreateFileWriter(*PakChunkLayerInfoFilename));

	// generate per-chunk pak list files
	for (int32 Index = 0; Index < FinalChunkManifests.Num(); ++Index)
	{
		// Is this chunk empty?
		if (!FinalChunkManifests[Index] || FinalChunkManifests[Index]->Num() == 0)
		{
			continue;
		}

		int32 FilenameIndex = 0;
		TArray<FString> ChunkFilenames;
		FinalChunkManifests[Index]->GenerateValueArray(ChunkFilenames);
		int32 SubChunkIndex = 0;
		while ( true )
		{
			FString PakChunkFilename = FString::Printf(TEXT("pakchunk%d.txt"), Index);
			if ( SubChunkIndex > 0 )
			{
				PakChunkFilename = FString::Printf(TEXT("pakchunk%d_s%d.txt"), Index, SubChunkIndex);
			}
			++SubChunkIndex;
			FString PakListFilename = FString::Printf(TEXT("%s/%s"), *TmpPackagingDir, *PakChunkFilename, Index);
			TUniquePtr<FArchive> PakListFile(IFileManager::Get().CreateFileWriter(*PakListFilename));

			if (!PakListFile)
			{
				UE_LOG(LogAssetRegistryGenerator, Error, TEXT("Failed to open output paklist file %s"), *PakListFilename);
				return false;
			}

			int64 CurrentPakSize = 0;
			bool bFinishedAllFiles = true;

			if (bUseAssetManager)
			{
				// Sort so the order is consistent. If load order is important then it should be specified as a load order file to UnrealPak
				ChunkFilenames.Sort();
			}

			for (; FilenameIndex < ChunkFilenames.Num(); ++FilenameIndex)
			{
				FString Filename = ChunkFilenames[FilenameIndex];
				FString PakListLine = FPaths::ConvertRelativePathToFull(Filename.Replace(TEXT("[Platform]"), *Platform));
				if (MaxChunkSize > 0)
				{
					TArray<FString> FoundFiles;
					FString FileSearchString = FString::Printf(TEXT("%s.*"), *PakListLine);
					IFileManager::Get().FindFiles(FoundFiles, *FileSearchString, true,false);
					const FString Path = FPaths::GetPath(FileSearchString);
					for ( const FString& FoundFile : FoundFiles )
					{
						int64 FileSize = IFileManager::Get().FileSize(*(FPaths::Combine(Path,FoundFile)));
						CurrentPakSize +=  FileSize > 0 ? FileSize : 0;
					}
					if (MaxChunkSize < CurrentPakSize)
					{
						// early out if we are over memory limit
						bFinishedAllFiles = false;
						break;
					}
				}
				
				PakListLine.ReplaceInline(TEXT("/"), TEXT("\\"));
				PakListLine += TEXT("\r\n");
				PakListFile->Serialize(TCHAR_TO_ANSI(*PakListLine), PakListLine.Len());
			}

			PakListFile->Close();

			// add this pakfilelist to our master list of pakfilelists
			FString PakChunkListLine = FString::Printf(TEXT("%s\r\n"), *PakChunkFilename);
			PakChunkListFile->Serialize(TCHAR_TO_ANSI(*PakChunkListLine), PakChunkListLine.Len());

			int32 TargetLayer = 0;
			FGameDelegates::Get().GetAssignLayerChunkDelegate().ExecuteIfBound(FinalChunkManifests[Index], Platform, Index, TargetLayer);

			FString LayerString = FString::Printf(TEXT("%d\r\n"), TargetLayer);

			ChunkLayerFile->Serialize(TCHAR_TO_ANSI(*LayerString), LayerString.Len());
			
			if (bFinishedAllFiles)
			{
				break;
			}
		}

	}

	ChunkLayerFile->Close();
	PakChunkListFile->Close();

	return true;
}

void FAssetRegistryGenerator::GenerateChunkManifestForPackage(const FName& PackageFName, const FString& PackagePathName, const FString& SandboxFilename, const FString& LastLoadedMapName, FSandboxPlatformFile* InSandboxFile)
{
	TArray<int32> TargetChunks;
	TArray<int32> ExistingChunkIDs;

	if (!bGenerateChunks)
	{
		TargetChunks.AddUnique(0);
		ExistingChunkIDs.AddUnique(0);
	}

	if (bGenerateChunks)
	{
		// Collect all chunk IDs associated with this package from the asset registry
		TArray<int32> RegistryChunkIDs = GetAssetRegistryChunkAssignments(PackageFName);

		ExistingChunkIDs = GetExistingPackageChunkAssignments(PackageFName);
		if (bUseAssetManager)
		{
			// No distinction between source of existing chunks for new flow
			RegistryChunkIDs.Append(ExistingChunkIDs);

			UAssetManager::Get().GetPackageChunkIds(PackageFName, TargetPlatform, RegistryChunkIDs, TargetChunks);
		}
		else
		{
			// Try to call game-specific delegate to determine the target chunk ID
			// FString Name = Package->GetPathName();
			if (FGameDelegates::Get().GetAssignStreamingChunkDelegate().IsBound())
			{
				FGameDelegates::Get().GetAssignStreamingChunkDelegate().ExecuteIfBound(PackagePathName, LastLoadedMapName, RegistryChunkIDs, ExistingChunkIDs, TargetChunks);
			}
			else
			{
				//Take asset registry assignments and existing assignments
				TargetChunks.Append(RegistryChunkIDs);
				TargetChunks.Append(ExistingChunkIDs);
			}
		}
	}

	// if the delegate requested a specific chunk assignment, add them package to it now.
	for (const auto& PackageChunk : TargetChunks)
	{
		AddPackageToManifest(SandboxFilename, PackageFName, PackageChunk);
	}
	// If the delegate requested to remove the package from any chunk, remove it now
	for (const auto& PackageChunk : ExistingChunkIDs)
	{
		if (!TargetChunks.Contains(PackageChunk))
		{
			RemovePackageFromManifest(PackageFName, PackageChunk);
		}
	}

}


void FAssetRegistryGenerator::CleanManifestDirectories()
{
	CleanTempPackagingDirectory(TargetPlatform->PlatformName());
}

bool FAssetRegistryGenerator::LoadPreviousAssetRegistry(const FString& Filename)
{
	// First try development asset registry
	FArrayReader SerializedAssetData;
	
	if (IFileManager::Get().FileExists(*Filename) && FFileHelper::LoadFileToArray(SerializedAssetData, *Filename))
	{
		FAssetRegistrySerializationOptions Options;
		Options.ModifyForDevelopment();

		return PreviousState.Serialize(SerializedAssetData, Options);
	}

	return false;
}

bool FAssetRegistryGenerator::SaveManifests(FSandboxPlatformFile* InSandboxFile)
{
	// Always do package dependency work, is required to modify asset registry
	FixupPackageDependenciesForChunks(InSandboxFile);

	if (bGenerateChunks)
	{	
		if (!GenerateStreamingInstallManifest())
		{
			return false;
		}

		// Generate map for the platform abstraction
		TMultiMap<FString, int32> ChunkMap;	// asset -> ChunkIDs map
		TSet<int32> ChunkIDsInUse;
		const FString PlatformName = TargetPlatform->PlatformName();

		// Collect all unique chunk indices and map all files to their chunks
		for (int32 ChunkIndex = 0; ChunkIndex < FinalChunkManifests.Num(); ++ChunkIndex)
		{
			if (FinalChunkManifests[ChunkIndex] && FinalChunkManifests[ChunkIndex]->Num())
			{
				ChunkIDsInUse.Add(ChunkIndex);
				for (auto& Filename : *FinalChunkManifests[ChunkIndex])
				{
					FString PlatFilename = Filename.Value.Replace(TEXT("[Platform]"), *PlatformName);
					ChunkMap.Add(PlatFilename, ChunkIndex);
				}
			}
		}

		// Sort our chunk IDs and file paths
		ChunkMap.KeySort(TLess<FString>());
		ChunkIDsInUse.Sort(TLess<int32>());

		// Platform abstraction will generate any required platform-specific files for the chunks
		if (!TargetPlatform->GenerateStreamingInstallManifest(ChunkMap, ChunkIDsInUse))
		{
			return false;
		}

		if (!bUseAssetManager)
		{
			GenerateAssetChunkInformationCSV(FPaths::Combine(*FPaths::ProjectLogDir(), TEXT("ChunkLists")));
		}
	}

	return true;
}

bool FAssetRegistryGenerator::ContainsMap(const FName& PackageName) const
{
	return PackagesContainingMaps.Contains(PackageName);
}

FAssetPackageData* FAssetRegistryGenerator::GetAssetPackageData(const FName& PackageName)
{
	return State.CreateOrGetAssetPackageData(PackageName);
}

void FAssetRegistryGenerator::Initialize(const TArray<FName> &InStartupPackages)
{
	StartupPackages.Append(InStartupPackages);

	FAssetRegistrySerializationOptions SaveOptions;

	ensureMsgf(!AssetRegistry.IsLoadingAssets(), TEXT("Cannot initialize asset registry generator while asset registry is still scanning source assets "));

	AssetRegistry.InitializeSerializationOptions(SaveOptions, TargetPlatform->IniPlatformName());

	AssetRegistry.InitializeTemporaryAssetRegistryState(State, SaveOptions);
}

void FAssetRegistryGenerator::ComputePackageDifferences(TSet<FName>& ModifiedPackages, TSet<FName>& NewPackages, TSet<FName>& RemovedPackages, TSet<FName>& IdenticalCookedPackages, TSet<FName>& IdenticalUncookedPackages, bool bRecurseModifications, bool bRecurseScriptModifications)
{
	TArray<FName> ModifiedScriptPackages;

	for (const TPair<FName, const FAssetPackageData*>& PackagePair : State.GetAssetPackageDataMap())
	{
		FName PackageName = PackagePair.Key;
		const FAssetPackageData* CurrentPackageData = PackagePair.Value;

		const FAssetPackageData* PreviousPackageData = PreviousState.GetAssetPackageData(PackageName);

		if (!PreviousPackageData)
		{
			NewPackages.Add(PackageName);
		}
		else if (CurrentPackageData->PackageGuid == PreviousPackageData->PackageGuid)
		{
			if (PreviousPackageData->DiskSize < 0)
			{
				IdenticalUncookedPackages.Add(PackageName);
			}
			else
			{
				IdenticalCookedPackages.Add(PackageName);
			}
		}
		else
		{
			if (FPackageName::IsScriptPackage(PackageName.ToString()))
			{
				ModifiedScriptPackages.Add(PackageName);
			}
			else
			{
				ModifiedPackages.Add(PackageName);
			}
		}
	}

	for (const TPair<FName, const FAssetPackageData*>& PackagePair : PreviousState.GetAssetPackageDataMap())
	{
		FName PackageName = PackagePair.Key;
		const FAssetPackageData* PreviousPackageData = PackagePair.Value;

		const FAssetPackageData* CurrentPackageData = State.GetAssetPackageData(PackageName);

		if (!CurrentPackageData)
		{
			RemovedPackages.Add(PackageName);
		}
	}

	if (bRecurseModifications)
	{
		// Recurse modified packages to their dependencies. This is needed because we only compare package guids
		TArray<FName> ModifiedPackagesToRecurse = ModifiedPackages.Array();

		if (bRecurseScriptModifications)
		{
			ModifiedPackagesToRecurse.Append(ModifiedScriptPackages);
		}

		for (int32 RecurseIndex = 0; RecurseIndex < ModifiedPackagesToRecurse.Num(); RecurseIndex++)
		{
			FName ModifiedPackage = ModifiedPackagesToRecurse[RecurseIndex];
			TArray<FAssetIdentifier> Referencers;
			State.GetReferencers(ModifiedPackage, Referencers, EAssetRegistryDependencyType::Hard);

			for (const FAssetIdentifier& Referencer : Referencers)
			{
				FName ReferencerPackage = Referencer.PackageName;
				if (!ModifiedPackages.Contains(ReferencerPackage) && (IdenticalCookedPackages.Contains(ReferencerPackage) || IdenticalUncookedPackages.Contains(ReferencerPackage)))
				{
					// Remove from identical list
					IdenticalCookedPackages.Remove(ReferencerPackage);
					IdenticalUncookedPackages.Remove(ReferencerPackage);

					ModifiedPackages.Add(ReferencerPackage);
					ModifiedPackagesToRecurse.Add(ReferencerPackage);
				}
			}
		}
	}
}

void FAssetRegistryGenerator::BuildChunkManifest(const TSet<FName>& InCookedPackages, const TSet<FName>& InDevelopmentOnlyPackages, FSandboxPlatformFile* InSandboxFile, bool bGenerateStreamingInstallManifest)
{
	// If we were asked to generate a streaming install manifest explicitly we will generate chunks.
	// Otherwise, we will defer to the config settings for the platform.
	if (bGenerateStreamingInstallManifest)
	{
		bGenerateChunks = true;
	}
	else
	{
		bGenerateChunks = ShouldPlatformGenerateStreamingInstallManifest(TargetPlatform);
	}

	CookedPackages = InCookedPackages;
	DevelopmentOnlyPackages = InDevelopmentOnlyPackages;

	TSet<FName> AllPackages;
	AllPackages.Append(CookedPackages);
	AllPackages.Append(DevelopmentOnlyPackages);

	// Prune our asset registry to cooked + dev only list
	State.PruneAssetData(AllPackages, TSet<FName>());

	// Mark development only packages as explicitly -1 size to indicate it was not cooked
	for (FName DevelopmentOnlyPackage : DevelopmentOnlyPackages)
	{
		FAssetPackageData* PackageData = State.CreateOrGetAssetPackageData(DevelopmentOnlyPackage);
		PackageData->DiskSize = -1;
	}

	// initialize FoundIDList, PackageChunkIDMap
	const TMap<FName, const FAssetData*>& ObjectToDataMap = State.GetObjectPathToAssetDataMap();

	for (const TPair<FName, const FAssetData*>& Pair : ObjectToDataMap)
	{
		// Chunk Ids are safe to modify in place so do a const cast
		FAssetData& AssetData = *const_cast<FAssetData*>(Pair.Value);
		for (auto ChunkIt = AssetData.ChunkIDs.CreateConstIterator(); ChunkIt; ++ChunkIt)
		{
			int32 ChunkID = *ChunkIt;
			if (ChunkID < 0)
			{
				UE_LOG(LogAssetRegistryGenerator, Warning, TEXT("Out of range ChunkID: %d"), ChunkID);
				ChunkID = 0;
			}
			
			auto* FoundIDList = PackageChunkIDMap.Find(AssetData.PackageName);
			if (!FoundIDList)
			{
				FoundIDList = &PackageChunkIDMap.Add(AssetData.PackageName);
			}
			FoundIDList->AddUnique(ChunkID);
		}

		// Now clear the original chunk id list. We will fill it with real IDs when cooking.
		AssetData.ChunkIDs.Empty();

		// Update whether the owner package contains a map
		if (AssetData.GetClass()->IsChildOf(UWorld::StaticClass()) || AssetData.GetClass()->IsChildOf(ULevel::StaticClass()))
		{
			PackagesContainingMaps.Add(AssetData.PackageName);
		}
	}

	// add all the packages to the unassigned package list
	for (FName CookedPackage : CookedPackages)
	{
		const FString SandboxPath = InSandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*FPackageName::LongPackageNameToFilename(CookedPackage.ToString()));

		AllCookedPackageSet.Add(CookedPackage, SandboxPath);
		UnassignedPackageSet.Add(CookedPackage, SandboxPath);
	}

	TArray<FName> UnassignedPackageList;

	// Old path has map specific code, new code doesn't care about map or load order
	if (!bUseAssetManager)
	{
		// Assign startup packages, these will generally end up in chunk 0
		FString StartupPackageMapName(TEXT("None"));
		for (FName CookedPackage : StartupPackages)
		{
			const FString SandboxPath = InSandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*FPackageName::LongPackageNameToFilename(CookedPackage.ToString()));
			const FString PackagePathName = CookedPackage.ToString();
			AllCookedPackageSet.Add(CookedPackage, SandboxPath);
			GenerateChunkManifestForPackage(CookedPackage, PackagePathName, SandboxPath, StartupPackageMapName, InSandboxFile);
		}

		// Capture list at start as it may change during iteration
		UnassignedPackageSet.GenerateKeyArray(UnassignedPackageList);

		// assign chunks for all the map packages
		for (FName MapFName : UnassignedPackageList)
		{
			if (ContainsMap(MapFName) == false)
			{
				continue;
			}

			// get all the dependencies for this map
			TArray<FName> MapDependencies;
			ensure(GatherAllPackageDependencies(MapFName, MapDependencies));

			for (const auto& RawPackageFName : MapDependencies)
			{
				const FName PackageFName = GetPackageNameFromDependencyPackageName(RawPackageFName);

				if (PackageFName == NAME_None)
				{
					continue;
				}

				const FString PackagePathName = PackageFName.ToString();
				const FString MapName = MapFName.ToString();
				const FString* SandboxFilenamePtr = AllCookedPackageSet.Find(PackageFName);
				if (!SandboxFilenamePtr)
				{
					const FString SandboxPath = InSandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*FPackageName::LongPackageNameToFilename(PackagePathName));

					AllCookedPackageSet.Add(PackageFName, SandboxPath);

					SandboxFilenamePtr = AllCookedPackageSet.Find(PackageFName);
					check(SandboxFilenamePtr);
				}
				const FString& SandboxFilename = *SandboxFilenamePtr;

				GenerateChunkManifestForPackage(PackageFName, PackagePathName, SandboxFilename, MapName, InSandboxFile);
			}
		}
	}

	// Capture list at start as it may change during iteration
	UnassignedPackageSet.GenerateKeyArray(UnassignedPackageList);

	// process the remaining unassigned packages
	for (FName PackageFName : UnassignedPackageList)
	{
		const FString& SandboxFilename = AllCookedPackageSet.FindChecked(PackageFName);
		const FString PackagePathName = PackageFName.ToString();

		GenerateChunkManifestForPackage(PackageFName, PackagePathName, SandboxFilename, FString(), InSandboxFile);
	}

	// anything that remains in the UnAssignedPackageSet will be put in chunk0 when we save the asset registry

}

void FAssetRegistryGenerator::AddAssetToFileOrderRecursive(FAssetData* InAsset, TArray<FName>& OutFileOrder, TArray<FName>& OutEncounteredNames, const TMap<FName, FAssetData*>& InAssets, const TArray<FName>& InTopLevelAssets)
{
	if (!OutEncounteredNames.Contains(InAsset->PackageName))
	{
		OutEncounteredNames.Add(InAsset->PackageName);

		TArray<FName> Dependencies;
		AssetRegistry.GetDependencies(InAsset->PackageName, Dependencies, EAssetRegistryDependencyType::Hard);

		for (auto DependencyName : Dependencies)
		{
			if (InAssets.Contains(DependencyName) && !OutFileOrder.Contains(DependencyName))
			{
				if (!InTopLevelAssets.Contains(DependencyName))
				{
					auto Dependency = InAssets[DependencyName];
					AddAssetToFileOrderRecursive(Dependency, OutFileOrder, OutEncounteredNames, InAssets, InTopLevelAssets);
				}
			}
		}

		OutFileOrder.Add(InAsset->PackageName);
	}
}

bool FAssetRegistryGenerator::SaveAssetRegistry(const FString& SandboxPath, bool bSerializeDevelopmentAssetRegistry )
{
	UE_LOG(LogAssetRegistryGenerator, Display, TEXT("Saving asset registry."));
	const TMap<FName, const FAssetData*>& ObjectToDataMap = State.GetObjectPathToAssetDataMap();

	// Write development first, this will always write
	FAssetRegistrySerializationOptions DevelopmentSaveOptions;
	AssetRegistry.InitializeSerializationOptions(DevelopmentSaveOptions, TargetPlatform->IniPlatformName());
	DevelopmentSaveOptions.ModifyForDevelopment();

	// Write runtime registry, this can be excluded per game/platform
	FAssetRegistrySerializationOptions SaveOptions;
	AssetRegistry.InitializeSerializationOptions(SaveOptions, TargetPlatform->IniPlatformName());

	// Flush the asset registry and make sure the asset data is in sync, as it may have been updated during cook
	AssetRegistry.Tick(-1.0f);

	AssetRegistry.InitializeTemporaryAssetRegistryState(State, SaveOptions, true);

	if (DevelopmentSaveOptions.bSerializeAssetRegistry && bSerializeDevelopmentAssetRegistry)
	{
		// Create development registry data, used for incremental cook and editor viewing
		FArrayWriter SerializedAssetRegistry;

		State.Serialize(SerializedAssetRegistry, DevelopmentSaveOptions);

		// Save the generated registry
		FString PlatformSandboxPath = SandboxPath.Replace(TEXT("[Platform]"), *TargetPlatform->PlatformName());
		PlatformSandboxPath.ReplaceInline(TEXT("AssetRegistry.bin"), TEXT("DevelopmentAssetRegistry.bin"));
		FFileHelper::SaveArrayToFile(SerializedAssetRegistry, *PlatformSandboxPath);
	}

	if (SaveOptions.bSerializeAssetRegistry)
	{
		// Prune out the development only packages
		State.PruneAssetData(CookedPackages, TSet<FName>(), SaveOptions.bFilterAssetDataWithNoTags);

		// Create runtime registry data
		FArrayWriter SerializedAssetRegistry;
		SerializedAssetRegistry.SetFilterEditorOnly(true);

		State.Serialize(SerializedAssetRegistry, SaveOptions);

		// Save the generated registry
		FString PlatformSandboxPath = SandboxPath.Replace(TEXT("[Platform]"), *TargetPlatform->PlatformName());
		FFileHelper::SaveArrayToFile(SerializedAssetRegistry, *PlatformSandboxPath);
		UE_LOG(LogAssetRegistryGenerator, Display, TEXT("Generated asset registry num assets %d, size is %5.2fkb"), ObjectToDataMap.Num(), (float)SerializedAssetRegistry.Num() / 1024.f);
	}

	UE_LOG(LogAssetRegistryGenerator, Display, TEXT("Done saving asset registry."));

	return true;
}

bool FAssetRegistryGenerator::WriteCookerOpenOrder()
{
	TMap<FName, FAssetData*> PackageNameToDataMap;
	TArray<FName> MapList;
	const TMap<FName, const FAssetData*>& ObjectToDataMap = State.GetObjectPathToAssetDataMap();
	for (const TPair<FName, const FAssetData*>& Pair : ObjectToDataMap)
	{
		FAssetData* AssetData = const_cast<FAssetData*>(Pair.Value);
		PackageNameToDataMap.Add(AssetData->PackageName, AssetData);

		// REPLACE WITH PRIORITY

		if (ContainsMap(AssetData->PackageName))
		{
			MapList.Add(AssetData->PackageName);
		}
	}

	FString CookerFileOrderString = CreateCookerFileOrderString(PackageNameToDataMap, MapList);

	if (CookerFileOrderString.Len())
	{
		auto OpenOrderFilename = FString::Printf(TEXT("%sBuild/%s/FileOpenOrder/CookerOpenOrder.log"), *FPaths::ProjectDir(), *TargetPlatform->PlatformName());
		FFileHelper::SaveStringToFile(CookerFileOrderString, *OpenOrderFilename);
	}

	return true;
}

/** Helper function which reroots a sandbox path to the staging area directory which UnrealPak expects */
inline void ConvertFilenameToPakFormat(FString& InOutPath)
{
	auto ProjectDir = FPaths::ProjectDir();
	auto EngineDir = FPaths::EngineDir();
	auto GameName = FApp::GetProjectName();

	if (InOutPath.Contains(ProjectDir))
	{
		FPaths::MakePathRelativeTo(InOutPath, *ProjectDir);
		InOutPath = FString::Printf(TEXT("../../../%s/%s"), GameName, *InOutPath);
	}
	else if (InOutPath.Contains(EngineDir))
	{
		FPaths::MakePathRelativeTo(InOutPath, *EngineDir);
		InOutPath = FPaths::Combine(TEXT("../../../Engine/"), *InOutPath);
	}
}

FString FAssetRegistryGenerator::CreateCookerFileOrderString(const TMap<FName, FAssetData*>& InAssetData, const TArray<FName>& InTopLevelAssets)
{
	FString FileOrderString;
	TArray<FAssetData*> TopLevelMapNodes;
	TArray<FAssetData*> TopLevelNodes;

	for (auto Asset : InAssetData)
	{
		auto PackageName = Asset.Value->PackageName;
		TArray<FName> Referencers;
		AssetRegistry.GetReferencers(PackageName, Referencers);

		bool bIsTopLevel = true;
		bool bIsMap = InTopLevelAssets.Contains(PackageName);

		if (!bIsMap && Referencers.Num() > 0)
		{
			for (auto ReferencerName : Referencers)
			{
				if (InAssetData.Contains(ReferencerName))
				{
					bIsTopLevel = false;
					break;
				}
			}
		}

		if (bIsTopLevel)
		{
			if (bIsMap)
			{
				TopLevelMapNodes.Add(Asset.Value);
			}
			else
			{
				TopLevelNodes.Add(Asset.Value);
			}
		}
	}

	TopLevelMapNodes.Sort([&InTopLevelAssets](const FAssetData& A, const FAssetData& B)
	{
		auto IndexA = InTopLevelAssets.Find(A.PackageName);
		auto IndexB = InTopLevelAssets.Find(B.PackageName);
		return IndexA < IndexB;
	});

	TArray<FName> FileOrder;
	TArray<FName> EncounteredNames;
	for (auto Asset : TopLevelNodes)
	{
		AddAssetToFileOrderRecursive(Asset, FileOrder, EncounteredNames, InAssetData, InTopLevelAssets);
	}

	for (auto Asset : TopLevelMapNodes)
	{
		AddAssetToFileOrderRecursive(Asset, FileOrder, EncounteredNames, InAssetData, InTopLevelAssets);
	}

	int32 CurrentIndex = 0;
	for (auto PackageName : FileOrder)
	{
		auto Asset = InAssetData[PackageName];
		bool bIsMap = InTopLevelAssets.Contains(Asset->PackageName);
		auto Filename = FPackageName::LongPackageNameToFilename(Asset->PackageName.ToString(), bIsMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());

		ConvertFilenameToPakFormat(Filename);
		auto Line = FString::Printf(TEXT("\"%s\" %i\n"), *Filename, CurrentIndex++);
		FileOrderString.Append(Line);
	}

	return FileOrderString;
}

bool FAssetRegistryGenerator::GetPackageDependencyChain(FName SourcePackage, FName TargetPackage, TSet<FName>& VisitedPackages, TArray<FName>& OutDependencyChain)
{	
	//avoid crashing from circular dependencies.
	if (VisitedPackages.Contains(SourcePackage))
	{		
		return false;
	}
	VisitedPackages.Add(SourcePackage);

	if (SourcePackage == TargetPackage)
	{		
		OutDependencyChain.Add(SourcePackage);
		return true;
	}

	TArray<FName> SourceDependencies;
	if (GetPackageDependencies(SourcePackage, SourceDependencies, DependencyType) == false)
	{		
		return false;
	}

	int32 DependencyCounter = 0;
	while (DependencyCounter < SourceDependencies.Num())
	{		
		const FName& ChildPackageName = SourceDependencies[DependencyCounter];
		if (GetPackageDependencyChain(ChildPackageName, TargetPackage, VisitedPackages, OutDependencyChain))
		{
			OutDependencyChain.Add(SourcePackage);
			return true;
		}
		++DependencyCounter;
	}
	
	return false;
}

bool FAssetRegistryGenerator::GetPackageDependencies(FName PackageName, TArray<FName>& DependentPackageNames, EAssetRegistryDependencyType::Type InDependencyType)
{	
	if (FGameDelegates::Get().GetGetPackageDependenciesForManifestGeneratorDelegate().IsBound())
	{
		return FGameDelegates::Get().GetGetPackageDependenciesForManifestGeneratorDelegate().Execute(PackageName, DependentPackageNames, InDependencyType);
	}
	else
	{
		return AssetRegistry.GetDependencies(PackageName, DependentPackageNames, InDependencyType);
	}
}

bool FAssetRegistryGenerator::GatherAllPackageDependencies(FName PackageName, TArray<FName>& DependentPackageNames)
{	
	if (GetPackageDependencies(PackageName, DependentPackageNames, DependencyType) == false)
	{
		return false;
	}

	TSet<FName> VisitedPackages;
	VisitedPackages.Append(DependentPackageNames);

	int32 DependencyCounter = 0;
	while (DependencyCounter < DependentPackageNames.Num())
	{
		const FName& ChildPackageName = DependentPackageNames[DependencyCounter];
		++DependencyCounter;
		TArray<FName> ChildDependentPackageNames;
		if (GetPackageDependencies(ChildPackageName, ChildDependentPackageNames, DependencyType) == false)
		{
			return false;
		}

		for (const auto& ChildDependentPackageName : ChildDependentPackageNames)
		{
			if (!VisitedPackages.Contains(ChildDependentPackageName))
			{
				DependentPackageNames.Add(ChildDependentPackageName);
				VisitedPackages.Add(ChildDependentPackageName);
			}
		}
	}

	return true;
}

bool FAssetRegistryGenerator::GenerateAssetChunkInformationCSV(const FString& OutputPath)
{
	FString TmpString;
	FString CSVString;
	FString HeaderText(TEXT("ChunkID, Package Name, Class Type, Hard or Soft Chunk, File Size, Other Chunks\n"));
	FString EndLine(TEXT("\n"));
	FString NoneText(TEXT("None\n"));
	CSVString = HeaderText;

	const TMap<FName, const FAssetData*>& ObjectToDataMap = State.GetObjectPathToAssetDataMap();
	for (int32 ChunkID = 0, ChunkNum = FinalChunkManifests.Num(); ChunkID < ChunkNum; ++ChunkID)
	{
		FString PerChunkManifestCSV = HeaderText;
		for (const TPair<FName, const FAssetData*> Pair : ObjectToDataMap)
		{
			const FAssetData& AssetData = *Pair.Value;
			// Add only assets that have actually been cooked and belong to any chunk
			if (AssetData.ChunkIDs.Num() > 0)
			{
				FString Fullname;
				if (AssetData.ChunkIDs.Contains(ChunkID) && FPackageName::DoesPackageExist(*AssetData.PackageName.ToString(), nullptr, &Fullname))
				{
					auto FileSize = IFileManager::Get().FileSize(*FPackageName::LongPackageNameToFilename(*AssetData.PackageName.ToString(), FPackageName::GetAssetPackageExtension()));
					if (FileSize == INDEX_NONE)
					{
						FileSize = IFileManager::Get().FileSize(*FPackageName::LongPackageNameToFilename(*AssetData.PackageName.ToString(), FPackageName::GetMapPackageExtension()));
					}

					if (FileSize == INDEX_NONE)
					{
						FileSize = 0;
					}

					FString SoftChain;
					bool bHardChunk = false;
					if (ChunkID < ChunkManifests.Num())
					{
						bHardChunk = ChunkManifests[ChunkID] && ChunkManifests[ChunkID]->Contains(AssetData.PackageName);
					
						if (!bHardChunk)
						{
							//
							SoftChain = GetShortestReferenceChain(AssetData.PackageName, ChunkID);
						}
					}
					if (SoftChain.IsEmpty())
					{
						SoftChain = TEXT("Soft: Possibly Unassigned Asset");
					}

					TmpString = FString::Printf(TEXT("%d,%s,%s,%s,%lld,"), ChunkID, *AssetData.PackageName.ToString(), *AssetData.AssetClass.ToString(), bHardChunk ? TEXT("Hard") : *SoftChain, FileSize);
					CSVString += TmpString;
					PerChunkManifestCSV += TmpString;
					if (AssetData.ChunkIDs.Num() == 1)
					{
						CSVString += NoneText;
						PerChunkManifestCSV += NoneText;
					}
					else
					{
						for (const auto& OtherChunk : AssetData.ChunkIDs)
						{
							if (OtherChunk != ChunkID)
							{
								TmpString = FString::Printf(TEXT("%d "), OtherChunk);
								CSVString += TmpString;
								PerChunkManifestCSV += TmpString;
							}
						}
						CSVString += EndLine;
						PerChunkManifestCSV += EndLine;
					}
				}
			}
		}

		FFileHelper::SaveStringToFile(PerChunkManifestCSV, *FPaths::Combine(*OutputPath, *FString::Printf(TEXT("Chunks%dInfo.csv"), ChunkID)));
	}

	return FFileHelper::SaveStringToFile(CSVString, *FPaths::Combine(*OutputPath, TEXT("AllChunksInfo.csv")));
}

void FAssetRegistryGenerator::AddPackageToManifest(const FString& PackageSandboxPath, FName PackageName, int32 ChunkId)
{
	while (ChunkId >= ChunkManifests.Num())
	{
		ChunkManifests.Add(nullptr);
	}
	if (!ChunkManifests[ChunkId])
	{
		ChunkManifests[ChunkId] = new FChunkPackageSet();
	}
	ChunkManifests[ChunkId]->Add(PackageName, PackageSandboxPath);
	//Safety check, it the package happens to exist in the unassigned list remove it now.
	UnassignedPackageSet.Remove(PackageName);
}


void FAssetRegistryGenerator::RemovePackageFromManifest(FName PackageName, int32 ChunkId)
{
	if (ChunkManifests[ChunkId])
	{
		ChunkManifests[ChunkId]->Remove(PackageName);
	}
}

void FAssetRegistryGenerator::ResolveChunkDependencyGraph(const FChunkDependencyTreeNode& Node, FChunkPackageSet BaseAssetSet, TArray<TArray<FName>>& OutPackagesMovedBetweenChunks)
{
	if (FinalChunkManifests.Num() > Node.ChunkID && FinalChunkManifests[Node.ChunkID])
	{
		for (auto It = BaseAssetSet.CreateConstIterator(); It; ++It)
		{
			// Remove any assets belonging to our parents.			
			if (FinalChunkManifests[Node.ChunkID]->Remove(It.Key()) > 0)
			{
				OutPackagesMovedBetweenChunks[Node.ChunkID].Add(It.Key());
				UE_LOG(LogAssetRegistryGenerator, Verbose, TEXT("Removed %s from chunk %i because it is duplicated in another chunk."), *It.Key().ToString(), Node.ChunkID);
			}
		}
		// Add the current Chunk's assets
		for (auto It = FinalChunkManifests[Node.ChunkID]->CreateConstIterator(); It; ++It)//for (const auto It : *(FinalChunkManifests[Node.ChunkID]))
		{
			BaseAssetSet.Add(It.Key(), It.Value());
		}
		for (const auto It : Node.ChildNodes)
		{
			ResolveChunkDependencyGraph(It, BaseAssetSet, OutPackagesMovedBetweenChunks);
		}
	}
}

bool FAssetRegistryGenerator::CheckChunkAssetsAreNotInChild(const FChunkDependencyTreeNode& Node)
{
	for (const auto It : Node.ChildNodes)
	{
		if (!CheckChunkAssetsAreNotInChild(It))
		{
			return false;
		}
	}

	if (!(FinalChunkManifests.Num() > Node.ChunkID && FinalChunkManifests[Node.ChunkID]))
	{
		return true;
	}

	for (const auto ChildIt : Node.ChildNodes)
	{
		if(FinalChunkManifests.Num() > ChildIt.ChunkID && FinalChunkManifests[ChildIt.ChunkID])
		{
			for (auto It = FinalChunkManifests[Node.ChunkID]->CreateConstIterator(); It; ++It)
			{
				if (FinalChunkManifests[ChildIt.ChunkID]->Find(It.Key()))
				{
					return false;
				}
			}
		}
	}

	return true;
}

void FAssetRegistryGenerator::AddPackageAndDependenciesToChunk(FChunkPackageSet* ThisPackageSet, FName InPkgName, const FString& InSandboxFile, int32 ChunkID, FSandboxPlatformFile* SandboxPlatformFile)
{
	FChunkPackageSet* InitialPackageSetForThisChunk = ChunkManifests.IsValidIndex(ChunkID) ? ChunkManifests[ChunkID] : nullptr;

	//Add this asset
	ThisPackageSet->Add(InPkgName, InSandboxFile);

	// Only gather dependencies the slow way if we're chunking or not using asset manager
	if (!bGenerateChunks || bUseAssetManager)
	{
		return;
	}

	//now add any dependencies
	TArray<FName> DependentPackageNames;
	if (GatherAllPackageDependencies(InPkgName, DependentPackageNames))
	{
		for (const auto& PkgName : DependentPackageNames)
		{
			bool bSkip = false;
			if (ChunkID != 0 && FinalChunkManifests[0])
			{
				// Do not add if this asset was assigned to the 0 chunk. These assets always exist on disk
				bSkip = FinalChunkManifests[0]->Contains(PkgName);
			}
			if (!bSkip)
			{
				const FName FilteredPackageName = GetPackageNameFromDependencyPackageName(PkgName);
				if (FilteredPackageName == NAME_None)
				{
					continue;
				}
				FString DependentSandboxFile = SandboxPlatformFile->ConvertToAbsolutePathForExternalAppForWrite(*FPackageName::LongPackageNameToFilename(*FilteredPackageName.ToString()));
				if (!ThisPackageSet->Contains(FilteredPackageName))
				{
					if ((InitialPackageSetForThisChunk != nullptr) && InitialPackageSetForThisChunk->Contains(PkgName))
					{
						// Don't print anything out; it was pre-assigned to this chunk but we haven't gotten to it yet in the calling loop; we'll go ahead and grab it now
					}
					else
					{
						if (UE_LOG_ACTIVE(LogAssetRegistryGenerator, Verbose))
						{
							// It was not assigned to this chunk and we're forcing it to be dragged in, let the user known
							UE_LOG(LogAssetRegistryGenerator, Verbose, TEXT("Adding %s to chunk %i because %s depends on it."), *FilteredPackageName.ToString(), ChunkID, *InPkgName.ToString());

							TSet<FName> VisitedPackages;
							TArray<FName> DependencyChain;
							GetPackageDependencyChain(InPkgName, PkgName, VisitedPackages, DependencyChain);
							for (const auto& ChainName : DependencyChain)
							{
								UE_LOG(LogAssetRegistryGenerator, Verbose, TEXT("\tchain: %s"), *ChainName.ToString());
							}
						}
					}
				}
				ThisPackageSet->Add(FilteredPackageName, DependentSandboxFile);
				UnassignedPackageSet.Remove(PkgName);
			}
		}
	}
}

void FAssetRegistryGenerator::FixupPackageDependenciesForChunks(FSandboxPlatformFile* InSandboxFile)
{
	UE_LOG(LogAssetRegistryGenerator, Log, TEXT("Starting FixupPackageDependenciesForChunks..."));
	SCOPE_LOG_TIME_IN_SECONDS(TEXT("... FixupPackageDependenciesForChunks complete."), nullptr);

	for (int32 ChunkID = 0, MaxChunk = ChunkManifests.Num(); ChunkID < MaxChunk; ++ChunkID)
	{
		FinalChunkManifests.Add(nullptr);
		if (!ChunkManifests[ChunkID])
		{
			continue;
		}
		FinalChunkManifests[ChunkID] = new FChunkPackageSet();
		for (auto It = ChunkManifests[ChunkID]->CreateConstIterator(); It; ++It)
		{
			AddPackageAndDependenciesToChunk(FinalChunkManifests[ChunkID], It.Key(), It.Value(), ChunkID, InSandboxFile);
		}
	}

	const FChunkDependencyTreeNode* ChunkDepGraph = DependencyInfo->GetOrBuildChunkDependencyGraph(ChunkManifests.Num() - 1);
	// Once complete, Add any remaining assets (that are not assigned to a chunk) to the first chunk.
	if (FinalChunkManifests.Num() == 0)
	{
		FinalChunkManifests.Add(nullptr);
	}
	if (!FinalChunkManifests[0])
	{
		FinalChunkManifests[0] = new FChunkPackageSet();
	}
	// Copy the remaining assets
	auto RemainingAssets = UnassignedPackageSet;
	for (auto It = RemainingAssets.CreateConstIterator(); It; ++It)
	{
		AddPackageAndDependenciesToChunk(FinalChunkManifests[0], It.Key(), It.Value(), 0, InSandboxFile);
	}

	if (!CheckChunkAssetsAreNotInChild(*ChunkDepGraph))
	{
		UE_LOG(LogAssetRegistryGenerator, Log, TEXT("Initial scan of chunks found duplicate assets in graph children"));
	}
		
	TArray<TArray<FName>> PackagesRemovedFromChunks;
	PackagesRemovedFromChunks.AddDefaulted(ChunkManifests.Num());

	//Finally, if the previous step may added any extra packages to the 0 chunk. Pull them out of other chunks and save space
	ResolveChunkDependencyGraph(*ChunkDepGraph, FChunkPackageSet(), PackagesRemovedFromChunks);

	for (int32 i = 0; i < ChunkManifests.Num(); ++i)
	{
		if (!bUseAssetManager)
		{
			FName CollectionName(*FString::Printf(TEXT("PackagesRemovedFromChunk%i"), i));
			if (CreateOrEmptyCollection(CollectionName))
			{
				WriteCollection(CollectionName, PackagesRemovedFromChunks[i]);
			}
		}
	}

	for (int32 ChunkID = 0, MaxChunk = ChunkManifests.Num(); ChunkID < MaxChunk; ++ChunkID)
	{
		const int32 ChunkManifestNum = ChunkManifests[ChunkID] ? ChunkManifests[ChunkID]->Num() : 0;
		const int32 FinalChunkManifestNum = FinalChunkManifests[ChunkID] ? FinalChunkManifests[ChunkID]->Num() : 0;
		UE_LOG(LogAssetRegistryGenerator, Log, TEXT("Chunk: %i, Started with %i packages, Final after dependency resolve: %i"), ChunkID, ChunkManifestNum, FinalChunkManifestNum);
	}
	
	// Fix up the asset registry to reflect this chunk layout
	for (int32 ChunkID = 0, MaxChunk = FinalChunkManifests.Num(); ChunkID < MaxChunk; ++ChunkID)
	{
		if (!FinalChunkManifests[ChunkID])
		{
			continue;
		}
		for (const TPair<FName, FString>& Asset : *FinalChunkManifests[ChunkID])
		{
			const TArray<const FAssetData*> AssetIndexArray = State.GetAssetsByPackageName(Asset.Key);
			for (const FAssetData* AssetData : AssetIndexArray)
			{
				// Chunk Ids are safe to modify in place
				const_cast<FAssetData*>(AssetData)->ChunkIDs.AddUnique(ChunkID);
			}
		}
	}
}


void FAssetRegistryGenerator::FindShortestReferenceChain(TArray<FReferencePair> PackageNames, int32 ChunkID, uint32& OutParentIndex, FString& OutChainPath)
{
	TArray<FReferencePair> ReferencesToCheck;
	uint32 Index = 0;
	for (const auto& Pkg : PackageNames)
	{
		if (ChunkManifests[ChunkID] && ChunkManifests[ChunkID]->Contains(Pkg.PackageName))
		{
			OutChainPath += TEXT("Soft: ");
			OutChainPath += Pkg.PackageName.ToString();
			OutParentIndex = Pkg.ParentNodeIndex;
			return;
		}
		TArray<FName> AssetReferences;
		AssetRegistry.GetReferencers(Pkg.PackageName, AssetReferences);
		for (const auto& Ref : AssetReferences)
		{
			if (!InspectedNames.Contains(Ref))
			{
				ReferencesToCheck.Add(FReferencePair(Ref, Index));
				InspectedNames.Add(Ref);
			}
		}

		++Index;
	}

	if (ReferencesToCheck.Num() > 0)
	{
		uint32 ParentIndex = INDEX_NONE;
		FindShortestReferenceChain(ReferencesToCheck, ChunkID, ParentIndex, OutChainPath);

		if (ParentIndex < (uint32)PackageNames.Num())
		{
			OutChainPath += TEXT("->");
			OutChainPath += PackageNames[ParentIndex].PackageName.ToString();
			OutParentIndex = PackageNames[ParentIndex].ParentNodeIndex;
		}
	}
	else if (PackageNames.Num() > 0)
	{
		//best guess
		OutChainPath += TEXT("Soft From Unassigned Package? Best Guess: ");
		OutChainPath += PackageNames[0].PackageName.ToString();
		OutParentIndex = PackageNames[0].ParentNodeIndex;
	}
}

FString FAssetRegistryGenerator::GetShortestReferenceChain(FName PackageName, int32 ChunkID)
{
	FString StringChain;
	TArray<FReferencePair> ReferencesToCheck;
	uint32 ParentIndex;
	ReferencesToCheck.Add(FReferencePair(PackageName, 0));
	InspectedNames.Empty();
	InspectedNames.Add(PackageName);
	FindShortestReferenceChain(ReferencesToCheck, ChunkID, ParentIndex, StringChain);

	return StringChain;
}


bool FAssetRegistryGenerator::CreateOrEmptyCollection(FName CollectionName)
{
	ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();

	if (CollectionManager.CollectionExists(CollectionName, ECollectionShareType::CST_Local))
	{
		return CollectionManager.EmptyCollection(CollectionName, ECollectionShareType::CST_Local);
	}
	else if (CollectionManager.CreateCollection(CollectionName, ECollectionShareType::CST_Local, ECollectionStorageMode::Static))
	{
		return true;
	}

	return false;
}

void FAssetRegistryGenerator::WriteCollection(FName CollectionName, const TArray<FName>& PackageNames)
{
	if (CreateOrEmptyCollection(CollectionName))
	{
		TArray<FName> AssetNames = PackageNames;

		// Convert package names to asset names
		for (FName& Name : AssetNames)
		{
			FString PackageName = Name.ToString();
			int32 LastPathDelimiter;
			if (PackageName.FindLastChar(TEXT('/'), /*out*/ LastPathDelimiter))
			{
				const FString AssetName = PackageName.Mid(LastPathDelimiter + 1);
				PackageName = PackageName + FString(TEXT(".")) + AssetName;
				Name = *PackageName;
			}
		}

		ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();
		CollectionManager.AddToCollection(CollectionName, ECollectionShareType::CST_Local, AssetNames);

		UE_LOG(LogAssetRegistryGenerator, Log, TEXT("Updated collection %s"), *CollectionName.ToString());
	}
	else
	{
		UE_LOG(LogAssetRegistryGenerator, Warning, TEXT("Failed to update collection %s"), *CollectionName.ToString());
	}
}

#undef LOCTEXT_NAMESPACE
