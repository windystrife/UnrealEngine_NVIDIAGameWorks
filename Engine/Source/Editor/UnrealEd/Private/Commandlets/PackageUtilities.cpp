// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PackageUtilities.cpp: Commandlets for viewing information about package files
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ObjectThumbnail.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Serialization/ArchiveCountMem.h"
#include "Misc/PackageName.h"
#include "UObject/ObjectResource.h"
#include "UObject/LinkerLoad.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Commandlets/Commandlet.h"
#include "Commandlets/CompressAnimationsCommandlet.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "SourceControlHelpers.h"
#include "Commandlets/LoadPackageCommandlet.h"
#include "Commandlets/PkgInfoCommandlet.h"
#include "Commandlets/ReplaceActorCommandlet.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "GameFramework/WorldSettings.h"
#include "Editor.h"
#include "FileHelpers.h"

#include "Animation/AnimCompress_BitwiseCompressOnly.h"
#include "Animation/AnimCompress_Automatic.h"


#include "CollectionManagerTypes.h"
#include "ICollectionManager.h"
#include "CollectionManagerModule.h"
#include "PackageHelperFunctions.h"
#include "PackageUtilityWorkers.h"

#include "AnimationCompression.h"
#include "Animation/AnimationSettings.h"

#include "EngineUtils.h"

DEFINE_LOG_CATEGORY(LogPackageHelperFunctions);
DEFINE_LOG_CATEGORY_STATIC(LogPackageUtilities, Log, All);

/*-----------------------------------------------------------------------------
	Package Helper Functions (defined in PackageHelperFunctions.h
-----------------------------------------------------------------------------*/

void SearchDirectoryRecursive( const FString& SearchPathMask, TArray<FString>& out_PackageNames, TArray<FString>& out_PackageFilenames )
{
	const FString SearchPath = FPaths::GetPath(SearchPathMask);
	TArray<FString> PackageNames;
	IFileManager::Get().FindFiles( PackageNames, *SearchPathMask, true, false );
	if ( PackageNames.Num() > 0 )
	{
		for ( int32 PkgIndex = 0; PkgIndex < PackageNames.Num(); PkgIndex++ )
		{
			new(out_PackageFilenames) FString( SearchPath / PackageNames[PkgIndex] );
		}

		out_PackageNames += PackageNames;
	}

	// now search all subdirectories
	TArray<FString> Subdirectories;
	IFileManager::Get().FindFiles( Subdirectories, *(SearchPath / TEXT("*")), false, true );
	for ( int32 DirIndex = 0; DirIndex < Subdirectories.Num(); DirIndex++ )
	{
		SearchDirectoryRecursive( SearchPath / Subdirectories[DirIndex] / FPaths::GetCleanFilename(SearchPathMask), out_PackageNames, out_PackageFilenames);
	}
}

/**
* Takes an array of package names (in any format) and converts them into relative pathnames for each package.
*
* @param	PackageNames		the array of package names to normalize.  If this array is empty, the complete package list will be used.
* @param	PackagePathNames	will be filled with the complete relative path name for each package name in the input array
* @param	PackageWildcard		if specified, allows the caller to specify a wildcard to use for finding package files
* @param	PackageFilter		allows the caller to limit the types of packages returned.
*
* @return	true if packages were found successfully, false otherwise.
*/
bool NormalizePackageNames( TArray<FString> PackageNames, TArray<FString>& PackagePathNames, const FString& PackageWildcard, uint8 PackageFilter )
{
	if ( PackageNames.Num() == 0 )
	{
		IFileManager::Get().FindFiles( PackageNames, *PackageWildcard, true, false );
	}

	const FString DeveloperFolder = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::GameDevelopersDir());

	if( PackageNames.Num() == 0 )
	{
		TArray<FString> Paths;
		if ( GConfig->GetArray( TEXT("Core.System"), TEXT("Paths"), Paths, GEngineIni ) > 0 )
		{
			for ( int32 i = 0; i < Paths.Num(); i++ )
			{
				FString SearchWildcard = Paths[i] / PackageWildcard;
				UE_LOG(LogPackageUtilities, Log, TEXT("Searching using wildcard: '%s'"), *SearchWildcard);
				SearchDirectoryRecursive( SearchWildcard, PackageNames, PackagePathNames );
			}
		}

		if ( PackageNames.Num() == 0 )
		{
			// Check if long package name is provided and if it exists on disk.
			FString Filename;
			if ( FPackageName::IsValidLongPackageName(PackageWildcard, true) && FPackageName::DoesPackageExist(PackageWildcard, NULL, &Filename) )
			{
				PackagePathNames.Add(Filename);
			}
		}
	}
	else
	{
		// re-add the path information so that GetPackageLinker finds the correct version of the file.
		const FString WildcardPath = FPaths::GetPath(PackageWildcard);
		for ( int32 FileIndex = 0; FileIndex < PackageNames.Num(); FileIndex++ )
		{
			PackagePathNames.Add(WildcardPath / PackageNames[FileIndex]);
		}
	}

	if ( PackagePathNames.Num() == 0 )
	{
		UE_LOG(LogPackageUtilities, Log, TEXT("No packages found using '%s'!"), *PackageWildcard);
		return false;
	}

	// now apply any filters to the list of packages
	for ( int32 PackageIndex = PackagePathNames.Num() - 1; PackageIndex >= 0; PackageIndex-- )
	{
		FString PackageExtension = FPaths::GetExtension(PackagePathNames[PackageIndex], true);
		if ( !FPackageName::IsPackageExtension(*PackageExtension) )
		{
			// not a valid package file - remove it
			PackagePathNames.RemoveAt(PackageIndex);
		}
		else
		{
			if ( (PackageFilter&NORMALIZE_ExcludeMapPackages) != 0 )
			{
				if ( PackageExtension == FPackageName::GetMapPackageExtension() )
				{
					PackagePathNames.RemoveAt(PackageIndex);
					continue;
				}
			}

			if ( (PackageFilter&NORMALIZE_ExcludeContentPackages) != 0 )
			{
				if ( PackageExtension == FPackageName::GetAssetPackageExtension() )
				{
					PackagePathNames.RemoveAt(PackageIndex);
					continue;
				}
			}

			if ( (PackageFilter&NORMALIZE_ExcludeEnginePackages) != 0 )
			{
				if (PackagePathNames[PackageIndex].StartsWith(FPaths::EngineDir()))
				{
					PackagePathNames.RemoveAt(PackageIndex);
					continue;
				}
			}

			FString Filename = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*PackagePathNames[PackageIndex]);
			
			if ( (PackageFilter&NORMALIZE_ExcludeDeveloperPackages) != 0 )
			{
				if (Filename.StartsWith(DeveloperFolder))
				{
					PackagePathNames.RemoveAt(PackageIndex);
					continue;
				}
			}
			else if ( (PackageFilter&NORMALIZE_ExcludeNonDeveloperPackages) != 0 )
			{
				if (!Filename.StartsWith(DeveloperFolder))
				{
					PackagePathNames.RemoveAt(PackageIndex);
					continue;
				}
			}

			if ( (PackageFilter&NORMALIZE_ExcludeNoRedistPackages) != 0 )
			{
				if(PackagePathNames.Contains("/NoRedist/") || PackagePathNames.Contains("/NotForLicensees/") || PackagePathNames.Contains("/EpicInternal/"))
				{
					PackagePathNames.RemoveAt(PackageIndex);
					continue;
				}
			}
		}
	}

	if ( (PackageFilter&NORMALIZE_ResetExistingLoaders) != 0 )
	{
		// reset the loaders for the packages we want to load so that we don't find the wrong version of the file
		for ( int32 PackageIndex = 0; PackageIndex < PackagePathNames.Num(); PackageIndex++ )
		{
			// (otherwise, attempting to run a commandlet on e.g. Engine.xxx will always return results for Engine.u instead)
			const FString& PackageName = PackagePathNames[PackageIndex];
			UPackage* ExistingPackage = FindObject<UPackage>(NULL, *PackageName, true);
			if ( ExistingPackage != NULL )
			{
				ResetLoaders(ExistingPackage);
			}
		}
	}

	return true;
}


/** 
* Helper function to save a package that may or may not be a map package
*
* @param	Package		The package to save
* @param	Filename	The location to save the package to
* @param	KeepObjectFlags	Objects with any these flags will be kept when saving even if unreferenced.
* @param	ErrorDevice	the output device to use for warning and error messages
* @param	LinkerToConformAgainst
* @param				optional linker to use as a base when saving Package; if specified, all common names, imports and exports
*						in Package will be sorted in the same order as the corresponding entries in the LinkerToConformAgainst

* @return true if successful
*/
bool SavePackageHelper(UPackage* Package, FString Filename, EObjectFlags KeepObjectFlags, FOutputDevice* ErrorDevice, FLinkerLoad* LinkerToConformAgainst, ESaveFlags SaveFlags)
{
	// look for a world object in the package (if there is one, there's a map)
	UWorld* World = UWorld::FindWorldInPackage(Package);
	return GEditor->SavePackage(Package, World, KeepObjectFlags, *Filename, ErrorDevice, LinkerToConformAgainst, false, true, SaveFlags);
}

/**
 * Policy that marks Asset Sets via the CollectionManager module
 */
class FCollectionPolicy
{
public:
	static bool CreateAssetSet(FName InSetName, ECollectionShareType::Type InSetType)
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		return CollectionManagerModule.Get().CreateCollection(InSetName, InSetType, ECollectionStorageMode::Static);
	}

	static bool DestroyAssetSet(FName InSetName, ECollectionShareType::Type InSetType )
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		return CollectionManagerModule.Get().DestroyCollection(InSetName, InSetType);
	}

	static bool RemoveAssetsFromSet(FName InSetName, ECollectionShareType::Type InSetType, const TArray<FName>& InAssetPathNames )
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		return CollectionManagerModule.Get().RemoveFromCollection(InSetName, InSetType, InAssetPathNames);
	}

	static bool AddAssetsToSet(FName InSetName, ECollectionShareType::Type InSetType, const TArray<FName>& InAssetPathNames )
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		return CollectionManagerModule.Get().AddToCollection(InSetName, InSetType, InAssetPathNames);
	}

	static bool QueryAssetsInSet(FName InSetName, ECollectionShareType::Type InSetType, TArray<FName>& OutAssetPathNames )
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		return CollectionManagerModule.Get().GetAssetsInCollection(InSetName, InSetType, OutAssetPathNames);
	}
};

template <class AssetSetPolicy>	
bool FContentHelper::CreateAssetSet(FName InSetName, ECollectionShareType::Type InSetType )
{
	return AssetSetPolicy::CreateAssetSet(InSetName, InSetType);
}

/** Clears the content of a Tag or Collection */
template <class AssetSetPolicy>	
bool FContentHelper::ClearAssetSet(FName InSetName, ECollectionShareType::Type InSetType )
{
	if (bInitialized == false)
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper is not initialized."));
		return false;
	}

	if ( AssetSetPolicy::DestroyAssetSet( InSetName, InSetType ) == false)
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper failed to destroy collection %s."), *InSetName.ToString());
		return false;
	}

	return true;
}


/** Sets the contents of a Tag or Collection to be the InAssetList. Assets not mentioned in the list will be untagged. */
template <class AssetSetPolicy>	
bool FContentHelper::AssignSetContent(FName InSetName, ECollectionShareType::Type InType, const TArray<FName>& InAssetList )
{
	bool bResult = true;

	if (bInitialized == false)
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper is not initialized."));
		return false;
	}

	// We ALWAYS want to create the collection. 
	// Even when there is nothing to add, it will indicate the operation was a success. 
	// For example, if a commandlet is run and a collection isn't generated, it would
	// not be clear whether the commandlet actually completed successfully.
	if (AssetSetPolicy::CreateAssetSet(InSetName, InType) == true)
	{
		// If there is nothing to update, we are done.
		if (InAssetList.Num() >= 0)
		{
			bool bAddCompleteInAssetList = true;

			TArray<FName> AssetsInCollection;
			AssetSetPolicy::QueryAssetsInSet(InSetName, InType, AssetsInCollection);
			int32 CurrentAssetCount = AssetsInCollection.Num();
			if (CurrentAssetCount != 0)
			{
				// Generate the lists
				TArray<FName> TrueAddList;
				TArray<FName> TrueRemoveList;

				// See how many items are really being added/removed
				for (int32 CheckIdx = 0; CheckIdx < AssetsInCollection.Num(); CheckIdx++)
				{
					FName CheckAsset = AssetsInCollection[CheckIdx];
					if (InAssetList.Find(CheckAsset) != INDEX_NONE)
					{
						TrueAddList.AddUnique(CheckAsset);
					}
					else
					{
						TrueRemoveList.AddUnique(CheckAsset);
					}
				}

				if ((TrueRemoveList.Num() + TrueAddList.Num()) < CurrentAssetCount)
				{
					// Remove and add only the required assets.
					bAddCompleteInAssetList = false;
					if (TrueRemoveList.Num() > 0)
					{
						if (AssetSetPolicy::RemoveAssetsFromSet(InSetName, InType, TrueRemoveList) == false)
						{
							UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper failed to remove assets from collection %s."), *InSetName.ToString());
							bResult = false;
						}
					}
					if (TrueAddList.Num() > 0)
					{
						if (AssetSetPolicy::AddAssetsToSet(InSetName, InType, TrueAddList) == false)
						{
							UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper failed to add assets to collection %s."), *InSetName.ToString());
							bResult = false;
						}
					}
				}
				else
				{
					// Clear the collection and fall into the add all case
					bAddCompleteInAssetList = ClearAssetSet<AssetSetPolicy>(InSetName, InType);
					if (bAddCompleteInAssetList == false)
					{
						// this is a problem!!!
						UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper failed to clear assets for collection %s."), *InSetName.ToString());
						bResult = false;
					}
				}
			}

			if (bAddCompleteInAssetList == true)
			{
				// Just add 'em all...
				if (AssetSetPolicy::AddAssetsToSet(InSetName, InType, InAssetList) == false)
				{
					UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper failed to add assets to collection %s."), *InSetName.ToString());
					bResult = false;
				}
			}
		}
	}
	else
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper failed to create collection %s."), *InSetName.ToString());
		bResult = false;
	}

	return bResult;
}

/** Add and remove assets for the specified Tag or Connection. Assets from InAddList are added; assets from InRemoveList are removed. */
template <class AssetSetPolicy>	
bool FContentHelper::UpdateSetContent(FName InSetName, ECollectionShareType::Type InType, const TArray<FName>& InAddList, const TArray<FName>& InRemoveList )
{
	bool bResult = true;

	if (bInitialized == false)
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper is not initialized."));
		return false;
	}

	// We ALWAYS want to create the collection. 
	// Even when there is nothing to add, it will indicate the operation was a success. 
	// For example, if a commandlet is run and a collection isn't generated, it would
	// not be clear whether the commandlet actually completed successfully.
	if (AssetSetPolicy::CreateAssetSet(InSetName, InType) == true)
	{
		// If there is nothing to update, we are done.
		if ((InAddList.Num() >= 0) || (InRemoveList.Num() >= 0))
		{
			TArray<FName> AssetsInCollection;
			AssetSetPolicy::QueryAssetsInSet(InSetName, InType, AssetsInCollection);
			if (AssetsInCollection.Num() != 0)
			{
				// Clean up the lists
				TArray<FName> TrueAddList;
				TArray<FName> TrueRemoveList;

				// Generate the true Remove list, only removing items that are actually in the collection.
				for (int32 RemoveIdx = 0; RemoveIdx < InRemoveList.Num(); RemoveIdx++)
				{
					if (AssetsInCollection.Contains(InRemoveList[RemoveIdx]) == true)
					{
						TrueRemoveList.AddUnique(InRemoveList[RemoveIdx]);
					}
				}

				if (TrueRemoveList.Num() > 0)
				{
					if (AssetSetPolicy::RemoveAssetsFromSet(InSetName, InType, TrueRemoveList) == false)
					{
						UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper failed to remove assets from collection %s."), *InSetName.ToString());
						bResult = false;
					}
				}

				// Generate the true Add list, only adding items that are not already in the collection.
				for (int32 AddIdx = 0; AddIdx < InAddList.Num(); AddIdx++)
				{
					if (AssetsInCollection.Contains(InAddList[AddIdx]) == false)
					{
						TrueAddList.AddUnique(InAddList[AddIdx]);
					}
				}

				if (TrueAddList.Num() > 0)
				{
					if (AssetSetPolicy::AddAssetsToSet(InSetName, InType, TrueAddList) == false)
					{
						UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper failed to add assets to collection %s."), *InSetName.ToString());
						bResult = false;
					}
				}
			}
			else
			{
				// Just add 'em all...
				if (AssetSetPolicy::AddAssetsToSet(InSetName, InType, InAddList) == false)
				{
					UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper failed to add assets to collection %s."), *InSetName.ToString());
					bResult = false;
				}
			}
		}
	}
	else
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper failed to create collection %s."), *InSetName.ToString());
		bResult = false;
	}

	return bResult;
}

/** Get the list of all assets in the specified Collection or Tag */
template <class AssetSetPolicy>	
bool FContentHelper::QuerySetContent(FName InSetName, ECollectionShareType::Type InType, TArray<FName>& OutAssetPathNames)
{
	if (bInitialized == false)
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("Collection Helper is not initialized."));
		return false;
	}

	return AssetSetPolicy::QueryAssetsInSet(InSetName, InType, OutAssetPathNames);
}


/**
 *	Initialize the Collection helper
 *	
 *	@return	bool					true if successful, false if failed
 */
bool FContentHelper::Initialize()
{
	// We no longer need to initialize anything. Keep this here in case we need to in the future.
	bInitialized = true;
	return bInitialized;
}

/**
 *	Shutdown the collection helper
 */
void FContentHelper::Shutdown()
{
	// We no longer need to shut down anything. Keep this here in case we need to in the future.
	bInitialized = false;
}

bool FContentHelper::CreateCollection(FName CollectionName, ECollectionShareType::Type InType)
{
	return this->CreateAssetSet<FCollectionPolicy>(CollectionName, InType);
}

/**
 *	Clear the given collection
 *	
 *	@param	InCollectionName	The name of the collection to create
 *	@param	InType				Type of collection
 *
 *	@return	bool				true if successful, false if failed
 */
bool FContentHelper::ClearCollection(FName InCollectionName, ECollectionShareType::Type InType)
{
	return this->ClearAssetSet<FCollectionPolicy>( InCollectionName, InType );
}

/**
 *	Fill the given collection with the given list of assets
 *
 *	@param	InCollectionName	The name of the collection to fill
 *	@param	InType				Type of collection
 *	@param	InAssetList			The list of items to fill the collection with (can be empty)
 *
 *	@return	bool				true if successful, false if not.
 */
bool FContentHelper::SetCollection(FName InCollectionName, ECollectionShareType::Type InType, const TArray<FName>& InAssetList)
{
	return this->AssignSetContent<FCollectionPolicy>(InCollectionName, InType, InAssetList);
}

/**
 *	Update the given collection with the lists of adds/removes
 *
 *	@param	InCollectionName	The name of the collection to update
 *	@param	InType				Type of collection
 *	@param	InAddList			The list of items to ADD to the collection (can be empty)
 *	@param	InRemoveList		The list of items to REMOVE from the collection (can be empty)
 *
 *	@return	bool				true if successful, false if not.
 */
bool FContentHelper::UpdateCollection(FName InCollectionName, ECollectionShareType::Type InType, const TArray<FName>& InAddList, const TArray<FName>& InRemoveList)
{
	return this->UpdateSetContent<FCollectionPolicy>( InCollectionName, InType, InAddList, InRemoveList );
}

/**
 *	Retrieve the assets contained in the given collection.
 *
 *	@param	InCollectionName	Name of collection to query
 *	@param	InType				Type of collection
 *	@param	OutAssetPathNames	The assets contained in the collection
 * 
 *	@return True if collection was created successfully
 */
bool FContentHelper::QueryAssetsInCollection(FName InCollectionName, ECollectionShareType::Type InType, TArray<FName>& OutAssetPathNames)
{
	return this->QuerySetContent<FCollectionPolicy>(InCollectionName, InType, OutAssetPathNames);
}


/*-----------------------------------------------------------------------------
ULoadPackageCommandlet
-----------------------------------------------------------------------------*/
ULoadPackageCommandlet::ULoadPackageCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LogToConsole = false;
}


bool ULoadPackageCommandlet::ParseLoadListFile(FString& LoadListFilename, TArray<FString>& Tokens)
{
	//Open file
	FString Data;
	if (FFileHelper::LoadFileToString(Data, *LoadListFilename) == true)
	{
		const TCHAR* Ptr = *Data;
		FString StrLine;

		while (FParse::Line(&Ptr, StrLine))
		{
			//UE_LOG(LogPackageUtilities, Log, TEXT("Read in: %s"), *StrLine);
			Tokens.AddUnique(StrLine);
		}

		// debugging...
		//UE_LOG(LogPackageUtilities, Log, TEXT("\nPACKAGES TO LOAD:"));
		for (int32 TokenIdx = 0; TokenIdx < Tokens.Num(); TokenIdx++)
		{
			//UE_LOG(LogPackageUtilities, Log, TEXT("\t%s"), *(Tokens(TokenIdx)));
		}
		return (Tokens.Num() > 0);
	}

	return false;
}



int32 ULoadPackageCommandlet::Main( const FString& Params )
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	bool bLoadAllPackages = Switches.Contains(TEXT("ALL"));
	bool bCheckForLegacyPackages = Switches.Contains(TEXT("CheckForLegacyPackages"));
	bool bFast = Switches.Contains(TEXT("FAST"));

	int32 MinVersion = MAX_int32;

	// Check for a load list file...
	for (int32 TokenIdx = 0; TokenIdx < Tokens.Num(); TokenIdx++)
	{
		FString LoadListFilename = TEXT("");
		if (FParse::Value(*(Tokens[TokenIdx]), TEXT("LOADLIST="), LoadListFilename))
		{
			// Found one - this will be a list of packages to load
			//UE_LOG(LogPackageUtilities, Log, TEXT("LoadList in file %s"), *LoadListFilename);

			TArray<FString> TempTokens;
			if (ParseLoadListFile(LoadListFilename, TempTokens) == true)
			{
				bLoadAllPackages = false;

				Tokens.Empty(TempTokens.Num());
				Tokens = TempTokens;
			}
		}
	}

	TArray<FString> FilesInPath;
	if ( bLoadAllPackages )
	{
		Tokens.Empty(2);
		Tokens.Add(FString("*") + FPackageName::GetAssetPackageExtension());
		Tokens.Add(FString("*") + FPackageName::GetMapPackageExtension());
	}

	if ( Tokens.Num() == 0 )
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("You must specify a package name (multiple files can be delimited by spaces) or wild-card, or specify -all to include all registered packages"));
		return 1;
	}

	uint8 PackageFilter = NORMALIZE_DefaultFlags;
	if (Switches.Contains(TEXT("SKIPMAPS")))
	{
		PackageFilter |= NORMALIZE_ExcludeMapPackages;
	}
	else if (Switches.Contains(TEXT("MAPSONLY")))
	{
		PackageFilter |= NORMALIZE_ExcludeContentPackages;
	}

	if (Switches.Contains(TEXT("PROJECTONLY")))
	{
		PackageFilter |= NORMALIZE_ExcludeEnginePackages;
	}

	if (Switches.Contains(TEXT("SkipDeveloperFolders")) || Switches.Contains(TEXT("NODEV")))
	{
		PackageFilter |= NORMALIZE_ExcludeDeveloperPackages;
	}
	else if (Switches.Contains(TEXT("OnlyDeveloperFolders")))
	{
		PackageFilter |= NORMALIZE_ExcludeNonDeveloperPackages;
	}

	// assume the first token is the map wildcard/pathname
	TArray<FString> Unused;
	for ( int32 TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++ )
	{
		TArray<FString> TokenFiles;
		if ( !NormalizePackageNames( Unused, TokenFiles, Tokens[TokenIndex], PackageFilter) )
		{
			UE_LOG(LogPackageUtilities, Display, TEXT("No packages found for parameter %i: '%s'"), TokenIndex, *Tokens[TokenIndex]);
			continue;
		}

		FilesInPath += TokenFiles;
	}

	if ( FilesInPath.Num() == 0 )
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("No files found."));
		return 1;
	}

	GIsClient = !Switches.Contains(TEXT("NOCLIENT"));
	GIsServer = !Switches.Contains(TEXT("NOSERVER"));
	GIsEditor = !Switches.Contains(TEXT("NOEDITOR"));

	for( int32 FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FString& Filename = FilesInPath[FileIndex];

		UE_LOG(LogPackageUtilities, Warning, TEXT("Loading %s"), *Filename );

		FString PackageName;
		if (FPackageName::TryConvertFilenameToLongPackageName(Filename, PackageName))
		{
			UPackage* Package = FindObject<UPackage>(nullptr, *PackageName, true);
			if (Package != NULL && !bLoadAllPackages)
			{
				ResetLoaders(Package);
			}
		}

		if (bCheckForLegacyPackages)
		{
			BeginLoad();
			auto Linker = GetPackageLinker(nullptr, *Filename,LOAD_NoVerify,NULL,NULL);
			EndLoad();
			MinVersion = FMath::Min<int32>(MinVersion, Linker->Summary.GetFileVersionUE4());
		}
		else
		{
			UPackage* Package = LoadPackage(nullptr, *Filename, LOAD_None );
			if(Package == nullptr)
			{
				UE_LOG(LogPackageUtilities, Error, TEXT("Error loading %s!"), *Filename );
			}
		}
		if (!bFast || FileIndex % 100 == 99)
		{
			CollectGarbage(RF_NoFlags);
		}
	}
	GIsEditor = GIsServer = GIsClient = true;
	if (bCheckForLegacyPackages)
	{
		UE_LOG(LogPackageUtilities, Log, TEXT("%d minimum UE4 version number."), MinVersion );
	}

	return 0;
}

/*-----------------------------------------------------------------------------
	UPkgInfo commandlet.
-----------------------------------------------------------------------------*/

struct FExportInfo
{
	FObjectExport Export;
	int32 ExportIndex;
	FString PathName;
	FString OuterPathName;

	FExportInfo( FLinkerLoad* Linker, int32 InIndex )
	: Export(Linker->ExportMap[InIndex]), ExportIndex(InIndex)
	, OuterPathName(TEXT("NULL"))
	{
		PathName = Linker->GetExportPathName(ExportIndex);
		SetOuterPathName(Linker);
	}

	void SetOuterPathName( FLinkerLoad* Linker )
	{
		if ( !Export.OuterIndex.IsNull() )
		{
			OuterPathName = Linker->GetPathName(Export.OuterIndex);
		}
	}
};

namespace
{
	enum EExportSortType
	{
		EXPORTSORT_ExportSize,
		EXPORTSORT_ExportIndex,
		EXPORTSORT_ObjectPathname,
		EXPORTSORT_OuterPathname,
		EXPORTSORT_MAX
	};

	struct FObjectExport_Sorter
	{
		static EExportSortType SortPriority[EXPORTSORT_MAX];

		// Comparison method
		bool operator()( const FExportInfo& A, const FExportInfo& B ) const
		{
			int32 Result = 0;

			for ( int32 PriorityType = 0; PriorityType < EXPORTSORT_MAX; PriorityType++ )
			{
				switch ( SortPriority[PriorityType] )
				{
					case EXPORTSORT_ExportSize:
						Result = B.Export.SerialSize - A.Export.SerialSize;
						break;

					case EXPORTSORT_ExportIndex:
						Result = A.ExportIndex - B.ExportIndex;
						break;

					case EXPORTSORT_ObjectPathname:
						Result = A.PathName.Len() - B.PathName.Len();
						if ( Result == 0 )
						{
							Result = FCString::Stricmp(*A.PathName, *B.PathName);
						}
						break;

					case EXPORTSORT_OuterPathname:
						Result = A.OuterPathName.Len() - B.OuterPathName.Len();
						if ( Result == 0 )
						{
							Result = FCString::Stricmp(*A.OuterPathName, *B.OuterPathName);
						}
						break;

					case EXPORTSORT_MAX:
						return !!Result;
				}

				if ( Result != 0 )
				{
					break;
				}
			}
			return Result < 0;
		}
	};

	EExportSortType FObjectExport_Sorter::SortPriority[EXPORTSORT_MAX] =
	{ EXPORTSORT_ExportIndex, EXPORTSORT_ExportSize, EXPORTSORT_OuterPathname, EXPORTSORT_ObjectPathname };
}

/** Given a package filename, creates a linker and a temporary package. The filename does not need to point to a package under the current project content folder */
FLinkerLoad* CreateLinkerForFilename(const FString& InFilename)
{
	FString TempPackageName;
	TempPackageName = FPaths::Combine(TEXT("/Temp"), *FPaths::GetPath(InFilename.Mid(InFilename.Find(TEXT(":"), ESearchCase::CaseSensitive) + 1)), *FPaths::GetBaseFilename(InFilename));
	UPackage* Package = FindObjectFast<UPackage>(nullptr, *TempPackageName);
	if (!Package)
	{
		Package = CreatePackage(nullptr, *TempPackageName);
	}
	FLinkerLoad* Linker = FLinkerLoad::CreateLinker(Package, *InFilename, LOAD_NoVerify);
	return Linker;
}

/**
 * Writes information about the linker to the log.
 *
 * @param	InLinker	if specified, changes this reporter's Linker before generating the report.
 */
void FPkgInfoReporter_Log::GeneratePackageReport( FLinkerLoad* InLinker/*=NULL*/ )
{
	check(InLinker);

	if ( InLinker != NULL )
	{
		SetLinker(InLinker);
	}

	if ( PackageCount++ > 0 )
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT(""));
	}

	// Display information about the package.
	FName LinkerName = Linker->LinkerRoot->GetFName();

	// Display summary info.
	UE_LOG(LogPackageUtilities, Warning, TEXT("********************************************") );
	UE_LOG(LogPackageUtilities, Warning, TEXT("Package '%s' Summary"), *LinkerName.ToString() );
	UE_LOG(LogPackageUtilities, Warning, TEXT("--------------------------------------------") );

	UE_LOG(LogPackageUtilities, Warning, TEXT("\t         Filename: %s"), *Linker->Filename);
	UE_LOG(LogPackageUtilities, Warning, TEXT("\t     File Version: %i"), Linker->UE4Ver() );
	UE_LOG(LogPackageUtilities, Warning, TEXT("\t   Engine Version: %s"), *Linker->Summary.SavedByEngineVersion.ToString());
	UE_LOG(LogPackageUtilities, Warning, TEXT("\t   Compat Version: %s"), *Linker->Summary.CompatibleWithEngineVersion.ToString());
	UE_LOG(LogPackageUtilities, Warning, TEXT("\t     PackageFlags: %X"), Linker->Summary.PackageFlags );
	UE_LOG(LogPackageUtilities, Warning, TEXT("\t        NameCount: %d"), Linker->Summary.NameCount );
	UE_LOG(LogPackageUtilities, Warning, TEXT("\t       NameOffset: %d"), Linker->Summary.NameOffset );
	UE_LOG(LogPackageUtilities, Warning, TEXT("\t      ImportCount: %d"), Linker->Summary.ImportCount );
	UE_LOG(LogPackageUtilities, Warning, TEXT("\t     ImportOffset: %d"), Linker->Summary.ImportOffset );
	UE_LOG(LogPackageUtilities, Warning, TEXT("\t      ExportCount: %d"), Linker->Summary.ExportCount );
	UE_LOG(LogPackageUtilities, Warning, TEXT("\t     ExportOffset: %d"), Linker->Summary.ExportOffset );
	UE_LOG(LogPackageUtilities, Warning, TEXT("\tCompression Flags: %X"), Linker->Summary.CompressionFlags);
	UE_LOG(LogPackageUtilities, Warning, TEXT("\t  Custom Versions:\n%s"), *Linker->Summary.GetCustomVersionContainer().ToString("\t\t"));
	

	FString szGUID = Linker->Summary.Guid.ToString();
	UE_LOG(LogPackageUtilities, Warning, TEXT("\t             Guid: %s"), *szGUID );
	GWarn->Log ( TEXT("\t      Generations:"));
	for( int32 i = 0; i < Linker->Summary.Generations.Num(); ++i )
	{
		const FGenerationInfo& generationInfo = Linker->Summary.Generations[ i ];
		UE_LOG(LogPackageUtilities, Warning,TEXT("\t\t\t%d) ExportCount=%d, NameCount=%d "), i, generationInfo.ExportCount, generationInfo.NameCount );
	}

	if( (InfoFlags&PKGINFO_Names) != 0 )
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("--------------------------------------------") );
		GWarn->Log ( TEXT("Name Map"));
		GWarn->Log ( TEXT("========"));
		for( int32 i = 0; i < Linker->NameMap.Num(); ++i )
		{
			FName& name = Linker->NameMap[ i ];
			UE_LOG(LogPackageUtilities, Warning, TEXT("\t%d: Name '%s' Comparison Index %d Display Index %d [Internal: %s, %d]"), i, *name.ToString(), name.GetComparisonIndex(), name.GetDisplayIndex(), *name.GetPlainNameString(), name.GetNumber() );
		}
	}

	// if we _only_ want name info, skip this part completely
	if ( InfoFlags != PKGINFO_Names )
	{
		if( (InfoFlags&PKGINFO_Imports) != 0 )
		{
			UE_LOG(LogPackageUtilities, Warning, TEXT("--------------------------------------------") );
			GWarn->Log ( TEXT("Import Map"));
			GWarn->Log ( TEXT("=========="));
		}

		TArray<FName> DependentPackages;
		for( int32 i = 0; i < Linker->ImportMap.Num(); ++i )
		{
			FObjectImport& import = Linker->ImportMap[ i ];

			FName PackageName = NAME_None;
			FName OuterName = NAME_None;
			if ( !import.OuterIndex.IsNull() )
			{
				if ( (InfoFlags&PKGINFO_Paths) != 0 )
				{
					OuterName = *Linker->GetPathName(import.OuterIndex);
				}
				else
				{
					OuterName = Linker->ImpExp(import.OuterIndex).ObjectName;
				}

				// Find the package which contains this import.  import.SourceLinker is cleared in EndLoad, so we'll need to do this manually now.
				FPackageIndex OutermostLinkerIndex = import.OuterIndex;
				for ( FPackageIndex LinkerIndex = import.OuterIndex; !LinkerIndex.IsNull(); )
				{
					OutermostLinkerIndex = LinkerIndex;
					LinkerIndex = Linker->ImpExp(LinkerIndex).OuterIndex;
				}
				check(!OutermostLinkerIndex.IsNull());
				PackageName = Linker->ImpExp(OutermostLinkerIndex).ObjectName;
			}

			if ( (InfoFlags&PKGINFO_Imports) != 0 )
			{
				GWarn->Log ( TEXT("\t*************************"));
				UE_LOG(LogPackageUtilities, Display, TEXT("\tImport %d: '%s'"), i, *import.ObjectName.ToString() );
				UE_LOG(LogPackageUtilities, Display, TEXT("\t\t       Outer: '%s' (%d)"), *OuterName.ToString(), import.OuterIndex.ForDebugging());
				UE_LOG(LogPackageUtilities, Display, TEXT("\t\t     Package: '%s'"), *PackageName.ToString());
				UE_LOG(LogPackageUtilities, Display, TEXT("\t\t       Class: '%s'"), *import.ClassName.ToString() );
				UE_LOG(LogPackageUtilities, Display, TEXT("\t\tClassPackage: '%s'"), *import.ClassPackage.ToString() );
				UE_LOG(LogPackageUtilities, Display, TEXT("\t\t     XObject: %s"), import.XObject ? TEXT("VALID") : TEXT("NULL"));
				UE_LOG(LogPackageUtilities, Display, TEXT("\t\t SourceIndex: %d"), import.SourceIndex );

				// dump depends info
				if (InfoFlags & PKGINFO_Depends)
				{
					UE_LOG(LogPackageUtilities, Display, TEXT("\t\t  All Depends:"));

					TSet<FDependencyRef> AllDepends;
					Linker->GatherImportDependencies(i, AllDepends);
					int32 DependsIndex = 0;
					for(TSet<FDependencyRef>::TConstIterator It(AllDepends);It;++It)
					{
						const FDependencyRef& Ref = *It;
						if (Ref.Linker)
						{
							UE_LOG(LogPackageUtilities, Display, TEXT("\t\t\t%i) %s"), DependsIndex++, *Ref.Linker->GetExportFullName(Ref.ExportIndex));
						}
						else
						{
							UE_LOG(LogPackageUtilities, Display, TEXT("\t\t\t%i) NULL"), DependsIndex++);
						}
					}
				}
			}

			if ( PackageName == NAME_None && import.ClassName == NAME_Package )
			{
				PackageName = import.ObjectName;
			}

			if ( PackageName != NAME_None && PackageName != LinkerName )
			{
				DependentPackages.AddUnique(PackageName);
			}

			if ( import.ClassPackage != NAME_None && import.ClassPackage != LinkerName )
			{
				DependentPackages.AddUnique(import.ClassPackage);
			}
		}

		if ( DependentPackages.Num() )
		{
			UE_LOG(LogPackageUtilities, Warning, TEXT("--------------------------------------------") );
			UE_LOG(LogPackageUtilities, Warning, TEXT("\tPackages referenced by %s:"), *LinkerName.ToString());
			for ( int32 i = 0; i < DependentPackages.Num(); i++ )
			{
				UE_LOG(LogPackageUtilities, Warning, TEXT("\t\t%i) %s"), i, *DependentPackages[i].ToString());
			}
		}
	}

	if( (InfoFlags&PKGINFO_Exports) != 0 )
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("--------------------------------------------") );
		GWarn->Log ( TEXT("Export Map"));
		GWarn->Log ( TEXT("=========="));

		TArray<FExportInfo> SortedExportMap;
		SortedExportMap.Empty(Linker->ExportMap.Num());
		for( int32 i = 0; i < Linker->ExportMap.Num(); ++i )
		{
			new(SortedExportMap) FExportInfo(Linker, i);
		}

		FString SortingParms;
		if ( FParse::Value(FCommandLine::Get(), TEXT("SORT="), SortingParms) )
		{
			TArray<FString> SortValues;
			SortingParms.ParseIntoArray(SortValues, TEXT(","), true);

			for ( int32 i = 0; i < EXPORTSORT_MAX; i++ )
			{
				if ( i < SortValues.Num() )
				{
					const FString Value = SortValues[i];
					if ( Value == TEXT("index") )
					{
						FObjectExport_Sorter::SortPriority[i] = EXPORTSORT_ExportIndex;
					}
					else if ( Value == TEXT("size") )
					{
						FObjectExport_Sorter::SortPriority[i] = EXPORTSORT_ExportSize;
					}
					else if ( Value == TEXT("name") )
					{
						FObjectExport_Sorter::SortPriority[i] = EXPORTSORT_ObjectPathname;
					}
					else if ( Value == TEXT("outer") )
					{
						FObjectExport_Sorter::SortPriority[i] = EXPORTSORT_OuterPathname;
					}
				}
				else
				{
					FObjectExport_Sorter::SortPriority[i] = EXPORTSORT_MAX;
				}
			}
		}

		SortedExportMap.Sort( FObjectExport_Sorter() );

		if ( (InfoFlags&PKGINFO_Compact) == 0 )
		{
			for( const auto& ExportInfo : SortedExportMap )
			{
				GWarn->Log ( TEXT("\t*************************"));
				const FObjectExport& Export = ExportInfo.Export;

				UE_LOG(LogPackageUtilities, Warning, TEXT("\tExport %d: '%s'"), ExportInfo.ExportIndex, *Export.ObjectName.ToString() );

				// find the name of this object's class
				FPackageIndex ClassIndex = Export.ClassIndex;
				FName ClassName = ClassIndex.IsNull() ? FName(NAME_Class) : Linker->ImpExp(ClassIndex).ObjectName;

				// find the name of this object's parent...for UClasses, this will be the parent class
				// for UFunctions, this will be the SuperFunction, if it exists, etc.
				FString ParentName;
				if ( !Export.SuperIndex.IsNull() )
				{
					if ( (InfoFlags&PKGINFO_Paths) != 0 )
					{
						ParentName = *Linker->GetPathName(Export.SuperIndex);
					}
					else
					{
						ParentName = Linker->ImpExp(Export.SuperIndex).ObjectName.ToString();
					}
				}

				// find the name of this object's parent...for UClasses, this will be the parent class
				// for UFunctions, this will be the SuperFunction, if it exists, etc.
				FString TemplateName;
				if (!Export.TemplateIndex.IsNull())
				{
					if ((InfoFlags&PKGINFO_Paths) != 0)
					{
						TemplateName = *Linker->GetPathName(Export.TemplateIndex);
					}
					else
					{
						TemplateName = Linker->ImpExp(Export.TemplateIndex).ObjectName.ToString();
					}
				}

				// find the name of this object's Outer.  For UClasses, this will generally be the
				// top-level package itself.  For properties, a UClass, etc.
				FString OuterName;
				if ( !Export.OuterIndex.IsNull() )
				{
					if ( (InfoFlags&PKGINFO_Paths) != 0 )
					{
						OuterName = *Linker->GetPathName(Export.OuterIndex);
					}
					else
					{
						OuterName = Linker->ImpExp(Export.OuterIndex).ObjectName.ToString();
					}
				}

				UE_LOG(LogPackageUtilities, Warning, TEXT("\t\t         Class: '%s' (%i)"), *ClassName.ToString(), ClassIndex.ForDebugging() );
				UE_LOG(LogPackageUtilities, Warning, TEXT("\t\t        Parent: '%s' (%d)"), *ParentName, Export.SuperIndex.ForDebugging());
				UE_LOG(LogPackageUtilities, Warning, TEXT("\t\t      Template: '%s' (%d)"), *TemplateName, Export.TemplateIndex.ForDebugging());
				UE_LOG(LogPackageUtilities, Warning, TEXT("\t\t         Outer: '%s' (%d)"), *OuterName, Export.OuterIndex.ForDebugging() );
				UE_LOG(LogPackageUtilities, Warning, TEXT("\t\t      Pkg Guid: %s"), *Export.PackageGuid.ToString());
				UE_LOG(LogPackageUtilities, Warning, TEXT("\t\t   ObjectFlags: 0x%08X"), (uint32)Export.ObjectFlags );
				UE_LOG(LogPackageUtilities, Warning, TEXT("\t\t          Size: %d"), Export.SerialSize );
				if ( !bHideOffsets )
				{
					UE_LOG(LogPackageUtilities, Warning, TEXT("\t\t      Offset: %d"), Export.SerialOffset );
				}
				UE_LOG(LogPackageUtilities, Warning, TEXT("\t\t       Object: %s"), Export.Object ? TEXT("VALID") : TEXT("NULL"));
				if ( !bHideOffsets )
				{
					UE_LOG(LogPackageUtilities, Warning, TEXT("\t\t    HashNext: %d"), Export.HashNext );
				}
				UE_LOG(LogPackageUtilities, Warning, TEXT("\t\t   bNotForClient: %d"), Export.bNotForClient );
				UE_LOG(LogPackageUtilities, Warning, TEXT("\t\t   bNotForServer: %d"), Export.bNotForServer );

				// dump depends info
				if (InfoFlags & PKGINFO_Depends)
				{
					if (ExportInfo.ExportIndex < Linker->DependsMap.Num())
					{
						TArray<FPackageIndex>& Depends = Linker->DependsMap[ExportInfo.ExportIndex];
						UE_LOG(LogPackageUtilities, Display, TEXT("\t\t  DependsMap:"));

						for (int32 DependsIndex = 0; DependsIndex < Depends.Num(); DependsIndex++)
						{
							UE_LOG(LogPackageUtilities, Warning,TEXT("\t\t\t%i) %s (%i)"),
								DependsIndex, 
								*Linker->GetFullImpExpName(Depends[DependsIndex]),
								Depends[DependsIndex].ForDebugging()
							);
						}

						TSet<FDependencyRef> AllDepends;
						Linker->GatherExportDependencies(ExportInfo.ExportIndex, AllDepends);
						UE_LOG(LogPackageUtilities, Display, TEXT("\t\t  All Depends:"));
						int32 DependsIndex = 0;
						for(TSet<FDependencyRef>::TConstIterator It(AllDepends);It;++It)
						{
							const FDependencyRef& Ref = *It;
							if (Ref.Linker)
							{
								UE_LOG(LogPackageUtilities, Warning,TEXT("\t\t\t%i) %s (%i)"),
									DependsIndex++,
									*Ref.Linker->GetExportFullName(Ref.ExportIndex),
									Ref.ExportIndex);
							}
							else
							{
								UE_LOG(LogPackageUtilities, Warning,TEXT("\t\t\t%i) NULL (%i)"),
									DependsIndex++,
									Ref.ExportIndex);
							}
						}
					}
				}
			}
		}
		else
		{
			for( const auto& ExportInfo : SortedExportMap )
			{
				const FObjectExport& Export = ExportInfo.Export;
				UE_LOG(LogPackageUtilities, Warning, TEXT("  %8i %10i %32s %s"), ExportInfo.ExportIndex, Export.SerialSize, 
					*(Linker->GetExportClassName(ExportInfo.ExportIndex).ToString()), 
					(InfoFlags&PKGINFO_Paths) != 0 ? *Linker->GetExportPathName(ExportInfo.ExportIndex) : *Export.ObjectName.ToString());
			}
		}
	}

	if( (InfoFlags&PKGINFO_Text) != 0 )
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("--------------------------------------------") );
		GWarn->Log ( TEXT("Gatherable Text Data Map"));
		GWarn->Log ( TEXT("=========="));

		if (Linker->SerializeGatherableTextDataMap(true))
		{
			UE_LOG(LogPackageUtilities, Display, TEXT("Number of Text Data Entries: %d"), Linker->GatherableTextDataMap.Num());

			for (int32 i = 0; i < Linker->GatherableTextDataMap.Num(); ++i)
			{
				const FGatherableTextData& GatherableTextData = Linker->GatherableTextDataMap[i];
				UE_LOG(LogPackageUtilities, Display, TEXT("Entry %d:"), 1 + i);
				UE_LOG(LogPackageUtilities, Display, TEXT("\t   String: %s"), *GatherableTextData.SourceData.SourceString.ReplaceCharWithEscapedChar());
				UE_LOG(LogPackageUtilities, Display, TEXT("\tNamespace: %s"), *GatherableTextData.NamespaceName);
				UE_LOG(LogPackageUtilities, Display, TEXT("\t   Key(s): %d"), GatherableTextData.SourceSiteContexts.Num());
				for (const FTextSourceSiteContext& TextSourceSiteContext : GatherableTextData.SourceSiteContexts)
				{
					UE_LOG(LogPackageUtilities, Display, TEXT("\t\t%s from %s"), *TextSourceSiteContext.KeyName, *TextSourceSiteContext.SiteDescription);
				}
			}
		}
		else
		{
			if ( Linker->Summary.GatherableTextDataOffset > 0 )
			{
				UE_LOG(LogPackageUtilities, Warning,TEXT("Failed to load gatherable text data for package %s!"), *LinkerName.ToString());
			}
		}
	}


	if( (InfoFlags&PKGINFO_Thumbs) != 0 )
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("--------------------------------------------") );
		GWarn->Log ( TEXT("Thumbnail Data"));
		GWarn->Log ( TEXT("=========="));

		if ( Linker->SerializeThumbnails(true) )
		{
			if ( Linker->LinkerRoot->HasThumbnailMap() )
			{
				FThumbnailMap& LinkerThumbnails = Linker->LinkerRoot->AccessThumbnailMap();

				int32 MaxObjectNameSize = 0;
				for ( TMap<FName, FObjectThumbnail>::TIterator It(LinkerThumbnails); It; ++It )
				{
					FName& ObjectPathName = It.Key();
					MaxObjectNameSize = FMath::Max(MaxObjectNameSize, ObjectPathName.ToString().Len());
				}

				int32 ThumbIdx=0;
				for ( TMap<FName, FObjectThumbnail>::TIterator It(LinkerThumbnails); It; ++It )
				{
					FName& ObjectFullName = It.Key();
					FObjectThumbnail& Thumb = It.Value();

					UE_LOG(LogPackageUtilities, Warning,TEXT("\t\t%i) %*s: %ix%i\t\tImage Data:%i bytes"), ThumbIdx++, MaxObjectNameSize, *ObjectFullName.ToString(), Thumb.GetImageWidth(), Thumb.GetImageHeight(), Thumb.GetCompressedDataSize());
				}
			}
			else
			{
				UE_LOG(LogPackageUtilities, Warning,TEXT("%s has no thumbnail map!"), *LinkerName.ToString());
			}
		}
		else
		{
			if ( Linker->Summary.ThumbnailTableOffset > 0 )
			{
				UE_LOG(LogPackageUtilities, Warning,TEXT("Failed to load thumbnails for package %s!"), *LinkerName.ToString());
			}
		}
	}

	if( (InfoFlags&PKGINFO_Lazy) != 0 )
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("--------------------------------------------") );
		GWarn->Log ( TEXT("Lazy Pointer Data"));
		GWarn->Log ( TEXT("==============="));		
	}

	if( (InfoFlags&PKGINFO_AssetRegistry) != 0 )
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("--------------------------------------------"));

		{
			const int32 NextOffset = Linker->Summary.WorldTileInfoDataOffset ? Linker->Summary.WorldTileInfoDataOffset : Linker->Summary.TotalHeaderSize;
			const int32 AssetRegistrySize = NextOffset - Linker->Summary.AssetRegistryDataOffset;
			UE_LOG(LogPackageUtilities, Display, TEXT("Asset Registry Size: %10i"), AssetRegistrySize);
		}
		
		GWarn->Log ( TEXT("Asset Registry Data"));
		GWarn->Log ( TEXT("=========="));

		if( Linker->Summary.AssetRegistryDataOffset > 0 )
		{
			// Seek to the AssetRegistry table of contents
			Linker->Loader->Seek( Linker->Summary.AssetRegistryDataOffset );

			// Load the number of assets in the tag map
			int32 AssetCount = 0;
			*Linker << AssetCount;

			UE_LOG(LogPackageUtilities, Display, TEXT("Number of assets with Asset Registry data: %d"), AssetCount );

			// If there are any Asset Registry tags, print them
			for (int32 AssetIdx = 0; AssetIdx < AssetCount; ++AssetIdx)
			{
				// Display the asset class and path
				FString ObjectPath;
				FString ObjectClassName;
				int32 TagCount = 0;

				*Linker << ObjectPath;
				*Linker << ObjectClassName;
				*Linker << TagCount;

				UE_LOG(LogPackageUtilities, Display, TEXT("\t\t%d) %s'%s' (%d Tags)"), AssetIdx, *ObjectClassName, *ObjectPath, TagCount );

				// Now display all tags on this asset
				for (int32 TagIdx = 0; TagIdx < TagCount; ++TagIdx)
				{
					FString Key;
					FString Value;
					*Linker << Key;
					*Linker << Value;

					UE_LOG(LogPackageUtilities, Display, TEXT("\t\t\t\"%s\": \"%s\""), *Key, *Value );
				}
			}
		}
	}
}

UPkgInfoCommandlet::UPkgInfoCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LogToConsole = false;
}


int32 UPkgInfoCommandlet::Main( const FString& Params )
{
	// turn off as it makes diffing hard
	auto bOldGPrintLogTimes = GPrintLogTimes;
	GPrintLogTimes = ELogTimes::None;

	const TCHAR* Parms = *Params;

	TArray<FString> Tokens, Switches;
	ParseCommandLine(Parms, Tokens, Switches);

	// find out which type of info we're looking for
	uint32 InfoFlags = PKGINFO_None;
	if ( Switches.Contains(TEXT("names")) )
	{
		InfoFlags |= PKGINFO_Names;
	}
	if ( Switches.Contains(TEXT("imports")) )
	{
		InfoFlags |= PKGINFO_Imports;
	}
	if ( Switches.Contains(TEXT("exports")) )
	{
		InfoFlags |= PKGINFO_Exports;
	}
	if ( Switches.Contains(TEXT("simple")) )
	{
		InfoFlags |= PKGINFO_Compact;
	}
	if ( Switches.Contains(TEXT("depends")) )
	{
		InfoFlags |= PKGINFO_Depends;
	}
	if ( Switches.Contains(TEXT("paths")) )
	{
		InfoFlags |= PKGINFO_Paths;
	}
	if ( Switches.Contains(TEXT("thumbnails")) )
	{
		InfoFlags |= PKGINFO_Thumbs;
	}
	if ( Switches.Contains(TEXT("lazy")) )
	{
		InfoFlags |= PKGINFO_Lazy;
	}
	if ( Switches.Contains(TEXT("assetregistry")) )
	{
		InfoFlags |= PKGINFO_AssetRegistry;
	}
	if ( Switches.Contains(TEXT("all")) )
	{
		InfoFlags |= PKGINFO_All;
	}

	const bool bHideOffsets = Switches.Contains(TEXT("HideOffsets"));

	FPkgInfoReporter* Reporter = new FPkgInfoReporter_Log(InfoFlags, bHideOffsets);

	TArray<FString> FilesInPath;
	if( Switches.Contains(TEXT("AllPackages")) )
	{
		FEditorFileUtils::FindAllPackageFiles(FilesInPath);
	}
	else
	{
		for ( int32 TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++ )
		{
			FString& PackageWildcard = Tokens[TokenIndex];

			TArray<FString> PerTokenFilesInPath;
			IFileManager::Get().FindFiles( PerTokenFilesInPath, *PackageWildcard, true, false );

			if( PerTokenFilesInPath.Num() == 0 )
			{
				TArray<FString> Paths;
				if ( GConfig->GetArray( TEXT("Core.System"), TEXT("Paths"), Paths, GEngineIni ) > 0 )
				{
					for ( int32 i = 0; i < Paths.Num(); i++ )
					{
						IFileManager::Get().FindFiles( PerTokenFilesInPath, *(Paths[i] / PackageWildcard), 1, 0 );
					}
				}

				if ( PerTokenFilesInPath.Num() == 0 )
				{
					// Check if long package name is provided and if it exists on disk.
					FString Filename;
					if ( FPackageName::IsValidLongPackageName(PackageWildcard, true) && FPackageName::DoesPackageExist(PackageWildcard, NULL, &Filename) )
					{
						PerTokenFilesInPath.Add(Filename);
					}
				}
			}
			else
			{
				// re-add the path information so that GetPackageLinker finds the correct version of the file.
				FString WildcardPath = PackageWildcard;
				for ( int32 FileIndex = 0; FileIndex < PerTokenFilesInPath.Num(); FileIndex++ )
				{
					PerTokenFilesInPath[FileIndex] = FPaths::GetPath(WildcardPath) / PerTokenFilesInPath[FileIndex];
					FPaths::NormalizeFilename(PerTokenFilesInPath[FileIndex]);
				}
			}

			if ( PerTokenFilesInPath.Num() == 0 )
			{
				UE_LOG(LogPackageUtilities, Warning, TEXT("No packages found using '%s'!"), *PackageWildcard);
				continue;
			}

			FilesInPath += PerTokenFilesInPath;
		}
	}

	for( int32 FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		FString Filename = FPaths::ConvertRelativePathToFull(FilesInPath[FileIndex]);

		{
			// reset the loaders for the packages we want to load so that we don't find the wrong version of the file
			// (otherwise, attempting to run pkginfo on e.g. Engine.xxx will always return results for Engine.u instead)
			FString PackageName;
			if (FPackageName::TryConvertFilenameToLongPackageName(Filename, PackageName))
			{
				UPackage* ExistingPackage = FindObject<UPackage>(nullptr, *PackageName, true);
				if (ExistingPackage != nullptr)
				{
					ResetLoaders(ExistingPackage);
				}
			}
		}

		BeginLoad();
		FLinkerLoad* Linker = CreateLinkerForFilename(Filename);
		EndLoad();

		if( Linker )
		{
			Reporter->GeneratePackageReport(Linker);
		}

		CollectGarbage(RF_NoFlags);
	}

	// turn off as it makes diffing hard
	GPrintLogTimes = bOldGPrintLogTimes;

	delete Reporter;
	Reporter = NULL;
	return 0;
}

/*-----------------------------------------------------------------------------
 CompressAnimations Commandlet
-----------------------------------------------------------------------------*/

static int32 AnalyzeCompressionCandidates = 0;
static TArray<FString> PackagesThatCouldNotBeSavedList;

struct AddAllSkeletalMeshesToListFunctor
{
	template< typename OBJECTYPE >
	void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
		for( TObjectIterator<OBJECTYPE> It; It; ++It )
		{
			OBJECTYPE* SkelMesh = *It;
			SkelMesh->AddToRoot();
		}
	}
};

/**
 * 
 */
struct CompressAnimationsFunctor
{
	template< typename OBJECTYPE >
	void DoIt( UCommandlet* Commandlet, UPackage* Package, TArray<FString>& Tokens, TArray<FString>& Switches )
	{
		// Count the number of animations to provide some limited progress indication
		int32 NumAnimationsInPackage = 0;
		for (TObjectIterator<OBJECTYPE> It; It; ++It)
		{
			OBJECTYPE* AnimSeq = *It;
			if (!AnimSeq->IsIn(Package))
			{
				continue;
			}

			++NumAnimationsInPackage;
		}

		// Skip packages that contain no Animations.
		if (NumAnimationsInPackage == 0)
		{
			return;
		}

		// @todoanim: we expect this won't work properly since it won't have any skeletalmesh,
		// but soon, the compression will changed based on skeleton. 
		// when that happens, this doesn't have to worry about skeletalmesh not loaded
	 	float LastSaveTime = FPlatformTime::Seconds();
		bool bDirtyPackage = false;
		const FName& PackageName = Package->GetFName(); 
		FString PackageFileName;
		FPackageName::DoesPackageExist( PackageName.ToString(), NULL, &PackageFileName );

		// Ensure source control is initialized and shut down properly
		FScopedSourceControl SourceControl;

		const bool bSkipCinematicPackages = Switches.Contains(TEXT("SKIPCINES"));
		const bool bSkipLongAnimations = Switches.Contains(TEXT("SKIPLONGANIMS"));
		// Reset compression, don't do incremental compression, start from scratch
		const bool bResetCompression = Switches.Contains(TEXT("RESETCOMPRESSION"));
		/** Clear bDoNotOverrideCompression flag in animations */
		const bool bClearNoCompressionOverride = Switches.Contains(TEXT("CLEARNOCOMPRESSIONOVERRIDE"));
		/** If we're analyzing, we're not actually going to recompress, so we can skip some significant work. */
		const bool bAnalyze = Switches.Contains(TEXT("ANALYZE"));
		// See if we can save this package. If we can't, don't bother...
		/** if we should auto checkout packages that need to be saved **/
		const bool bAutoCheckOut = Switches.Contains(TEXT("AUTOCHECKOUTPACKAGES"));

		FSourceControlStatePtr SourceControlState = SourceControl.GetProvider().GetState(PackageFileName, EStateCacheUsage::ForceUpdate);

		// check to see if we need to check this package out
		if( !bAnalyze && SourceControlState.IsValid() && SourceControlState->CanCheckout() )
		{
			// Cant check out, check to see why
			if (bAutoCheckOut == true)
			{
				// Checked out by other.. fail :(
				if( SourceControlState->IsCheckedOutOther() )
				{
					UE_LOG(LogPackageUtilities, Warning, TEXT("Package (%s) checked out by other, skipping."), *PackageFileName);
					PackagesThatCouldNotBeSavedList.Add( PackageFileName );
					return;
				}
				// Package not at head revision
				else if ( !SourceControlState->IsCurrent() )
				{
					UE_LOG(LogPackageUtilities, Warning, TEXT("Package (%s) is not at head revision, skipping."), *PackageFileName );
					PackagesThatCouldNotBeSavedList.Add( PackageFileName );
					return;
				}
				// Package marked for delete
				else if ( SourceControlState->IsDeleted() )
				{
					UE_LOG(LogPackageUtilities, Warning, TEXT("Package (%s) is marked for delete, skipping."), *PackageFileName );
					PackagesThatCouldNotBeSavedList.Add( PackageFileName );
					return;
				}
			}
			// not allowed to auto check out :(
			else
			{
				UE_LOG(LogPackageUtilities, Warning, TEXT("Package (%s) cannot be checked out. Switch AUTOCHECKOUTPACKAGES not set. Skip."), *PackageFileName);
				PackagesThatCouldNotBeSavedList.AddUnique( PackageFileName );
				return;
			}
		}

		if (bSkipCinematicPackages && (PackageFileName.Contains(TEXT("CINE"))))
		{
			UE_LOG(LogPackageUtilities, Warning, TEXT("Package (%s) name contains 'cine' and switch SKIPCINES is set. Skip."), *PackageFileName);
			PackagesThatCouldNotBeSavedList.AddUnique( PackageFileName );
			return;
		}

		// Get version number. Bump this up every time you want to recompress all animations.
		const int32 CompressCommandletVersion = UAnimationSettings::Get()->CompressCommandletVersion;

		int32 ActiveAnimationIndex = 0;
		for (TObjectIterator<OBJECTYPE> It; It; ++It)
		{
			OBJECTYPE* AnimSeq = *It;
			if (!AnimSeq->IsIn(Package))
			{
				continue;
			}

			++ActiveAnimationIndex;

			// If animation hasn't been compressed, force it.
			bool bForceCompression = (AnimSeq->CompressedTrackOffsets.Num() == 0);

			// If animation has already been compressed with the commandlet and version is the same. then skip.
			// We're only interested in new animations.
			if( !bAnalyze && !bForceCompression && AnimSeq->CompressCommandletVersion == CompressCommandletVersion )
			{
				UE_LOG(LogPackageUtilities, Warning, TEXT("Same CompressCommandletVersion (%i) skip animation: %s (%s)"), CompressCommandletVersion, *AnimSeq->GetName(), *AnimSeq->GetFullName());
				continue;
			}

			if( !bAnalyze && !bForceCompression && bSkipLongAnimations && (AnimSeq->NumFrames > 300) )
			{
				UE_LOG(LogPackageUtilities, Warning, TEXT("Animation (%s) has more than 300 frames (%i frames) and SKIPLONGANIMS switch is set. Skipping."), *AnimSeq->GetName(), AnimSeq->NumFrames);
				continue;
			}

			USkeleton* Skeleton = AnimSeq->GetSkeleton();
			check (Skeleton);
			if (Skeleton->HasAnyFlags(RF_NeedLoad))
			{
				Skeleton->GetLinker()->Preload(Skeleton);
			}

			if( bAnalyze )
			{
				static int32 NumTotalAnimations = 0;
				static int32 NumTotalSize = 0;
				static int32 Trans96Savings = 0;
				static int32 Trans48Savings = 0;
				static int32 Rot96Savings = 0;
				static int32 Rot48Savings = 0;
				static int32 Scale96Savings = 0;
				static int32 Scale48Savings = 0;
				static int32 Num96TransTracks = 0;
				static int32 Num96RotTracks = 0;
				static int32 Num96ScaleTracks = 0;
				static int32 Num48TransTracks = 0;
				static int32 Num48RotTracks = 0;
				static int32 Num48ScaleTracks = 0;
				static int32 Num32TransTracks = 0;
				static int32 Num32ScaleTracks = 0;
				static int32 UnknownTransTrack = 0;
				static int32 UnknownRotTrack = 0;
				static int32 UnknownScaleTrack = 0;
				static int32 RotationOnlySavings = 0;
				static int32 RotationOnlyManyKeys = 0;

				NumTotalAnimations++;

				FArchiveCountMem CountBytesSize( AnimSeq );
				int32 ResourceSize = CountBytesSize.GetNum();

				NumTotalSize += ResourceSize;
				
				// Looking for PerTrackCompression using 96bit translation compression.
				if( AnimSeq->KeyEncodingFormat == AKF_PerTrackCompression && AnimSeq->CompressedByteStream.Num() > 0 )
				{
					bool bCandidate = false;

					for (int32 i = 0; i<AnimSeq->GetCompressedTrackToSkeletonMapTable().Num(); i++)
 					{
 						const int32 TrackIndex = i;
						const int32 BoneTreeIndex = AnimSeq->GetCompressedTrackToSkeletonMapTable()[TrackIndex].BoneTreeIndex;
						const FName BoneTreeName = Skeleton->GetReferenceSkeleton().GetBoneName(BoneTreeIndex);

 						// Translation
						{
							// Use the CompressedTrackOffsets stream to find the data addresses
							const int32* RESTRICT TrackDataForTransKey = AnimSeq->CompressedTrackOffsets.GetData() + (TrackIndex * 2);
							const int32 TransKeysOffset = TrackDataForTransKey[0];
 							if( TransKeysOffset != INDEX_NONE )
 							{
								const uint8* RESTRICT TrackData = AnimSeq->CompressedByteStream.GetData() + TransKeysOffset + 4;
								const int32 Header = *((int32*)(AnimSeq->CompressedByteStream.GetData() + TransKeysOffset));
	 
 								int32 KeyFormat;
 								int32 NumKeys;
 								int32 FormatFlags;
 								int32 BytesPerKey;
 								int32 FixedBytes;
 								FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/BytesPerKey, /*OUT*/ FixedBytes);
 								if( KeyFormat == ACF_Float96NoW )
 								{
									Num96TransTracks++;

 									// Determine which components we could let go, and bytes we could save.
									const FBox KeyBounds((FVector*)(TrackData + FixedBytes), NumKeys);
									const bool bHasX = (FMath::Abs(KeyBounds.Max.X) >= 0.0002f) || (FMath::Abs(KeyBounds.Min.X) >= 0.0002f);
									const bool bHasY = (FMath::Abs(KeyBounds.Max.Y) >= 0.0002f) || (FMath::Abs(KeyBounds.Min.Y) >= 0.0002f);
									const bool bHasZ = (FMath::Abs(KeyBounds.Max.Z) >= 0.0002f) || (FMath::Abs(KeyBounds.Min.Z) >= 0.0002f);

									if( !bHasX )
									{
										Trans96Savings += (4 * NumKeys);
										bCandidate = true;
									}
									if( !bHasY )
									{
										Trans96Savings += (4 * NumKeys);
										bCandidate = true;
									}
									if( !bHasZ )
									{
										Trans96Savings += (4 * NumKeys);
										bCandidate = true;
									}
								}
								// Measure savings on 48bits translations
								else if( KeyFormat == ACF_Fixed48NoW )
								{
									Num48TransTracks++;

									const int32 SavedBytes = (6 - BytesPerKey) * NumKeys;
									if( SavedBytes > 0 )
									{
										bCandidate = true;
										Trans48Savings += SavedBytes;
									}
								}
								else if( KeyFormat == ACF_IntervalFixed32NoW )
								{
									Num32TransTracks++;
								}
								else
								{
									UnknownTransTrack++;
								}

								// Measure how much we'd save if we used "rotation only" for compression
								// root bone is true if BoneTreeIndex == 0 
								// @todoanim : @fixmelh : AnimRotationOnly fix
								if( BoneTreeIndex > 0 )
// 									&& ((AnimSet->UseTranslationBoneNames.Num() > 0 && AnimSet->UseTranslationBoneNames.FindItemIndex(BoneName) == INDEX_NONE) 
// 										|| (AnimSet->ForceMeshTranslationBoneNames.FindItemIndex(BoneName) != INDEX_NONE)) 
// 									)
								{
									RotationOnlySavings += (BytesPerKey * NumKeys);
									if( NumKeys > 1 )
									{
										const uint8* RESTRICT KeyData0 = TrackData + FixedBytes;
										FVector V0;
										FAnimationCompression_PerTrackUtils::DecompressTranslation(KeyFormat, FormatFlags, V0, TrackData, KeyData0);

										float MaxErrorFromFirst = 0.f;
										float MaxErrorFromDefault = 0.f;
										const TArray<FTransform> & LocalRefPoses = Skeleton->GetRefLocalPoses();
										for(int32 KeyIdx=0; KeyIdx<NumKeys; KeyIdx++)
										{
											const uint8* RESTRICT KeyDataN = TrackData + FixedBytes + KeyIdx * BytesPerKey;
											FVector VN;
											FAnimationCompression_PerTrackUtils::DecompressTranslation(KeyFormat, FormatFlags, VN, TrackData, KeyDataN);

											// @todoanim: we will need more discussion, but we might use Skeleton->RefLocalPoses for compression
											MaxErrorFromDefault = FMath::Max(MaxErrorFromDefault, FMath::Abs(VN.X - LocalRefPoses[BoneTreeIndex].GetLocation().X));
											MaxErrorFromDefault = FMath::Max(MaxErrorFromDefault, FMath::Abs(VN.Y - LocalRefPoses[BoneTreeIndex].GetLocation().Y));
											MaxErrorFromDefault = FMath::Max(MaxErrorFromDefault, FMath::Abs(VN.Z - LocalRefPoses[BoneTreeIndex].GetLocation().Z));

											MaxErrorFromFirst = FMath::Max(MaxErrorFromFirst, FMath::Abs(VN.X - V0.X));
											MaxErrorFromFirst = FMath::Max(MaxErrorFromFirst, FMath::Abs(VN.Y - V0.Y));
											MaxErrorFromFirst = FMath::Max(MaxErrorFromFirst, FMath::Abs(VN.Z - V0.Z));
										}

										UE_LOG(LogPackageUtilities, Warning, TEXT("RotationOnly translation track that is animated! %s, %s (%s) NumKeys: %i, MaxErrorFromDefault: %f, MaxErrorFromFirst: %f"), 
											*BoneTreeName.ToString(), *AnimSeq->GetName(), *AnimSeq->GetFullName(), NumKeys, MaxErrorFromDefault, MaxErrorFromFirst);
										RotationOnlyManyKeys += (BytesPerKey * (NumKeys-1));
									}
								}
 							}
						}

						// Rotation
						{
							// Use the CompressedTrackOffsets stream to find the data addresses
							const int32* RESTRICT TrackDataForRotKey = AnimSeq->CompressedTrackOffsets.GetData() + (TrackIndex * 2);
							const int32 RotKeysOffset = TrackDataForRotKey[1];
							if( RotKeysOffset != INDEX_NONE )
							{
								const uint8* RESTRICT TrackData = AnimSeq->CompressedByteStream.GetData() + RotKeysOffset + 4;
								const int32 Header = *((int32*)(AnimSeq->CompressedByteStream.GetData() + RotKeysOffset));

								int32 KeyFormat;
								int32 NumKeys;
								int32 FormatFlags;
								int32 BytesPerKey;
								int32 FixedBytes;
								FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/BytesPerKey, /*OUT*/ FixedBytes);
								if( KeyFormat == ACF_Float96NoW )
								{
									Num96RotTracks++;

									// Determine which components we could let go, and bytes we could save.
									const FBox KeyBounds((FVector*)(TrackData + FixedBytes), NumKeys);
									const bool bHasX = (FMath::Abs(KeyBounds.Max.X) >= 0.0002f) || (FMath::Abs(KeyBounds.Min.X) >= 0.0002f);
									const bool bHasY = (FMath::Abs(KeyBounds.Max.Y) >= 0.0002f) || (FMath::Abs(KeyBounds.Min.Y) >= 0.0002f);
									const bool bHasZ = (FMath::Abs(KeyBounds.Max.Z) >= 0.0002f) || (FMath::Abs(KeyBounds.Min.Z) >= 0.0002f);

									if( !bHasX )
									{
										Rot96Savings += (4 * NumKeys);
										bCandidate = true;
									}
									if( !bHasY )
									{
										Rot96Savings += (4 * NumKeys);
										bCandidate = true;
									}
									if( !bHasZ )
									{
										Rot96Savings += (4 * NumKeys);
										bCandidate = true;
									}
								}
								// Measure savings on 48bits rotations.
								else if( KeyFormat == ACF_Fixed48NoW )
								{
									Num48RotTracks++;

									const int32 SavedBytes = (6 - BytesPerKey) * NumKeys;
									if( SavedBytes > 0 )
									{
										bCandidate = true;
										Rot48Savings += SavedBytes;
									}
								}
								else
								{
									UnknownRotTrack++;
								}
							}
						}

						// Scale
						{
							// Use the CompressedTrackOffsets stream to find the data addresses
							const int32 ScaleKeysOffset = AnimSeq->CompressedScaleOffsets.GetOffsetData(TrackIndex, 0);
							if( ScaleKeysOffset != INDEX_NONE )
							{
								const uint8* RESTRICT TrackData = AnimSeq->CompressedByteStream.GetData() + ScaleKeysOffset + 4;
								const int32 Header = *((int32*)(AnimSeq->CompressedByteStream.GetData() + ScaleKeysOffset));

								int32 KeyFormat;
								int32 NumKeys;
								int32 FormatFlags;
								int32 BytesPerKey;
								int32 FixedBytes;
								FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/BytesPerKey, /*OUT*/ FixedBytes);
								if( KeyFormat == ACF_Float96NoW )
								{
									Num96ScaleTracks++;

									// Determine which components we could let go, and bytes we could save.
									const FBox KeyBounds((FVector*)(TrackData + FixedBytes), NumKeys);
									const bool bHasX = (FMath::Abs(KeyBounds.Max.X) >= 0.0002f) || (FMath::Abs(KeyBounds.Min.X) >= 0.0002f);
									const bool bHasY = (FMath::Abs(KeyBounds.Max.Y) >= 0.0002f) || (FMath::Abs(KeyBounds.Min.Y) >= 0.0002f);
									const bool bHasZ = (FMath::Abs(KeyBounds.Max.Z) >= 0.0002f) || (FMath::Abs(KeyBounds.Min.Z) >= 0.0002f);

									if( !bHasX )
									{
										Scale96Savings += (4 * NumKeys);
										bCandidate = true;
									}
									if( !bHasY )
									{
										Scale96Savings += (4 * NumKeys);
										bCandidate = true;
									}
									if( !bHasZ )
									{
										Scale96Savings += (4 * NumKeys);
										bCandidate = true;
									}
								}
								// Measure savings on 48bits Scales
								else if( KeyFormat == ACF_Fixed48NoW )
								{
									Num48ScaleTracks++;

									const int32 SavedBytes = (6 - BytesPerKey) * NumKeys;
									if( SavedBytes > 0 )
									{
										bCandidate = true;
										Scale48Savings += SavedBytes;
									}
								}
								else if( KeyFormat == ACF_IntervalFixed32NoW )
								{
									Num32ScaleTracks++;
								}
								else
								{
									UnknownScaleTrack++;
								}

								// Measure how much we'd save if we used "rotation only" for compression
								// root bone is true if BoneTreeIndex == 0 
								// @todoanim : @fixmelh : AnimRotationOnly fix
								if( BoneTreeIndex > 0 )
									// 									&& ((AnimSet->UseScaleBoneNames.Num() > 0 && AnimSet->UseScaleBoneNames.FindItemIndex(BoneName) == INDEX_NONE) 
										// 										|| (AnimSet->ForceMeshScaleBoneNames.FindItemIndex(BoneName) != INDEX_NONE)) 
											// 									)
								{
									RotationOnlySavings += (BytesPerKey * NumKeys);
									if( NumKeys > 1 )
									{
										const uint8* RESTRICT KeyData0 = TrackData + FixedBytes;
										FVector V0;
										FAnimationCompression_PerTrackUtils::DecompressScale(KeyFormat, FormatFlags, V0, TrackData, KeyData0);

										float MaxErrorFromFirst = 0.f;
										float MaxErrorFromDefault = 0.f;
										const TArray<FTransform> & LocalRefPoses = Skeleton->GetRefLocalPoses();
										for(int32 KeyIdx=0; KeyIdx<NumKeys; KeyIdx++)
										{
											const uint8* RESTRICT KeyDataN = TrackData + FixedBytes + KeyIdx * BytesPerKey;
											FVector VN;
											FAnimationCompression_PerTrackUtils::DecompressScale(KeyFormat, FormatFlags, VN, TrackData, KeyDataN);

											// @todoanim: we will need more discussion, but we might use Skeleton->RefLocalPoses for compression
											MaxErrorFromDefault = FMath::Max(MaxErrorFromDefault, FMath::Abs(VN.X - LocalRefPoses[BoneTreeIndex].GetLocation().X));
											MaxErrorFromDefault = FMath::Max(MaxErrorFromDefault, FMath::Abs(VN.Y - LocalRefPoses[BoneTreeIndex].GetLocation().Y));
											MaxErrorFromDefault = FMath::Max(MaxErrorFromDefault, FMath::Abs(VN.Z - LocalRefPoses[BoneTreeIndex].GetLocation().Z));

											MaxErrorFromFirst = FMath::Max(MaxErrorFromFirst, FMath::Abs(VN.X - V0.X));
											MaxErrorFromFirst = FMath::Max(MaxErrorFromFirst, FMath::Abs(VN.Y - V0.Y));
											MaxErrorFromFirst = FMath::Max(MaxErrorFromFirst, FMath::Abs(VN.Z - V0.Z));
										}

										UE_LOG(LogPackageUtilities, Warning, TEXT("RotationOnly Scale track that is animated! %s, %s (%s) NumKeys: %i, MaxErrorFromDefault: %f, MaxErrorFromFirst: %f"), 
											*BoneTreeName.ToString(), *AnimSeq->GetName(), *AnimSeq->GetFullName(), NumKeys, MaxErrorFromDefault, MaxErrorFromFirst);
										RotationOnlyManyKeys += (BytesPerKey * (NumKeys-1));
									}
								}
							}
						}
 					}

					if( bCandidate )
					{
						++AnalyzeCompressionCandidates;
						UE_LOG(LogPackageUtilities, Warning, TEXT("[%i] Animation could be recompressed: %s (%s), Trans96Savings: %i, Rot96Savings: %i, Scale96Savings: %i, Trans48Savings: %i, Rot48Savings: %i, Scale48Savings: %i, RotationOnlySavings: %i, RotationOnlyManyKeys: %i (bytes)"), 
							AnalyzeCompressionCandidates, *AnimSeq->GetName(), *AnimSeq->GetFullName(), Trans96Savings, Rot96Savings, Scale96Savings, Trans48Savings, Rot48Savings, Scale48Savings, RotationOnlySavings, RotationOnlyManyKeys);
						UE_LOG(LogPackageUtilities, Warning, TEXT("Translation Track Count, Num96TransTracks: %i, Num48TransTracks: %i, Num32TransTracks: %i, UnknownTransTrack: %i"), 
							Num96TransTracks, Num48TransTracks, Num32TransTracks, UnknownTransTrack);
						UE_LOG(LogPackageUtilities, Warning, TEXT("Rotation Track Count, Num96RotTracks: %i, Num48RotTracks: %i, UnknownRotTrack: %i"), 
							Num96RotTracks, Num48RotTracks, UnknownRotTrack);
						UE_LOG(LogPackageUtilities, Warning, TEXT("Scale Track Count, Num96ScaleTracks: %i, Num48ScaleTracks: %i, Num32ScaleTracks: %i, UnknownScaleTrack: %i"), 
							Num96ScaleTracks, Num48ScaleTracks, Num32ScaleTracks, UnknownScaleTrack);
					}
				}

// 				if( AnimSeq->NumFrames > 1 && AnimSeq->KeyEncodingFormat != AKF_PerTrackCompression )
// 				{
// 					++AnalyzeCompressionCandidates;
// 
// 					FArchiveCountMem CountBytesSize( AnimSeq );
// 					int32 ResourceSize = CountBytesSize.GetNum();
// 
// 					UE_LOG(LogPackageUtilities, Warning, TEXT("[%i] Animation could be recompressed: %s (%s), frames: %i, length: %f, size: %i bytes, compression scheme: %s"), 
// 						AnalyzeCompressionCandidates, *AnimSeq->GetName(), *AnimSet->GetFullName(),  AnimSeq->NumFrames, AnimSeq->SequenceLength, ResourceSize, AnimSeq->CompressionScheme ? *AnimSeq->CompressionScheme->GetClass()->GetName() : TEXT("NULL"));
// 				}

				continue;
			}

			float HighestRatio = 0.f;
#if 0 // @todoanim: not sure why we need this here
			USkeletalMesh*	BestSkeletalMeshMatch = NULL;

			// Test preview skeletal mesh
			USkeletalMesh* DefaultSkeletalMesh = LoadObject<USkeletalMesh>(NULL, *AnimSet->PreviewSkelMeshName.ToString(), NULL, LOAD_None, NULL);
			float DefaultMatchRatio = 0.f;
			if( DefaultSkeletalMesh )
			{
				DefaultMatchRatio = AnimSet->GetSkeletalMeshMatchRatio(DefaultSkeletalMesh);
			}

			// If our default mesh doesn't have a full match ratio, then see if we can find a better fit.
			if( DefaultMatchRatio < 1.f )
			{
				// Find the most suitable SkeletalMesh for this AnimSet
				for( TObjectIterator<USkeletalMesh> ItMesh; ItMesh; ++ItMesh )
				{
					USkeletalMesh* SkelMeshCandidate = *ItMesh;
					if( SkelMeshCandidate != DefaultSkeletalMesh )
					{
						float MatchRatio = AnimSet->GetSkeletalMeshMatchRatio(SkelMeshCandidate);
						if( MatchRatio > HighestRatio )
						{
							BestSkeletalMeshMatch = SkelMeshCandidate;
							HighestRatio = MatchRatio;

							// If we have found a perfect match, we can abort.
							if( FMath::Abs(1.f - MatchRatio) <= KINDA_SMALL_NUMBER )
							{
								break;
							}
						}
					}
				}

				// If we have found a best match
				if( BestSkeletalMeshMatch )
				{
					// if it is different than our preview mesh and his match ratio is higher
					// then replace preview mesh with this one, as it's a better match.
					if( BestSkeletalMeshMatch != DefaultSkeletalMesh && HighestRatio > DefaultMatchRatio )
					{
						UE_LOG(LogPackageUtilities, Warning, TEXT("Found more suitable preview mesh for %s (%s): %s (%f) instead of %s (%f)."), 
							*AnimSeq->GetName(), *AnimSet->GetFullName(), *BestSkeletalMeshMatch->GetFName().ToString(), HighestRatio, *AnimSet->PreviewSkelMeshName.ToString(), DefaultMatchRatio);

						// We'll now use this one from now on as it's a better fit.
						AnimSet->PreviewSkelMeshName = FName( *BestSkeletalMeshMatch->GetPathName() );
						AnimSet->MarkPackageDirty();

						DefaultSkeletalMesh = BestSkeletalMeshMatch;
						bDirtyPackage = true;
					}
				}
				else
				{
					UE_LOG(LogPackageUtilities, Warning, TEXT("Could not find suitable mesh for %s (%s) !!! Default was %s"), 
							*AnimSeq->GetName(), *AnimSet->GetFullName(), *AnimSet->PreviewSkelMeshName.ToString());
				}
			}
#endif
			SIZE_T OldSize;
			SIZE_T NewSize;

			{
				OldSize = AnimSeq->GetResourceSizeBytes(EResourceSizeMode::Inclusive);
			}

			// Clear bDoNotOverrideCompression flag
			if( bClearNoCompressionOverride && AnimSeq->bDoNotOverrideCompression )
			{
				AnimSeq->bDoNotOverrideCompression = false;
				bDirtyPackage = true;
			}

			// Reset to default compressor
			if( bResetCompression )
			{
				UE_LOG(LogPackageUtilities, Warning, TEXT("%s (%s) Resetting with BitwiseCompressOnly."), *AnimSeq->GetName(), *AnimSeq->GetFullName());
				UAnimCompress* CompressionAlgorithm = NewObject<UAnimCompress_BitwiseCompressOnly>();
				CompressionAlgorithm->RotationCompressionFormat = ACF_Float96NoW;
				CompressionAlgorithm->TranslationCompressionFormat = ACF_None;
				CompressionAlgorithm->ScaleCompressionFormat = ACF_Float96NoW;
				AnimSeq->CompressionScheme = CompressionAlgorithm;
				AnimSeq->RequestSyncAnimRecompression();

				// Force an update.
				AnimSeq->CompressCommandletVersion = 0;
			}
			
			// Do not perform automatic recompression on animations marked as 'bDoNotOverrideCompression'
			// Unless they have no compression scheme, or they're using automatic compression.
			if (AnimSeq->bDoNotOverrideCompression && (AnimSeq->CompressionScheme != nullptr) && !AnimSeq->CompressionScheme->IsA(UAnimCompress_Automatic::StaticClass()))
			{
				continue;
			}

			UE_LOG(LogPackageUtilities, Warning, TEXT("Compressing animation '%s' [#%d / %d in package '%s']"),
				*AnimSeq->GetName(),
				ActiveAnimationIndex,
				NumAnimationsInPackage,
				*PackageFileName);

			// First set automatic compressor and call it.
			// This will run through a bunch of compressors and pick the best.
			// Problem is this is going to create a DDC key with 'Automatic Compressor'
			UAnimCompress* CompressionAlgorithm = NewObject<UAnimCompress_Automatic>();
			AnimSeq->CompressionScheme = static_cast<UAnimCompress*>(StaticDuplicateObject(CompressionAlgorithm, AnimSeq));
			AnimSeq->RequestAnimCompression(false, true, false);

			// Automatic compression should have picked a suitable compressor that is not UAnimCompress_Automatic
			if (ensure(!AnimSeq->CompressionScheme->IsA(UAnimCompress_Automatic::StaticClass())))
			{
				// Update CompressCommandletVersion in that case, and create a proper DDC entry
				// (with actual compressor)
				AnimSeq->CompressCommandletVersion = CompressCommandletVersion;
				AnimSeq->RequestAnimCompression(false, false, false);
				bDirtyPackage = true;
			}

			{
				NewSize = AnimSeq->GetResourceSizeBytes(EResourceSizeMode::Inclusive);
			}

			// Only save package if size has changed.
			const int64 DeltaSize = NewSize - OldSize;
			bDirtyPackage = (bDirtyPackage || bForceCompression || (DeltaSize != 0));

			// if Dirty, then we need to be able to write to this package. 
			// If we can't, abort, don't want to waste time!!
			if( bDirtyPackage )
			{
				// Save dirty package every 10 minutes at least, to avoid losing work in case of a crash on very large packages.
				float const CurrentTime = FPlatformTime::Seconds();
				UE_LOG(LogPackageUtilities, Warning, TEXT("Time since last save: %f seconds"), (CurrentTime - LastSaveTime) );
				if( (CurrentTime - LastSaveTime) > 10.f * 60.f )
				{
					UE_LOG(LogPackageUtilities, Warning, TEXT("It's been over 10 minutes (%f seconds), try to save package."), (CurrentTime - LastSaveTime) );
					bool bCorrectlySaved = false;

					SourceControlState = SourceControl.GetProvider().GetState(Package, EStateCacheUsage::ForceUpdate);
					if( SourceControlState.IsValid() && SourceControlState->CanCheckout() && bAutoCheckOut )
					{
						SourceControl.GetProvider().Execute(ISourceControlOperation::Create<FCheckOut>(), Package);
					}

					SourceControlState = SourceControl.GetProvider().GetState(Package, EStateCacheUsage::ForceUpdate);
					if( !SourceControlState.IsValid() || SourceControlState->CanEdit() )
					{
						if( SavePackageHelper( Package, PackageFileName ) == true )
						{
							bCorrectlySaved = true;
							UE_LOG(LogPackageUtilities, Warning, TEXT("Correctly saved:  [%s]."), *PackageFileName );
						}
						else
						{
							UE_LOG(LogPackageUtilities, Error, TEXT("Error saving [%s]"), *PackageFileName );
						}
					}

					// Log which packages could not be saved
					if( !bCorrectlySaved )
					{
						PackagesThatCouldNotBeSavedList.AddUnique( PackageFileName );
						UE_LOG(LogPackageUtilities, Warning, TEXT("%s couldn't be saved, so abort this package, don't waste time on it."), *PackageFileName );
						// Abort!
						return;
					}

					// Correctly saved
					LastSaveTime = CurrentTime;
					bDirtyPackage = false;
				}
			}
		}

		// End of recompression
		// Does package need to be saved?
/*		bDirtyPackage = bDirtyPackage || Package->IsDirty();*/

		// If we need to save package, do so.
		if( bDirtyPackage && !bAnalyze )
		{
			bool bCorrectlySaved = false;

			// see if we should skip read only packages.
			bool bIsReadOnly = IFileManager::Get().IsReadOnly( *PackageFileName);

			// check to see if we need to check this package out
			SourceControlState = SourceControl.GetProvider().GetState(Package, EStateCacheUsage::ForceUpdate);
			if(	SourceControlState.IsValid() && SourceControlState->CanCheckout() && bAutoCheckOut == true )
			{
				SourceControl.GetProvider().Execute(ISourceControlOperation::Create<FCheckOut>(), Package);
			}

			SourceControlState = SourceControl.GetProvider().GetState(Package, EStateCacheUsage::ForceUpdate);
			if( !SourceControlState.IsValid() || SourceControlState->CanEdit() )
			{
				if( SavePackageHelper( Package, PackageFileName ) == true )
				{
					bCorrectlySaved = true;
					UE_LOG(LogPackageUtilities, Warning, TEXT("Correctly saved:  [%s]."), *PackageFileName );
				}
				else
				{
					UE_LOG(LogPackageUtilities, Warning, TEXT("Error saving [%s]"), *PackageFileName );
				}
			}

			// Log which packages could not be saved
			if( !bCorrectlySaved )
			{
				PackagesThatCouldNotBeSavedList.AddUnique( PackageFileName );
			}
		}
	}
};

UCompressAnimationsCommandlet::UCompressAnimationsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LogToConsole = false;
}


int32 UCompressAnimationsCommandlet::Main( const FString& Params )
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	// want everything in upper case, it's a mess otherwise
	const FString ParamsUpperCase = Params.ToUpper();
	const TCHAR* Parms = *ParamsUpperCase;
	UCommandlet::ParseCommandLine(Parms, Tokens, Switches);

	/** If we're analyzing, we're not actually going to recompress, so we can skip some significant work. */
	bool bAnalyze = Switches.Contains(TEXT("ANALYZE"));

	if (bAnalyze)
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("Analyzing content for uncompressed animations..."));
		DoActionToAllPackages<UAnimSequence, CompressAnimationsFunctor>(this, ParamsUpperCase);

		UE_LOG(LogPackageUtilities, Warning, TEXT("Done analyzing. Potential canditates: %i"), AnalyzeCompressionCandidates);
	}
	else
	{
		// Then do the animation recompression
		UE_LOG(LogPackageUtilities, Warning, TEXT("Recompressing all animations..."));
		DoActionToAllPackages<UAnimSequence, CompressAnimationsFunctor>(this, ParamsUpperCase);

		UE_LOG(LogPackageUtilities, Warning, TEXT("\n*** Packages that could not be recompressed: %i"), PackagesThatCouldNotBeSavedList.Num());
		for(int32 i=0; i<PackagesThatCouldNotBeSavedList.Num(); i++)
		{
			UE_LOG(LogPackageUtilities, Warning, TEXT("\t%s"), *PackagesThatCouldNotBeSavedList[i]);
		}
	}
	

	return 0;
}

//======================================================================
// UReplaceActorCommandlet
//======================================================================
UReplaceActorCommandlet::UReplaceActorCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LogToConsole = false;
}

int32 UReplaceActorCommandlet::Main(const FString& Params)
{
	const TCHAR* Parms = *Params;

// 	// get the specified filename/wildcard
// 	FString PackageWildcard;
// 	if (!FParse::Token(Parms, PackageWildcard, 0))
// 	{
// 		UE_LOG(LogPackageUtilities, Warning, TEXT("Syntax: replaceactor <file/wildcard> <Package.Class to remove> <Package.Class to replace with>"));
// 		return 1;
// 	}

	// find all the files matching the specified filename/wildcard
// 	TArray<FString> FilesInPath;
// 	IFileManager::Get().FindFiles(FilesInPath, *PackageWildcard, 1, 0);
// 	if (FilesInPath.Num() == 0)
// 	{
// 		UE_LOG(LogPackageUtilities, Error, TEXT("No packages found matching %s!"), *PackageWildcard);
// 		return 2;
// 	}

	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList;

	FString PackageWildcard;
	FString PackagePrefix;
// 	if(FParse::Token(Parms,PackageWildcard,false))
// 	{
// 		IFileManager::Get().FindFiles(PackageList,*PackageWildcard,true,false);
// 		PackagePrefix = FPaths::GetPath(PackageWildcard) * TEXT("");
// 	}
// 	else
// 	{
		FEditorFileUtils::FindAllPackageFiles(PackageList);
//	}
	if( !PackageList.Num() )
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT( "Found no packages to run UReplaceActorCommandlet on!" ) );
		return 0;
	}

	// get the directory part of the filename
	int32 ChopPoint = FMath::Max(PackageWildcard.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1, PackageWildcard.Find(TEXT("\\"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1);
	if (ChopPoint < 0)
	{
		ChopPoint = PackageWildcard.Find( TEXT("*"), ESearchCase::CaseSensitive, ESearchDir::FromEnd );
	}

	FString PathPrefix = (ChopPoint < 0) ? TEXT("") : PackageWildcard.Left(ChopPoint);

	// get the class to remove and the class to replace it with
	FString ClassName;
	if (!FParse::Token(Parms, ClassName, 0))
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("Syntax: replaceactor <file/wildcard> <Package.Class to remove> <Package.Class to replace with>"));
		return 1;
	}

	UClass* ClassToReplace = (UClass*)StaticLoadObject(UClass::StaticClass(), NULL, *ClassName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
	if (ClassToReplace == NULL)
	{
		UE_LOG(LogPackageUtilities, Error, TEXT("Invalid class to remove: %s"), *ClassName);
		return 4;
	}
	else
	{
		ClassToReplace->AddToRoot();
	}

	if (!FParse::Token(Parms, ClassName, 0))
	{
		UE_LOG(LogPackageUtilities, Warning, TEXT("Syntax: replaceactor <file/wildcard> <Package.Class to remove> <Package.Class to replace with>"));
		return 1;
	}

	UClass* ReplaceWithClass = (UClass*)StaticLoadObject(UClass::StaticClass(), NULL, *ClassName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
	if (ReplaceWithClass == NULL)
	{
		UE_LOG(LogPackageUtilities, Error, TEXT("Invalid class to replace with: %s"), *ClassName);
		return 5;
	}
	else
	{
		ReplaceWithClass->AddToRoot();
	}

	// find the most derived superclass common to both classes
	UClass* CommonSuperclass = NULL;
	for (UClass* BaseClass1 = ClassToReplace; BaseClass1 != NULL && CommonSuperclass == NULL; BaseClass1 = BaseClass1->GetSuperClass())
	{
		for (UClass* BaseClass2 = ReplaceWithClass; BaseClass2 != NULL && CommonSuperclass == NULL; BaseClass2 = BaseClass2->GetSuperClass())
		{
			if (BaseClass1 == BaseClass2)
			{
				CommonSuperclass = BaseClass1;
			}
		}
	}
	checkSlow(CommonSuperclass != NULL);

	const bool bAutoCheckOut = FParse::Param(*Params,TEXT("AutoCheckOutPackages"));
	
	// Ensure source control is initialized and shut down properly
	FScopedSourceControl SourceControl;

	for (int32 i = 0; i < PackageList.Num(); i++)
	{
		const FString& PackageName = PackageList[i];
		// get the full path name to the file
		FString FileName = PathPrefix + PackageName;

		const bool bIsAutoSave = FileName.Contains( TEXT("AUTOSAVES") );

		FSourceControlStatePtr SourceControlState = SourceControl.GetProvider().GetState(FileName, EStateCacheUsage::ForceUpdate);

		// skip if read-only
  		if( !bAutoCheckOut && SourceControlState.IsValid() && SourceControlState->CanCheckout() )
  		{
  			UE_LOG(LogPackageUtilities, Warning, TEXT("Skipping %s: the file can be checked out, but auto check out is disabled"), *FileName);
  			continue;
 		}
 		else if(bIsAutoSave)
		{
			UE_LOG(LogPackageUtilities, Warning, TEXT("Skipping %s (non map)"), *FileName);
			continue;
		}
		else if ( bAutoCheckOut && SourceControlState.IsValid() && !SourceControlState->IsCurrent() )
		{
			UE_LOG(LogPackageUtilities, Warning, TEXT("Skipping %s (Not at head source control revision)"), *PackageName );
			continue;
		}
		else
		{
			UWorld* World = GWorld;
			// clean up any previous world
			if (World != NULL)
			{
				World->CleanupWorld();
				World->RemoveFromRoot();
			}

			// load the package
			UE_LOG(LogPackageUtilities, Warning, TEXT("Loading %s..."), *FileName); 
			UPackage* Package = LoadPackage(NULL, *FileName, LOAD_None);

			// load the world we're interested in
			World = UWorld::FindWorldInPackage(Package);

			// this is the case where .uasset objects have class references (e.g. prefabs, animnodes, etc)
			if( World == NULL )
			{
				UE_LOG(LogPackageUtilities, Warning, TEXT("%s (not a map)"), *FileName);
				for( FObjectIterator It; It; ++It )
				{
					UObject* OldObject = *It;
					if( ( OldObject->GetOutermost() == Package )
						)
					{
						TMap<UClass*, UClass*> ReplaceMap;
						ReplaceMap.Add(ClassToReplace, ReplaceWithClass);
						FArchiveReplaceObjectRef<UClass> ReplaceAr(OldObject, ReplaceMap, false, false, false);
						if( ReplaceAr.GetCount() > 0 )
						{
							UE_LOG(LogPackageUtilities, Warning, TEXT("Replaced %i class references in an Object: %s"), ReplaceAr.GetCount(), *OldObject->GetName() );
							Package->MarkPackageDirty();
						}
					}
				}

				if( Package->IsDirty() == true )
				{
					if( SourceControlState.IsValid() && SourceControlState->CanCheckout() && bAutoCheckOut == true )
					{
						SourceControl.GetProvider().Execute(ISourceControlOperation::Create<FCheckOut>(), Package);
					}

					UE_LOG(LogPackageUtilities, Warning, TEXT("Saving %s..."), *FileName);
					GEditor->SavePackage( Package, NULL, RF_Standalone, *FileName, GWarn );
				}
			}
			else
			{
				// We shouldnt need this - but just in case
				GWorld = World;
				// need to have a bool so we dont' save every single map
				bool bIsDirty = false;

				World->WorldType = EWorldType::Editor;

				// add the world to the root set so that the garbage collection to delete replaced actors doesn't garbage collect the whole world
				World->AddToRoot();
				// initialize the levels in the world
				World->InitWorld(UWorld::InitializationValues().AllowAudioPlayback(false));
				World->GetWorldSettings()->PostEditChange();
				World->UpdateWorldComponents( true, false );

				// iterate through all the actors in the world, looking for matches with the class to replace (must have exact match, not subclass)
				for (TActorIterator<AActor> It(World, ClassToReplace); It; ++It)
				{
					AActor* OldActor = *It;
					if (OldActor->GetClass() == ClassToReplace)
					{
						// replace an instance of the old actor
						UE_LOG(LogPackageUtilities, Warning, TEXT("Replacing actor %s"), *OldActor->GetName());
						bIsDirty = true;
						// make sure we spawn the new actor in the same level as the old
						//@warning: this relies on the outer of an actor being the level
						FVector OldLocation = OldActor->GetActorLocation();

						FRotator OldRotator = OldActor->GetActorRotation();

						// Cache the level this actor is in.
						ULevel* Level = OldActor->GetLevel();
						// destroy the old actor, which removes it from the array but doesn't destroy it until GC
						OldActor->Destroy();

						FActorSpawnParameters SpawnInfo;
						SpawnInfo.OverrideLevel = Level;
						SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						// spawn the new actor
						AActor* NewActor = World->SpawnActor<AActor>( ReplaceWithClass, OldLocation, OldRotator, SpawnInfo );

						// copy non-native non-transient properties common to both that were modified in the old actor to the new actor
						for (UProperty* Property = CommonSuperclass->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
						{
							if ( !(Property->PropertyFlags & CPF_Transient) &&
								!(Property->PropertyFlags & (CPF_InstancedReference | CPF_ContainsInstancedReference)) &&
								!Property->Identical_InContainer(OldActor, OldActor->GetClass()->GetDefaultObject()) )
							{
								Property->CopyCompleteValue_InContainer(NewActor, OldActor);
								Package->MarkPackageDirty();
							}
						}

						if (ClassToReplace->IsChildOf(AWorldSettings::StaticClass()))
						{
							Level->SetWorldSettings(CastChecked<AWorldSettings>(NewActor));
						}
						check(OldActor->IsValidLowLevel()); // make sure DestroyActor() doesn't immediately trigger GC since that'll break the reference replacement
						// check for any references to the old Actor and replace them with the new one
						TMap<AActor*, AActor*> ReplaceMap;
						ReplaceMap.Add(OldActor, NewActor);
						FArchiveReplaceObjectRef<AActor> ReplaceAr(World, ReplaceMap, false, false, false);
						if (ReplaceAr.GetCount() > 0)
						{
							UE_LOG(LogPackageUtilities, Warning, TEXT("Replaced %i actor references in %s"), ReplaceAr.GetCount(), *It->GetName());
							Package->MarkPackageDirty();
						}
					}
					else
					{
						// check for any references to the old class and replace them with the new one
						TMap<UClass*, UClass*> ReplaceMap;
						ReplaceMap.Add(ClassToReplace, ReplaceWithClass);
						FArchiveReplaceObjectRef<UClass> ReplaceAr(*It, ReplaceMap, false, false, false);
						if (ReplaceAr.GetCount() > 0)
						{
							UE_LOG(LogPackageUtilities, Warning, TEXT("Replaced %i class references in actor %s"), ReplaceAr.GetCount(), *It->GetName());
							Package->MarkPackageDirty();
							bIsDirty = true;
						}
					}
				}

				// collect garbage to delete replaced actors and any objects only referenced by them (components, etc)
				GEngine->PerformGarbageCollectionAndCleanupActors();

				// save the world
				if( ( Package->IsDirty() == true ) && ( bIsDirty == true ) )
				{
					SourceControlState = SourceControl.GetProvider().GetState(FileName, EStateCacheUsage::ForceUpdate);
					if( SourceControlState.IsValid() && SourceControlState->CanCheckout() && bAutoCheckOut == true )
					{
						SourceControl.GetProvider().Execute(ISourceControlOperation::Create<FCheckOut>(), Package);
					}

					UE_LOG(LogPackageUtilities, Warning, TEXT("Saving %s..."), *FileName);
					GEditor->SavePackage(Package, World, RF_NoFlags, *FileName, GWarn);
				}

				// clear GWorld by removing it from the root set and replacing it with a new one
				World->CleanupWorld();
				World->RemoveFromRoot();
				World = GWorld = NULL;
			}
		}

		// get rid of the loaded world
		UE_LOG(LogPackageUtilities, Warning, TEXT("GCing..."));
		CollectGarbage(RF_NoFlags);
	}

	// UEditorEngine::FinishDestroy() expects GWorld to exist
	if( GWorld )
	{
		GWorld->DestroyWorld( false );
	}
	GWorld = UWorld::CreateWorld(EWorldType::Editor, false );
	return 0;
}
