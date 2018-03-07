// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UCContentCommandlets.cpp: Various commmandlets.
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "Misc/PackageName.h"
#include "Misc/EngineVersion.h"
#include "Misc/StartupPackages.h"
#include "Misc/RedirectCollector.h"
#include "Engine/EngineTypes.h"
#include "Materials/Material.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "SourceControlHelpers.h"
#include "ISourceControlModule.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Commandlets/ListMaterialsUsedWithMeshEmittersCommandlet.h"
#include "Commandlets/ListStaticMeshesImportedFromSpeedTreesCommandlet.h"
#include "Particles/ParticleSystem.h"
#include "Commandlets/ResavePackagesCommandlet.h"
#include "Commandlets/WrangleContentCommandlet.h"
#include "EngineGlobals.h"
#include "Particles/ParticleEmitter.h"
#include "Engine/StaticMesh.h"
#include "AssetData.h"
#include "Engine/Brush.h"
#include "Editor.h"
#include "FileHelpers.h"

#include "PackageHelperFunctions.h"
#include "PackageTools.h"

DEFINE_LOG_CATEGORY(LogContentCommandlet);

#include "AssetRegistryModule.h"
#include "IDirectoryWatcher.h"
#include "DirectoryWatcherModule.h"
#include "Particles/Material/ParticleModuleMeshMaterial.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Engine/LevelStreaming.h"
#include "EditorBuildUtils.h"

// for UResavePackagesCommandlet::PerformAdditionalOperations building lighting code
#include "LightingBuildOptions.h"
// For preloading FFindInBlueprintSearchManager
#include "FindInBlueprintManager.h"

/**-----------------------------------------------------------------------------
 *	UResavePackages commandlet.
 *
 * This commandlet is meant to resave packages as a default.  We are able to pass in
 * flags to determine which conditions we do NOT want to resave packages. (e.g. not dirty
 * or not older than some version)
 *
 *
----------------------------------------------------------------------------**/

#define CURRENT_PACKAGE_VERSION 0
#define IGNORE_PACKAGE_VERSION INDEX_NONE

UResavePackagesCommandlet::UResavePackagesCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


int32 UResavePackagesCommandlet::InitializeResaveParameters( const TArray<FString>& Tokens, TArray<FString>& PackageNames )
{
	Verbosity = VERY_VERBOSE;

	TArray<FString> Unused;
	bool bExplicitPackages = false;

	// Check to see if we have an explicit list of packages
	for( int32 SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++ )
	{
		FString Package;
		FString PackageFolder;
		FString Maps;
		FString File;
		const FString& CurrentSwitch = Switches[ SwitchIdx ];
		if( FParse::Value( *CurrentSwitch, TEXT( "PACKAGE="), Package ) )
		{
			FString PackageFile;
			FPackageName::SearchForPackageOnDisk( Package, NULL, &PackageFile );
			PackageNames.Add( *PackageFile );
			bExplicitPackages = true;
		}
		else if( FParse::Value( *CurrentSwitch, TEXT( "PACKAGEFOLDER="), PackageFolder ) )
        {
            TArray<FString> FilesInPackageFolder;
            FPackageName::FindPackagesInDirectory(FilesInPackageFolder, *PackageFolder);
            for( int32 FileIndex = 0; FileIndex < FilesInPackageFolder.Num(); FileIndex++ )
            {
				FString PackageFile(FilesInPackageFolder[FileIndex]);
				FPaths::MakeStandardFilename(PackageFile);
				PackageNames.Add( *PackageFile );
            }
			bExplicitPackages = true;
		}
		else if (FParse::Value(*CurrentSwitch, TEXT("MAP="), Maps))
		{
			// Allow support for -MAP=Value1+Value2+Value3
			for (int32 PlusIdx = Maps.Find(TEXT("+")); PlusIdx != INDEX_NONE; PlusIdx = Maps.Find(TEXT("+")))
			{
				const FString NextMap = Maps.Left(PlusIdx);
				if (NextMap.Len() > 0)
				{
					FString MapFile;
					FPackageName::SearchForPackageOnDisk(NextMap, NULL, &MapFile);
					PackageNames.Add(*MapFile);
					bExplicitPackages = true;
				}

				Maps = Maps.Right(Maps.Len() - (PlusIdx + 1));
			}
			FString MapFile;
			FPackageName::SearchForPackageOnDisk(Maps, NULL, &MapFile);
			PackageNames.Add(*MapFile);
			bExplicitPackages = true;
		}
		else if (FParse::Value(*CurrentSwitch, TEXT("FILE="), File))
		{
			FString Text;
			if (FFileHelper::LoadFileToString(Text, *File))
			{
				TArray<FString> Lines;

				// Remove all carriage return characters.
				Text.ReplaceInline(TEXT("\r"), TEXT(""));
				// Read all lines
				Text.ParseIntoArray(Lines, TEXT("\n"), true);

				for (auto Line : Lines)
				{
					FString PackageFile;
					if (FPackageName::SearchForPackageOnDisk(Line, NULL, &PackageFile))
					{
						PackageNames.AddUnique(*PackageFile);
					}
					else
					{
						UE_LOG(LogContentCommandlet, Error, TEXT("Failed to find package %s"), *Line);
					}
				}

				bExplicitPackages = true;
				UE_LOG(LogContentCommandlet, Display, TEXT("Loaded %d Packages from %s"), PackageNames.Num(), *File);
			}
			else
			{
				UE_LOG(LogContentCommandlet, Error, TEXT("Failed to load file %s"), *File);
			}
		}

	}

	if (bShouldBuildLighting && !bExplicitPackages)
	{
		UE_LOG(LogContentCommandlet, Display, TEXT("No maps found to save when building lighting, checking CommandletSettings:ResavePackages in EditorIni"));
		// if we haven't specified any maps and we are building lighting check if there are packages setup in the ini file to build
		TArray<FString> ResavePackages;
		GConfig->GetArray(TEXT("CommandletSettings"), TEXT("ResavePackages"), ResavePackages, GEditorIni);
		for ( const auto& ResavePackage : ResavePackages )
		{
			FString PackageFile;
			FPackageName::SearchForPackageOnDisk(ResavePackage, NULL, &PackageFile);
			UE_LOG(LogContentCommandlet, Display, TEXT("Rebuilding lighting for package %s"), *PackageFile);
			PackageNames.Add(*PackageFile);
			bExplicitPackages = true;
		}
	}

	// ... if not, load in all packages
	if( !bExplicitPackages )
	{
		uint8 PackageFilter = NORMALIZE_DefaultFlags;
		if ( Switches.Contains(TEXT("SKIPMAPS")) )
		{
			PackageFilter |= NORMALIZE_ExcludeMapPackages;
		}
		else if ( Switches.Contains(TEXT("MAPSONLY")) )
		{
			PackageFilter |= NORMALIZE_ExcludeContentPackages;
		}

		if (Switches.Contains(TEXT("PROJECTONLY")))
		{
			PackageFilter |= NORMALIZE_ExcludeEnginePackages;
		}

		if ( Switches.Contains(TEXT("SkipDeveloperFolders")) || Switches.Contains(TEXT("NODEV")) )
		{
			PackageFilter |= NORMALIZE_ExcludeDeveloperPackages;
		}
		else if ( Switches.Contains(TEXT("OnlyDeveloperFolders")) )
		{
			PackageFilter |= NORMALIZE_ExcludeNonDeveloperPackages;
		}

		bool bAnyFound = NormalizePackageNames(Unused, PackageNames, *FString::Printf(TEXT("*%s"), *FPackageName::GetAssetPackageExtension()), 
											   PackageFilter);
		bAnyFound = NormalizePackageNames(Unused, PackageNames, *FString::Printf(TEXT("*%s"), *FPackageName::GetMapPackageExtension()), PackageFilter) || bAnyFound;
		
		if (!bAnyFound)
		{
			return 1;
		}
	}

	// Check for a max package limit
	MaxPackagesToResave = -1;
	for ( int32 SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++ )
	{
		const FString& CurrentSwitch = Switches[SwitchIdx];
		if( FParse::Value(*CurrentSwitch,TEXT("MAXPACKAGESTORESAVE="),MaxPackagesToResave))
		{
			UE_LOG(LogContentCommandlet, Warning, TEXT( "Only resaving a maximum of %d packages." ), MaxPackagesToResave );
			break;
		}
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// This option works if a single package is specified, it will resave all packages that reference it, and all packages that it references
	const bool bResaveDirectRefsAndDeps = Switches.Contains(TEXT("ResaveDirectRefsAndDeps"));

	// This option will filter the package list and only save packages that are redirectors, or that reference redirectors
	const bool bFixupRedirects = (Switches.Contains(TEXT("FixupRedirects")) || Switches.Contains(TEXT("FixupRedirectors")));

	if (bResaveDirectRefsAndDeps || bFixupRedirects)
	{
		AssetRegistry.SearchAllAssets(true);

		// Force directory watcher tick to register paths
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		DirectoryWatcherModule.Get()->Tick(-1.0f);
	}

	if (bExplicitPackages && PackageNames.Num() == 1 && bResaveDirectRefsAndDeps)
	{
		FName PackageName = FName(*FPackageName::FilenameToLongPackageName(PackageNames[0]));

		TArray<FName> Referencers;
		AssetRegistry.GetReferencers(PackageName, Referencers);
		TArray<FName> Dependencies;
		AssetRegistry.GetDependencies(PackageName, Dependencies);

		for (FName Ref : Referencers)
		{
			FString File;
			FPackageName::SearchForPackageOnDisk(*Ref.ToString(), NULL, &File);
			PackageNames.Add(File);
		}
		for (FName Dep : Dependencies)
		{
			FString File;
			FPackageName::SearchForPackageOnDisk(*Dep.ToString(), NULL, &File);
			PackageNames.Add(File);
		}
	}
	else if (bFixupRedirects)
	{
		// Look for all packages containing redirects, and their referencers
		TArray<FAssetData> RedirectAssets;
		TSet<FString> RedirectPackages;
		TSet<FString> ReferencerPackages;

		AssetRegistry.GetAssetsByClass(UObjectRedirector::StaticClass()->GetFName(), RedirectAssets);

		for (const FAssetData& AssetData : RedirectAssets)
		{
			bool bIsAlreadyInSet = false;
			FString RedirectFile;
			FPackageName::SearchForPackageOnDisk(*AssetData.PackageName.ToString(), nullptr, &RedirectFile);

			RedirectPackages.Add(RedirectFile, &bIsAlreadyInSet);

			if (!bIsAlreadyInSet)
			{
				TArray<FName> Referencers;
				AssetRegistry.GetReferencers(AssetData.PackageName, Referencers);

				for (FName Referencer : Referencers)
				{
					FString ReferencerFile;
					FPackageName::SearchForPackageOnDisk(*Referencer.ToString(), nullptr, &ReferencerFile);

					ReferencerPackages.Add(ReferencerFile);
				}
			}
		}

		// Filter packagenames list to packages that are pointing to redirectors, it will probably be much smaller
		TArray<FString> OldArray = PackageNames;
		PackageNames.Reset();
		for (FString& PackageName : OldArray)
		{
			if (RedirectPackages.Contains(PackageName))
			{
				RedirectorsToFixup.Add(PackageName);
			}

			if (ReferencerPackages.Contains(PackageName))
			{
				PackageNames.Add(PackageName);
			}
		}
	}

	// Check for the min and max versions
	MinResaveUE4Version = IGNORE_PACKAGE_VERSION;
	MaxResaveUE4Version = IGNORE_PACKAGE_VERSION;
	MaxResaveLicenseeUE4Version = IGNORE_PACKAGE_VERSION;
	if ( Switches.Contains(TEXT("CHECKLICENSEEVER")) )
	{
		// Limits resaving to packages with this licensee package version or lower.
		MaxResaveLicenseeUE4Version = FMath::Max<int32>(GPackageFileLicenseeUE4Version - 1, 0);
	}
	if ( Switches.Contains(TEXT("CHECKUE4VER")) )
	{
		// Limits resaving to packages with this ue4 package version or lower.
		MaxResaveUE4Version = FMath::Max<int32>(GPackageFileUE4Version - 1, 0);
	}
	else if ( Switches.Contains(TEXT("RESAVEDEPRECATED")) )
	{
		// Limits resaving to packages with this package version or lower.
		MaxResaveUE4Version = FMath::Max<int32>(VER_UE4_DEPRECATED_PACKAGE - 1, 0);
	}
	else
	{
		// determine if the resave operation should be constrained to certain package versions
		for ( int32 SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++ )
		{
			const FString& CurrentSwitch = Switches[SwitchIdx];
			if ( MinResaveUE4Version == IGNORE_PACKAGE_VERSION && FParse::Value(*CurrentSwitch,TEXT("MINVER="),MinResaveUE4Version) )
			{
				if ( MinResaveUE4Version == CURRENT_PACKAGE_VERSION )
				{
					MinResaveUE4Version = GPackageFileUE4Version;
				}
			}

			if ( MaxResaveUE4Version == IGNORE_PACKAGE_VERSION && FParse::Value(*CurrentSwitch,TEXT("MAXVER="),MaxResaveUE4Version) )
			{
				if ( MaxResaveUE4Version == CURRENT_PACKAGE_VERSION )
				{
					MaxResaveUE4Version = GPackageFileUE4Version;
				}
			}
		}
	}

	FString ClassList;
	for (const FString& CurrentSwitch : Switches)
	{
		if ( FParse::Value(*CurrentSwitch, TEXT("RESAVECLASS="), ClassList, false) )
		{
			TArray<FString> ClassNames;
			ClassList.ParseIntoArray(ClassNames, TEXT(","), true);
			for (const FString& ClassName : ClassNames)
			{
				ResaveClasses.AddUnique(*ClassName);
			}

			break;
		}
	}

	/** determine if we should check subclasses of ResaveClasses */
	bool bIncludeChildClasses = Switches.Contains(TEXT("IncludeChildClasses"));
	if (bIncludeChildClasses && ResaveClasses.Num() == 0)
	{
		// Sanity check fail
		UE_LOG(LogContentCommandlet, Error, TEXT("AllowSubclasses param requires ResaveClass param."));
		return 1;
	}

	if (bIncludeChildClasses)
	{
		// Can't use ranged for here because the array grows inside of this loop.
		// Also, no need to iterate over the newly added objects as we know
		// we have found all of their subclasses too (IsChildOf guarantees that).
		const int32 NumResaveClasses = ResaveClasses.Num();
		for (int32 ClassIndex = 0; ClassIndex < NumResaveClasses; ++ClassIndex)
		{
			// Find the class object and then all derived classes
			UClass* ResaveClass = FindObject<UClass>(ANY_PACKAGE, *ResaveClasses[ClassIndex].ToString());
			if (ResaveClass)
			{
				for (TObjectIterator<UClass> It; It; ++It)
				{
					UClass* MaybeChildClass = *It;
					if (MaybeChildClass->IsChildOf(ResaveClass))
					{
						ResaveClasses.AddUnique(MaybeChildClass->GetFName());
					}
				}
			}
		}
	}

	return 0;
}

bool UResavePackagesCommandlet::ShouldSkipPackage(const FString& Filename)
{
	return false;
}

void UResavePackagesCommandlet::LoadAndSaveOnePackage(const FString& Filename)
{
	// Check to see if a derived commandlet wants to skip this package for one reason or another
	if (ShouldSkipPackage(Filename))
	{
		return;
	}

	// Skip the package if it doesn't have a required substring match
	if (PackageSubstring.Len() && !Filename.Contains(PackageSubstring) )
	{
		VerboseMessage(FString::Printf(TEXT("Skipping %s"), *Filename));
		return;
	}

	bool bIsReadOnly = IFileManager::Get().IsReadOnly(*Filename);

	if ( bIsReadOnly && !bVerifyContent && !bAutoCheckOut )
	{
		if (Verbosity != ONLY_ERRORS)
		{
			UE_LOG(LogContentCommandlet, Warning, TEXT("Skipping read-only file %s"), *Filename);
		}
	}
	else
	{
		if (Verbosity != ONLY_ERRORS)
		{
			UE_LOG(LogContentCommandlet, Display, TEXT("Loading %s"), *Filename);
		}

		static int32 LastErrorCount = 0;

		int32 NumErrorsFromLoading = GWarn->GetNumErrors();
		if (NumErrorsFromLoading > LastErrorCount)
		{
			UE_LOG(LogContentCommandlet, Warning, TEXT("%d total errors encountered during loading"), NumErrorsFromLoading);
		}
		LastErrorCount = NumErrorsFromLoading;

		// Get the package linker.
		VerboseMessage(TEXT("Pre GetPackageLinker"));

		BeginLoad();
		auto Linker = GetPackageLinker(NULL,*Filename,LOAD_NoVerify,NULL,NULL);
		EndLoad();
	
		// Bail early if we don't have a valid linker (package was out of date, etc)
		if( !Linker )
		{
			VerboseMessage(TEXT("Aborting...package could not be loaded"));
			CollectGarbage(RF_NoFlags);
			return;
		}

		VerboseMessage(TEXT("Post GetPackageLinker"));

		bool bSavePackage = true;
		PerformPreloadOperations(Linker, bSavePackage);

		VerboseMessage(FString::Printf(TEXT("Post PerformPreloadOperations, Resave? %d"), bSavePackage));
		
		if (bSavePackage)
		{
			PackagesRequiringResave++;

			// Only rebuild static meshes on load for the to be saved package.
			extern ENGINE_API FName GStaticMeshPackageNameToRebuild;
			GStaticMeshPackageNameToRebuild = FName(*FPackageName::FilenameToLongPackageName(Filename));

			// Assert if package couldn't be opened so we have no chance of messing up saving later packages.
			UPackage* Package = LoadPackage( NULL, *Filename, 0 );
			if (Package == NULL)
			{
				if (bCanIgnoreFails == true)
				{
					return;
				}
				else
				{
					check(Package);
				}
			}

			VerboseMessage(TEXT("Post LoadPackage"));

			// if we are only saving dirty packages and the package is not dirty, then we do not want to save the package (remember the default behavior is to ALWAYS save the package)
			if( ( bOnlySaveDirtyPackages == true ) && ( Package->IsDirty() == false ) )
			{
				bSavePackage = false;
			}

			// here we want to check and see if we have any loading warnings
			// if we do then we want to resave this package
			if( !bSavePackage && FParse::Param(FCommandLine::Get(),TEXT("SavePackagesThatHaveFailedLoads")) == true )
			{
				//UE_LOG(LogContentCommandlet, Warning, TEXT( "NumErrorsFromLoading: %d GWarn->Errors num: %d" ), NumErrorsFromLoading, GWarn->GetNumErrors() );

				if( NumErrorsFromLoading != GWarn->GetNumErrors() )
				{
					bSavePackage = true;
				}
			}

			{
				UWorld* World = UWorld::FindWorldInPackage(Package);
				if (World)
				{
					PerformAdditionalOperations(World, bSavePackage);
				}
			}

			// hook to allow performing additional checks without lumping everything into this one function
			PerformAdditionalOperations(Package,bSavePackage);

			VerboseMessage(TEXT("Post PerformAdditionalOperations"));

			// Check for any special per object operations
			for( FObjectIterator ObjectIt; ObjectIt; ++ObjectIt )
			{
				if( ObjectIt->IsIn( Package ) )
				{
					PerformAdditionalOperations( *ObjectIt, bSavePackage );
				}
			}
			
			VerboseMessage(TEXT("Post PerformAdditionalOperations Loop"));

			if (bStripEditorOnlyContent)
			{
				UE_LOG(LogContentCommandlet, Log, TEXT("Removing editor only data"));
				Package->SetPackageFlags(PKG_FilterEditorOnly);
			}

			if (bSavePackage == true)
			{
				bool bIsEmpty = true;
				// Check to see if this package contains only metadata, and if so delete the package instead of resaving it

				TArray<UObject *> ObjectsInOuter;
				GetObjectsWithOuter(Package, ObjectsInOuter);
				for (UObject* Obj : ObjectsInOuter)
				{
					if (!Obj->IsA(UMetaData::StaticClass()))
					{
						// This package has a real object
						bIsEmpty = false;
						break;
					}
				}

				if (bIsEmpty)
				{
					bSavePackage = false;
					Package = nullptr;
					
					UE_LOG(LogContentCommandlet, Display, TEXT("Package %s is empty and will be deleted"), *Filename);

					DeleteOnePackage(Filename);
				}
			}

			// Now based on the computation above we will see if we should actually attempt
			// to save this package
			if (bSavePackage == true)
			{
				if( bIsReadOnly == true && bVerifyContent == true && bAutoCheckOut == false )
				{
					UE_LOG(LogContentCommandlet, Warning, TEXT("Package [%s] is read-only but needs to be resaved (UE4 Version: %i, Licensee Version: %i  Current UE4 Version: %i, Current Licensee Version: %i)"),
						*Filename, Linker->Summary.GetFileVersionUE4(), Linker->Summary.GetFileVersionLicenseeUE4(), GPackageFileUE4Version, VER_LATEST_ENGINE_LICENSEEUE4 );
					if( SavePackageHelper(Package, FString(TEXT("Temp.temp"))) )
					{
						UE_LOG(LogContentCommandlet, Warning, TEXT("Correctly saved:  [Temp.temp].") );
					}
				}
				else
				{
					// check to see if we need to check this package out
					if ( bAutoCheckOut )
					{
						if( bIsReadOnly )
						{
							VerboseMessage(TEXT("Pre ForceGetStatus1"));
							ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
							FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState( Package, EStateCacheUsage::ForceUpdate );
							if(SourceControlState.IsValid())
							{
								if( SourceControlState->IsCheckedOutOther() )
								{
									UE_LOG(LogContentCommandlet, Warning, TEXT("[REPORT] Overwriting package %s (already checked out by someone else), will not submit"), *Filename);
								}
								else if( !SourceControlState->IsCurrent() )
								{
									UE_LOG(LogContentCommandlet, Warning, TEXT("[REPORT] Overwriting package %s (not at head), will not submit"), *Filename);
								}
								else
								{
									VerboseMessage(TEXT("Pre CheckOut"));

									SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), Package);

									VerboseMessage(TEXT("Post CheckOut"));

									FilesToSubmit.AddUnique(*Filename);
								}
							}
							VerboseMessage(TEXT("Post ForceGetStatus2"));
						}
					}

					// so now we need to see if we actually were able to check this file out
					// if the file is still read only then we failed and need to emit an error and go to the next package
					if (IFileManager::Get().IsReadOnly( *Filename ) == true)
					{
						UE_LOG(LogContentCommandlet, Error, TEXT("Unable to check out the Package: %s"), *Filename );
						return;
					}

					if (Verbosity != ONLY_ERRORS)
					{
						UE_LOG(LogContentCommandlet, Display, TEXT("Resaving package [%s] (UE4 Version: %i, Licensee Version: %i  Saved UE4 Version: %i, Saved Licensee Version: %i)"),
							*Filename,Linker->Summary.GetFileVersionUE4(), Linker->Summary.GetFileVersionLicenseeUE4(), GPackageFileUE4Version, VER_LATEST_ENGINE_LICENSEEUE4 );
					}

					if( SavePackageHelper(Package, Filename) )
					{
						if (Verbosity == VERY_VERBOSE)
						{
							UE_LOG(LogContentCommandlet, Display, TEXT("Correctly saved:  [%s]."), *Filename );
						}
					}
				}
			}
		}

		static int32 Counter = 0;

		if (!GarbageCollectionFrequency || Counter++ % GarbageCollectionFrequency == 0)
		{
			if (GarbageCollectionFrequency > 1)
			{
				UE_LOG(LogContentCommandlet, Display, TEXT("GC"));
			}
			VerboseMessage(TEXT("Pre CollectGarbage"));

			CollectGarbage(RF_NoFlags);

			VerboseMessage(TEXT("Post CollectGarbage"));
		}
	}
}

void UResavePackagesCommandlet::DeleteOnePackage(const FString& Filename)
{
	bool bIsReadOnly = IFileManager::Get().IsReadOnly(*Filename);

	if (bVerifyContent)
	{
		return;
	}

	if (bIsReadOnly && !bAutoCheckOut)
	{
		if (Verbosity != ONLY_ERRORS)
		{
			UE_LOG(LogContentCommandlet, Warning, TEXT("Skipping read-only file %s"), *Filename);
		}
		return;
	}

	FString PackageName;
	FPackageName::TryConvertFilenameToLongPackageName(Filename, PackageName);

	UPackage* Package = FindPackage(nullptr, *PackageName);

	if (Package)
	{
		// Unload package so we can delete it
		TArray<UPackage *> PackagesToDelete;
		PackagesToDelete.Add(Package);
		PackageTools::UnloadPackages(PackagesToDelete);
		PackagesToDelete.Empty();
		Package = nullptr;
	}

	FString PackageFilename = SourceControlHelpers::PackageFilename(Filename);
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(PackageFilename, EStateCacheUsage::ForceUpdate);

	if (SourceControlState.IsValid() && (SourceControlState->IsCheckedOut() || SourceControlState->IsAdded()))
	{
		UE_LOG(LogContentCommandlet, Display, TEXT("Revert '%s' from source control..."), *Filename);
		SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), PackageFilename);

		UE_LOG(LogContentCommandlet, Display, TEXT("Deleting '%s' from source control..."), *Filename);
		SourceControlProvider.Execute(ISourceControlOperation::Create<FDelete>(), PackageFilename);

		FilesToSubmit.AddUnique(Filename);
	}
	else if (SourceControlState.IsValid() && SourceControlState->CanCheckout())
	{
		UE_LOG(LogContentCommandlet, Display, TEXT("Deleting '%s' from source control..."), *Filename);
		SourceControlProvider.Execute(ISourceControlOperation::Create<FDelete>(), PackageFilename);

		FilesToSubmit.AddUnique(Filename);
	}
	else if (SourceControlState.IsValid() && SourceControlState->IsCheckedOutOther())
	{
		UE_LOG(LogContentCommandlet, Warning, TEXT("Couldn't delete '%s' from source control, someone has it checked out, skipping..."), *Filename);
	}
	else if (SourceControlState.IsValid() && !SourceControlState->IsSourceControlled())
	{
		UE_LOG(LogContentCommandlet, Warning, TEXT("'%s' is not in source control, attempting to delete from disk..."), *Filename);
		if (!IFileManager::Get().Delete(*Filename, false, true))
		{
			UE_LOG(LogContentCommandlet, Warning, TEXT("  ... failed to delete from disk."), *Filename);
		}
	}
	else
	{
		UE_LOG(LogContentCommandlet, Warning, TEXT("'%s' is in an unknown source control state, attempting to delete from disk..."), *Filename);
		if (!IFileManager::Get().Delete(*Filename, false, true))
		{
			UE_LOG(LogContentCommandlet, Warning, TEXT("  ... failed to delete from disk."), *Filename);
		}
	}
}

int32 UResavePackagesCommandlet::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;
	TArray<FString> Tokens;
	ParseCommandLine(Parms, Tokens, Switches);

	// Ensure source control is initialized and shut down properly
	FScopedSourceControl SourceControl;

	// strip editor only content
	bStripEditorOnlyContent = Switches.Contains(TEXT("STRIPEDITORONLY"));
	// skip the assert when a package can not be opened
	bCanIgnoreFails = Switches.Contains(TEXT("SKIPFAILS"));
	/** load all packages, and display warnings for those packages which would have been resaved but were read-only */
	bVerifyContent = Switches.Contains(TEXT("VERIFY"));
	/** if we should only save dirty packages **/
	bOnlySaveDirtyPackages = Switches.Contains(TEXT("OnlySaveDirtyPackages"));
	/** if we should auto checkout packages that need to be saved**/
	bAutoCheckOut = Switches.Contains(TEXT("AutoCheckOutPackages")) || Switches.Contains(TEXT("AutoCheckOut"));
	/** if we should auto checkin packages that were checked out**/
	bAutoCheckIn = bAutoCheckOut && (Switches.Contains(TEXT("AutoCheckIn")) || Switches.Contains(TEXT("AutoSubmit")));
	/** determine if we are building lighting for the map packages on the pass. **/
	bShouldBuildLighting = Switches.Contains(TEXT("buildlighting"));
	/** determine if we are building lighting for the map packages on the pass. **/
	bShouldBuildTextureStreaming = Switches.Contains(TEXT("buildtexturestreaming"));
	/** determine if we can skip the version changelist check */
	bIgnoreChangelist = Switches.Contains(TEXT("IgnoreChangelist"));
	if ( bShouldBuildLighting )
	{
		check( Switches.Contains(TEXT("AllowCommandletRendering")) );
		GarbageCollectionFrequency = 1;
	}

	// Default build on production
	LightingBuildQuality = Quality_Production;
	FString QualityStr;
	FParse::Value(*Params, TEXT("Quality="), QualityStr);
	if (QualityStr.Len())
	{
		if (QualityStr.Equals(TEXT("Preview"), ESearchCase::IgnoreCase))
		{
			LightingBuildQuality = Quality_Preview;
		}
		else if (QualityStr.Equals(TEXT("Medium"), ESearchCase::IgnoreCase))
		{
			LightingBuildQuality = Quality_Medium;
		}
		else if (QualityStr.Equals(TEXT("High"), ESearchCase::IgnoreCase))
		{
			LightingBuildQuality = Quality_High;
		}
		else if (QualityStr.Equals(TEXT("Production"), ESearchCase::IgnoreCase))
		{
			LightingBuildQuality = Quality_Production;
		}
		else
		{
			UE_LOG(LogContentCommandlet, Fatal, TEXT( "Unknown Quality(must be Preview/Medium/High/Production): %s"), *QualityStr );
		}
		UE_LOG(LogContentCommandlet, Display, TEXT("Lighing Build Quality is %s"), *QualityStr);
	}

	TArray<FString> PackageNames;
	int32 ResultCode = InitializeResaveParameters(Tokens, PackageNames);
	if ( ResultCode != 0 )
	{
		return ResultCode;
	}

	// Retrieve list of all packages in .ini paths.
	if(!PackageNames.Num() && !RedirectorsToFixup.Num())
	{
		return 0;
	}

	int32 GCIndex = 0;
	PackagesRequiringResave = 0;

	// allow for an option to restart at a given package name (in case it dies during a run, etc)
	bool bCanProcessPackage = true;
	FString FirstPackageToProcess;
	if (FParse::Value(*Params, TEXT("FirstPackage="), FirstPackageToProcess))
	{
		bCanProcessPackage = false;
	}
	FParse::Value(*Params, TEXT("PackageSubString="), PackageSubstring);
	if (PackageSubstring.Len())
	{
		UE_LOG(LogContentCommandlet, Display, TEXT( "Restricted to packages containing %s" ), *PackageSubstring );
	}

	// Avoid crash saving blueprint
	FFindInBlueprintSearchManager::Get();

	// Iterate over all packages.
	for( int32 PackageIndex = 0; PackageIndex < PackageNames.Num(); PackageIndex++ )
	{
		// Make sure we don't rebuild SMs that we're not going to save.
		extern ENGINE_API FName GStaticMeshPackageNameToRebuild;
		GStaticMeshPackageNameToRebuild = NAME_None;

		const FString& Filename = PackageNames[PackageIndex];

		// skip over packages before the first one allowed, if it was specified
		if (!bCanProcessPackage)
		{
			if (FPackageName::FilenameToLongPackageName(Filename) == FirstPackageToProcess)
			{
				bCanProcessPackage = true;
			}
			else
			{
				UE_LOG(LogContentCommandlet, Display, TEXT("Skipping %s"), *Filename);
				continue;
			}
		}

		// Load and save this package
		LoadAndSaveOnePackage(Filename);

		// Break out if we've resaved enough packages
		if( MaxPackagesToResave > -1 && PackagesRequiringResave >= MaxPackagesToResave )
		{
			UE_LOG(LogContentCommandlet, Warning, TEXT( "Attempting to resave more than MaxPackagesToResave; exiting" ) );
			break;
		}
	}

	// Force a directory watcher and asset registry tick
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	DirectoryWatcherModule.Get()->Tick(-1.0f);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.Tick(-1.0f);

	// Delete unreferenced redirector packages
	for (int32 PackageIndex = 0; PackageIndex < RedirectorsToFixup.Num(); PackageIndex++)
	{
		const FString& Filename = RedirectorsToFixup[PackageIndex];

		FName PackageName = FName(*FPackageName::FilenameToLongPackageName(Filename));

		// Load and save this package
		TArray<FName> Referencers;

		AssetRegistry.GetReferencers(PackageName, Referencers);

		if (Referencers.Num() == 0)
		{
			if (Verbosity != ONLY_ERRORS)
			{
				UE_LOG(LogContentCommandlet, Display, TEXT("Deleting unreferenced redirector [%s]"), *Filename);
			}

			DeleteOnePackage(Filename);
		}
		else if (Verbosity != ONLY_ERRORS)
		{
			UE_LOG(LogContentCommandlet, Display, TEXT("Can't delete redirector [%s], unsaved packages reference it"), *Filename);
		}
	}

	// Submit the results to source control
	if( bAutoCheckIn )
	{
		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
		SourceControlProvider.Init();

		// Check in all changed files
		if( FilesToSubmit.Num() > 0 )
		{
			TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();
			CheckInOperation->SetDescription( GetChangelistDescription() );
			SourceControlProvider.Execute(CheckInOperation, SourceControlHelpers::PackageFilenames(FilesToSubmit));
		}

		// toss the SCC manager
		SourceControlProvider.Close();
	}

	UE_LOG(LogContentCommandlet, Display, TEXT( "[REPORT] %d/%d packages required resaving" ), PackagesRequiringResave, PackageNames.Num() );


	return 0;
}

FText UResavePackagesCommandlet::GetChangelistDescription() const
{
	FText ChangelistDescription;

	if (bShouldBuildTextureStreaming && bShouldBuildLighting)
	{
		ChangelistDescription = NSLOCTEXT("ContentCmdlets", "ChangelistDescriptionBuildLightingAndTextureStreaming", "Rebuild lightmaps & texture streaming.");
	}
	else if (bShouldBuildLighting)
	{
		ChangelistDescription = NSLOCTEXT("ContentCmdlets", "ChangelistDescriptionBuildLighting", "Rebuild lightmaps.");
	}
	else if (bShouldBuildTextureStreaming)
	{
		ChangelistDescription = NSLOCTEXT("ContentCmdlets", "ChangelistDescriptionBuildTextureStreaming", "Rebuild texture streaming.");
	}
	else if (RedirectorsToFixup.Num() > 0)
	{
		ChangelistDescription = NSLOCTEXT("ContentCmdlets", "ChangelistDescriptionRedirectors", "Fixing Redirectors");
	}
	else
	{
		ChangelistDescription = NSLOCTEXT("ContentCmdlets", "ChangelistDescription", "Resave Deprecated Packages");
	}

	return ChangelistDescription;
}


void UResavePackagesCommandlet::PerformPreloadOperations( FLinkerLoad* PackageLinker, bool& bSavePackage )
{
	const int32 UE4PackageVersion = PackageLinker->Summary.GetFileVersionUE4();
	const int32 LicenseeUE4PackageVersion = PackageLinker->Summary.GetFileVersionLicenseeUE4();


	// validate that this package meets the minimum requirement
	if ( MinResaveUE4Version != IGNORE_PACKAGE_VERSION && UE4PackageVersion < MinResaveUE4Version )
	{
		bSavePackage = false;
	}

	// Check if this package meets the maximum requirements.
	bool bNoLimitation = MaxResaveUE4Version == IGNORE_PACKAGE_VERSION && MaxResaveLicenseeUE4Version == IGNORE_PACKAGE_VERSION;
	bool bAllowResave = bNoLimitation ||
						 (MaxResaveUE4Version != IGNORE_PACKAGE_VERSION && UE4PackageVersion <= MaxResaveUE4Version) ||
						 (MaxResaveLicenseeUE4Version != IGNORE_PACKAGE_VERSION && LicenseeUE4PackageVersion <= MaxResaveLicenseeUE4Version);

	// If the package was saved with a higher engine version do not try to resave it. This also addresses problem with people 
	// building editor locally and resaving content with a 0 CL version (e.g. BUILD_FROM_CL == 0)
	if (!bIgnoreChangelist && PackageLinker->Summary.SavedByEngineVersion.GetChangelist() > FEngineVersion::Current().GetChangelist())
	{
		UE_LOG(LogContentCommandlet, Warning, TEXT("Skipping resave of %s due to engine version mismatch (Package:%d, Editor:%d "), 
			*PackageLinker->GetArchiveName(),
			PackageLinker->Summary.SavedByEngineVersion.GetChangelist(), 
			FEngineVersion::Current().GetChangelist());
		bSavePackage = false;
	}

	// If not, don't resave it.
	if ( !bAllowResave )
	{
		bSavePackage = false;
	}

	// Check if the package contains any instances of the class that needs to be resaved.
	if ( bSavePackage && ResaveClasses.Num() > 0 )
	{
		bSavePackage = false;
		for (int32 ExportIndex = 0; !bSavePackage && ExportIndex < PackageLinker->ExportMap.Num(); ExportIndex++)
		{
			FName ExportClassName = PackageLinker->GetExportClassName(ExportIndex);
			if (ResaveClasses.Contains(ExportClassName))
			{
				bSavePackage = true;
				break;
			}
		}
	}
}

bool UResavePackagesCommandlet::CheckoutFile(const FString& Filename, bool bAddFile )
{
	if (!bAutoCheckOut)
	{
		return true;
	}

	bool bIsReadOnly = IFileManager::Get().IsReadOnly(*Filename);
	if (!bIsReadOnly && !bAddFile)
	{
		return true;
	}

	//FString FullPath = FPaths::ConvertRelativePathToFull(StreamingLevelPackageFilename);
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(*Filename, EStateCacheUsage::ForceUpdate);
	if (SourceControlState.IsValid())
	{
		if (SourceControlState->IsCheckedOutOther())
		{
			UE_LOG(LogContentCommandlet, Error, TEXT("[REPORT] %s level is already checked out by someone else, can not submit!"), *Filename);
		}
		else if (!SourceControlState->IsCurrent())
		{
			UE_LOG(LogContentCommandlet, Error, TEXT("[REPORT] %s is not synced to head, can not submit"), *Filename);
		}
		else if ( SourceControlState->IsSourceControlled() == false )
		{
			if ( bAddFile )
			{
				if (SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), *Filename) == ECommandResult::Succeeded)
				{
					UE_LOG(LogContentCommandlet, Display, TEXT("[REPORT] %s successfully added"), *Filename);
					return true;
				}
				else
				{
					UE_LOG(LogContentCommandlet, Error, TEXT("[REPORT] %s could not be added!"), *Filename);
				}
			}
		}
		else 
		{
			// already checked out this file
			if (SourceControlState->IsCheckedOut() || SourceControlState->IsAdded() )
			{
				return true;
			}
			if (SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), *Filename) == ECommandResult::Succeeded)
			{
				UE_LOG(LogContentCommandlet, Display, TEXT("[REPORT] %s Checked out successfully"), *Filename);
				return true;
			}
			else
			{
				UE_LOG(LogContentCommandlet, Error, TEXT("[REPORT] %s could not be checked out!"), *Filename);
			}
		}
	}
	return false;
}

void UResavePackagesCommandlet::PerformAdditionalOperations(class UWorld* World, bool& bSavePackage)
{
	check(World);

	TArray<TWeakObjectPtr<ULevel>> LevelsToRebuild;
	ABrush::NeedsRebuild(&LevelsToRebuild);
	for (const TWeakObjectPtr<ULevel>& Level : LevelsToRebuild)
	{
		if (Level.IsValid())
		{
			GEditor->RebuildLevel(*Level.Get());
		}
	}
	ABrush::OnRebuildDone();

	if (bShouldBuildLighting || bShouldBuildTextureStreaming)
	{
		bool bShouldProceedWithRebuild = true;

		static bool bHasLoadedStartupPackages = false;
		if (bHasLoadedStartupPackages == false)
		{
			// make sure all possible script/startup packages are loaded
			bHasLoadedStartupPackages = FStartupPackages::LoadAll();
		}

		// Setup the world.
		World->WorldType = EWorldType::Editor;
		World->AddToRoot();
		if (!World->bIsWorldInitialized)
		{
			UWorld::InitializationValues IVS;
			IVS.RequiresHitProxies(false);
			IVS.ShouldSimulatePhysics(false);
			IVS.EnableTraceCollision(false);
			IVS.CreateNavigation(false);
			IVS.CreateAISystem(false);
			IVS.AllowAudioPlayback(false);
			IVS.CreatePhysicsScene(true);

			World->InitWorld(IVS);
			World->PersistentLevel->UpdateModelComponents();
			World->UpdateWorldComponents(true, false);
		}
		FWorldContext &WorldContext = GEditor->GetEditorWorldContext(true);
		WorldContext.SetCurrentWorld(World);
		GWorld = World;

		TArray<FString> SublevelFilenames;
		auto CheckOutLevelFile = [this,&bShouldProceedWithRebuild, &SublevelFilenames](ULevel* InLevel)
		{
			if (InLevel && InLevel->MapBuildData)
			{
				UPackage* MapBuildDataPackage = InLevel->MapBuildData->GetOutermost();
				if (MapBuildDataPackage != InLevel->GetOutermost())
				{
					FString MapBuildDataPackageName;
					if (FPackageName::DoesPackageExist(MapBuildDataPackage->GetName(), NULL, &MapBuildDataPackageName))
					{
						if (CheckoutFile(MapBuildDataPackageName))
						{
							SublevelFilenames.Add(MapBuildDataPackageName);
						}
						else
						{
							bShouldProceedWithRebuild = false;
						}
					}
					else
					{
						bShouldProceedWithRebuild = false;
					}
				}
			}
		};

		// if we can't check out the main map or it's not up to date then we can't do the lighting rebuild at all!
		FString WorldPackageName;
		if (FPackageName::DoesPackageExist(World->GetOutermost()->GetName(), NULL, &WorldPackageName))
		{
			if (CheckoutFile(WorldPackageName))
			{
				SublevelFilenames.Add(WorldPackageName);

				CheckOutLevelFile(World->PersistentLevel);
			}
			else
			{
				bShouldProceedWithRebuild = false;
			}
		}
		else
		{
			bShouldProceedWithRebuild = false;
		}
		


		if (bShouldProceedWithRebuild)
		{
			World->LoadSecondaryLevels(true, NULL);

			for (ULevelStreaming* NextStreamingLevel : World->StreamingLevels)
			{
				CheckOutLevelFile(NextStreamingLevel->GetLoadedLevel());

				FString StreamingLevelPackageFilename;
				const FString StreamingLevelWorldAssetPackageName = NextStreamingLevel->GetWorldAssetPackageName();
				if (FPackageName::DoesPackageExist(StreamingLevelWorldAssetPackageName, NULL, &StreamingLevelPackageFilename))
				{
					// check to see if we need to check this package out
					if (CheckoutFile(StreamingLevelPackageFilename))
					{
						SublevelFilenames.Add(StreamingLevelPackageFilename);
					}
					else
					{
						bShouldProceedWithRebuild = false;
						break;
					}
				}

				NextStreamingLevel->bShouldBeVisible = true;
				NextStreamingLevel->bShouldBeLoaded = true;
			}
		}

	
		// If nothing came up that stops us from continuing, then start building lightmass
		if (bShouldProceedWithRebuild)
		{
			World->FlushLevelStreaming(EFlushLevelStreamingType::Full);

			// We need any deferred commands added when loading to be executed before we start building lighting.
			GEngine->TickDeferredCommands();

			if (bShouldBuildTextureStreaming)
			{
				FEditorBuildUtils::EditorBuildTextureStreaming(World);
			}

			if (bShouldBuildLighting)
			{
				FLightingBuildOptions LightingOptions;
 				LightingOptions.QualityLevel = LightingBuildQuality;

				auto BuildFailedDelegate = [&bShouldProceedWithRebuild,&World]() {
				UE_LOG(LogContentCommandlet, Error, TEXT("[REPORT] Failed building lighting for %s"), *World->GetOutermost()->GetName());
					bShouldProceedWithRebuild = false;
			};

			FDelegateHandle BuildFailedDelegateHandle = FEditorDelegates::OnLightingBuildFailed.AddLambda(BuildFailedDelegate);

			GEditor->BuildLighting(LightingOptions);
			while (GEditor->IsLightingBuildCurrentlyRunning())
			{
				GEditor->UpdateBuildLighting();
			}

			FEditorDelegates::OnLightingBuildFailed.Remove(BuildFailedDelegateHandle);
			}
			auto SaveMapBuildData = [this, &SublevelFilenames](ULevel* InLevel)
			{
				if (InLevel && InLevel->MapBuildData && bShouldBuildLighting)
				{
					UPackage* MapBuildDataPackage = InLevel->MapBuildData->GetOutermost();
					FString MapBuildDataPackageName = MapBuildDataPackage->GetName();

					if (MapBuildDataPackage != InLevel->GetOutermost())
					{
						FString MapBuildDataFilename;

						if (FPackageName::TryConvertLongPackageNameToFilename(MapBuildDataPackageName, MapBuildDataFilename, FPackageName::GetAssetPackageExtension()))
						{
							if (IFileManager::Get().FileExists(*MapBuildDataFilename))
							{
								if ( CheckoutFile(MapBuildDataFilename, true) )
								{
									SublevelFilenames.Add(MapBuildDataFilename);
								}

								SavePackageHelper(MapBuildDataPackage, MapBuildDataFilename); 
							}
							else
							{
								SavePackageHelper(MapBuildDataPackage, MapBuildDataFilename);
								if (CheckoutFile(MapBuildDataFilename, true))
								{
									SublevelFilenames.Add(MapBuildDataFilename);
								}
							}
						}
					}
				}
			};

			SaveMapBuildData( World->PersistentLevel );


			// If everything is a success, resave the levels.
			if( bShouldProceedWithRebuild )
			{
				for (ULevelStreaming* NextStreamingLevel : World->StreamingLevels)
				{
					FString StreamingLevelPackageFilename;
					const FString StreamingLevelWorldAssetPackageName = NextStreamingLevel->GetWorldAssetPackageName();
					if (FPackageName::DoesPackageExist(StreamingLevelWorldAssetPackageName, NULL, &StreamingLevelPackageFilename))
					{
						UPackage* SubLevelPackage = NextStreamingLevel->GetLoadedLevel()->GetOutermost();
						if (!SavePackageHelper(SubLevelPackage, StreamingLevelPackageFilename))
						{
							UE_LOG(LogContentCommandlet, Error, TEXT("[REPORT] Failed to save sub level: %s"), *StreamingLevelPackageFilename);
						}

						SaveMapBuildData(NextStreamingLevel->GetLoadedLevel());
					}
				}
			}
		}
		else
		{
			UE_LOG(LogContentCommandlet, Error, TEXT("[REPORT] Failed to complete steps necessary to start a lightmass or texture streaming build of %s"), *World->GetName());
		}

		if ((bShouldProceedWithRebuild == false)||(bSavePackage == false))
		{
			// don't save our parent package
			bSavePackage = false;
			
			//FString FullPath = FPaths::ConvertRelativePathToFull(StreamingLevelPackageFilename);
			ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
			
			// revert all our packages
			for (const auto& SublevelFilename : SublevelFilenames)
			{
				SourceControlProvider.Execute(ISourceControlOperation::Create<FRevert>(), *SublevelFilename);
			}
		}
		else
		{
			for(const auto& SublevelFilename : SublevelFilenames)
			{
				FilesToSubmit.AddUnique(SublevelFilename);
			}
		}

		
		World->RemoveFromRoot();

		WorldContext.SetCurrentWorld(nullptr);
		GWorld = nullptr;

	}
}

void UResavePackagesCommandlet::PerformAdditionalOperations( class UObject* Object, bool& bSavePackage )
{

}


void UResavePackagesCommandlet::PerformAdditionalOperations( UPackage* Package, bool& bSavePackage )
{
	check(Package);
	bool bShouldSavePackage = false;
	
	if( ( FParse::Param(FCommandLine::Get(), TEXT("CLEANCLASSES")) == true ) && ( CleanClassesFromContentPackages(Package) == true ) )
	{
		bShouldSavePackage = true;
	}

	// add additional operations here

	bSavePackage = bSavePackage || bShouldSavePackage;
}


bool UResavePackagesCommandlet::CleanClassesFromContentPackages( UPackage* Package )
{
	check(Package);
	bool bResult = false;

	for ( TObjectIterator<UClass> It; It; ++It )
	{
		if ( It->IsIn(Package) )
		{
			UE_LOG(LogContentCommandlet, Warning, TEXT("Removing class '%s' from package [%s]"), *It->GetPathName(), *Package->GetName());

			// mark the class as transient so that it won't be saved into the package
			It->SetFlags(RF_Transient);

			// clear the standalone flag just to be sure :)
			It->ClearFlags(RF_Standalone);
			bResult = true;
		}
	}

	return bResult;
}

void UResavePackagesCommandlet::VerboseMessage(const FString& Message)
{
	if (Verbosity == VERY_VERBOSE)
	{
		UE_LOG(LogContentCommandlet, Verbose, TEXT("%s"), *Message);
	}
}

/*-----------------------------------------------------------------------------
	UWrangleContent.
-----------------------------------------------------------------------------*/

/** 
 * Helper struct to store information about a unreferenced object
 */
struct FUnreferencedObject
{
	/** Name of package this object resides in */
	FString PackageName;
	/** Full name of object */
	FString ObjectName;
	/** Size on disk as recorded in FObjectExport */
	int32 SerialSize;

	/**
	 * Constructor for easy creation in a TArray
	 */
	FUnreferencedObject(const FString& InPackageName, const FString& InObjectName, int32 InSerialSize)
	: PackageName(InPackageName)
	, ObjectName(InObjectName)
	, SerialSize(InSerialSize)
	{
	}
};

/**
 * Helper struct to store information about referenced objects insde
 * a package. Stored in TMap<> by package name, so this doesn't need
 * to store the package name 
 */
struct FPackageObjects
{
	/** All objected referenced in this package, and their class */
	TMap<FString, UClass*> ReferencedObjects;

	/** Was this package a fully loaded package, and saved right after being loaded? */
	bool bIsFullyLoadedPackage;

	FPackageObjects()
	: bIsFullyLoadedPackage(false)
	{
	}

};
	
FArchive& operator<<(FArchive& Ar, FPackageObjects& PackageObjects)
{
	Ar << PackageObjects.bIsFullyLoadedPackage;

	if (Ar.IsLoading())
	{
		int32 NumObjects;
		FString ObjectName;
		FString ClassName;

		Ar << NumObjects;
		for (int32 ObjIndex = 0; ObjIndex < NumObjects; ObjIndex++)
		{
			Ar << ObjectName << ClassName;
			UClass* Class = StaticLoadClass(UObject::StaticClass(), NULL, *ClassName, NULL, LOAD_None, NULL);
			PackageObjects.ReferencedObjects.Add(*ObjectName, Class);
		}
	}
	else if (Ar.IsSaving())
	{
		int32 NumObjects = PackageObjects.ReferencedObjects.Num();
		Ar << NumObjects;
		for (TMap<FString, UClass*>::TIterator It(PackageObjects.ReferencedObjects); It; ++It)
		{
			FString ObjectName, ClassName;
			ObjectName = It.Key();
			ClassName = It.Value()->GetPathName();

			Ar << ObjectName << ClassName;
		}
		
	}

	return Ar;
}

/**
 * Stores the fact that an object (given just a name) was referenced
 *
 * @param PackageName Name of the package the object lives in
 * @param ObjectName FullName of the object
 * @param ObjectClass Class of the object
 * @param ObjectRefs Map to store the object information in
 * @param bIsFullLoadedPackage true if the packge this object is in was fully loaded
 */
void ReferenceObjectInner(const FString& PackageName, const FString& ObjectName, UClass* ObjectClass, TMap<FString, FPackageObjects>& ObjectRefs, bool bIsFullyLoadedPackage)
{
	// look for an existing FPackageObjects
	FPackageObjects* PackageObjs = ObjectRefs.Find(*PackageName);
	// if it wasn't found make a new entry in the map
	if (PackageObjs == NULL)
	{
		PackageObjs = &ObjectRefs.Add(*PackageName, FPackageObjects());
	}

	// if either the package was already marked as fully loaded or it now is fully loaded, then
	// it will be fully loaded
	PackageObjs->bIsFullyLoadedPackage = PackageObjs->bIsFullyLoadedPackage || bIsFullyLoadedPackage;

	// add this referenced object to the map
	PackageObjs->ReferencedObjects.Add(*ObjectName, ObjectClass);

	// make sure the class is in the root set so it doesn't get GC'd, making the pointer we cached invalid
	ObjectClass->AddToRoot();
}

/**
 * Stores the fact that an object was referenced
 *
 * @param Object The object that was referenced
 * @param ObjectRefs Map to store the object information in
 * @param bIsFullLoadedPackage true if the package this object is in was fully loaded
 */
void ReferenceObject(UObject* Object, TMap<FString, FPackageObjects>& ObjectRefs, bool bIsFullyLoadedPackage)
{
	FString PackageName = Object->GetOutermost()->GetName();

	// find the outermost non-upackage object, as it will be loaded later with all its subobjects
	while (Object->GetOuter() && Object->GetOuter()->GetClass() != UPackage::StaticClass())
	{
		Object = Object->GetOuter();
	}

	// make sure this object is valid (it's not in a script or native-only package)
	// An invalid writable outer name indicates the package name is in a temp or script path, or is using a short package name
	const bool bValidWritableOuterName = FPackageName::IsValidLongPackageName(Object->GetOutermost()->GetName());
	bool bIsValid = true;
	// can't be in a script packge or be a field/template in a native package, or a top level pacakge, or in the transient package
	if (!bValidWritableOuterName ||
		Object->GetOutermost()->HasAnyPackageFlags(PKG_ContainsScript) ||
		Object->IsA(UField::StaticClass()) ||
		Object->IsTemplate(RF_ClassDefaultObject) ||
		Object->GetOuter() == NULL ||
		Object->IsIn(GetTransientPackage()))
	{
		bIsValid = false;
	}

	if (bIsValid)
	{
		// save the reference
		ReferenceObjectInner(PackageName, Object->GetFullName(), Object->GetClass(), ObjectRefs, bIsFullyLoadedPackage);

		//@todo-packageloc Add reference to localized packages.
	}
}

/**
 * Take a package pathname and return a path for where to save the cutdown
 * version of the package. Will create the directory if needed.
 *
 * @param Filename Path to a package file
 * @param CutdownDirectoryName Name of the directory to put this package into
 *
 * @return Location to save the cutdown package
 */
FString MakeCutdownFilename(const FString& Filename, const TCHAR* CutdownDirectoryName=TEXT("CutdownPackages"))
{
	// replace the .. with ..\GAMENAME\CutdownContent
	FString CutdownDirectory = FPaths::GetPath(Filename);
	if ( CutdownDirectory.Contains(FPaths::ProjectDir()) )
	{
		// Content from the game directory may not be relative to the engine folder
		CutdownDirectory = CutdownDirectory.Replace(*FPaths::ProjectDir(), *FString::Printf(TEXT("%s%s/Game/"), *FPaths::ProjectSavedDir(), CutdownDirectoryName));
	}
	else
	{
		CutdownDirectory = CutdownDirectory.Replace(TEXT("../../../"), *FString::Printf(TEXT("%s%s/"), *FPaths::ProjectSavedDir(), CutdownDirectoryName));
	}

	// make sure it exists
	IFileManager::Get().MakeDirectory(*CutdownDirectory, true);

	// return the full pathname
	return CutdownDirectory / FPaths::GetCleanFilename(Filename);
}

UWrangleContentCommandlet::UWrangleContentCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	LogToConsole = false;
}

int32 UWrangleContentCommandlet::Main( const FString& Params )
{
	// overall commandlet control options
	bool bShouldRestoreFromPreviousRun = FParse::Param(*Params, TEXT("restore"));
	bool bShouldSavePackages = !FParse::Param(*Params, TEXT("nosave"));
	bool bShouldSaveUnreferencedContent = !FParse::Param(*Params, TEXT("nosaveunreferenced"));
	bool bShouldDumpUnreferencedContent = FParse::Param(*Params, TEXT("reportunreferenced"));
	bool bShouldCleanOldDirectories = !FParse::Param(*Params, TEXT("noclean"));
	bool bShouldSkipMissingClasses = FParse::Param(*Params, TEXT("skipMissingClasses"));

	// what per-object stripping to perform
	bool bShouldStripLargeEditorData = FParse::Param(*Params, TEXT("striplargeeditordata"));
	bool bShouldStripMips = FParse::Param(*Params, TEXT("stripmips"));

	// package loading options
	bool bShouldLoadAllMaps = FParse::Param(*Params, TEXT("allmaps"));
	
	// if no platforms specified, keep them all
	UE_LOG(LogContentCommandlet, Warning, TEXT("Keeping platform-specific data for ALL platforms"));

	FString SectionStr;
	FParse::Value( *Params, TEXT( "SECTION=" ), SectionStr );

	// store all referenced objects
	TMap<FString, FPackageObjects> AllReferencedPublicObjects;

	if (bShouldRestoreFromPreviousRun)
	{
		FArchive* Ar = IFileManager::Get().CreateFileReader(*(FPaths::ProjectDir() + TEXT("Wrangle.bin")));
		if( Ar != NULL )
		{
			*Ar << AllReferencedPublicObjects;
			delete Ar;
		}
		else
		{
			UE_LOG(LogContentCommandlet, Warning, TEXT("Could not read in Wrangle.bin so not restoring and doing a full wrangle") );
		}
	}
	else
	{
		// make name for our ini file to control loading
		FString WrangleContentIniName =	FPaths::SourceConfigDir() + TEXT("WrangleContent.ini");

		// figure out which section to use to get the packages to fully load
		FString SectionToUse = TEXT("WrangleContent.PackagesToFullyLoad");
		if( SectionStr.Len() > 0 )
		{
			SectionToUse = FString::Printf( TEXT( "WrangleContent.%sPackagesToFullyLoad" ), *SectionStr );
		}

		// get a list of packages to load
		const FConfigSection* PackagesToFullyLoadSection = GConfig->GetSectionPrivate( *SectionToUse, 0, 1, *WrangleContentIniName );
		const FConfigSection* StartupPackages = GConfig->GetSectionPrivate( TEXT("/Script/Engine.StartupPackages"), 0, 1, GEngineIni );

		// we expect either the .ini to exist, or -allmaps to be specified
		if (!PackagesToFullyLoadSection && !bShouldLoadAllMaps)
		{
			UE_LOG(LogContentCommandlet, Error, TEXT("This commandlet needs a WrangleContent.ini in the Config directory with a [WrangleContent.PackagesToFullyLoad] section"));
			return 1;
		}

		if (bShouldCleanOldDirectories)
		{
			IFileManager::Get().DeleteDirectory(*FString::Printf(TEXT("%sCutdownPackages"), *FPaths::ProjectSavedDir()), false, true);
			IFileManager::Get().DeleteDirectory(*FString::Printf(TEXT("%sNFSContent"), *FPaths::ProjectSavedDir()), false, true);
		}

		// copy the packages to load, since we are modifying it
		FConfigSectionMap PackagesToFullyLoad;
		if (PackagesToFullyLoadSection)
		{
			PackagesToFullyLoad = *PackagesToFullyLoadSection;
		}

		// make sure all possible script/startup packages are loaded
		FStartupPackages::LoadAll();

		// verify that all startup packages have been loaded
		if (StartupPackages)
		{
			for (FConfigSectionMap::TConstIterator It(*StartupPackages); It; ++It)
			{
				if (It.Key() == TEXT("Package"))
				{
					PackagesToFullyLoad.Add(*It.Key().ToString(), *It.Value().GetValue());
					if ( FindPackage(NULL, *It.Value().GetValue()) )
					{
						UE_LOG(LogContentCommandlet, Warning, TEXT("Startup package '%s' was loaded"), *It.Value().GetValue());
					}
					else
					{
						UE_LOG(LogContentCommandlet, Warning, TEXT("Startup package '%s' was not loaded during FStartupPackages::LoadAll..."), *It.Value().GetValue());
					}
				}
			}
		}

		if (bShouldLoadAllMaps)
		{
			TArray<FString> AllPackageFilenames;
			FEditorFileUtils::FindAllPackageFiles(AllPackageFilenames);
			for (int32 PackageIndex = 0; PackageIndex < AllPackageFilenames.Num(); PackageIndex++)
			{
				const FString& Filename = AllPackageFilenames[PackageIndex];
				if (FPaths::GetExtension(Filename,true) == FPackageName::GetMapPackageExtension() )
				{
					PackagesToFullyLoad.Add(TEXT("Package"), FPackageName::FilenameToLongPackageName(Filename));
				}
			}
		}


		// read in the per-map packages to cook
		TMap<FString, TArray<FString> > PerMapCookPackages;
		GConfig->Parse1ToNSectionOfStrings(TEXT("/Script/Engine.PackagesToForceCookPerMap"), TEXT("Map"), TEXT("Package"), PerMapCookPackages, GEngineIni);

		// gather any per map packages for cooking
		TArray<FString> PerMapPackagesToLoad;
		for (FConfigSectionMap::TIterator PackageIt(PackagesToFullyLoad); PackageIt; ++PackageIt)
		{
			// add dependencies for the per-map packages for this map (if any)
			TArray<FString>* Packages = PerMapCookPackages.Find(PackageIt.Value().GetValue());
			if (Packages != NULL)
			{
				for (int32 PackageIndex = 0; PackageIndex < Packages->Num(); PackageIndex++)
				{
					PerMapPackagesToLoad.Add(*(*Packages)[PackageIndex]);
				}
			}
		}

		// now add them to the list of all packges to load
		for (int32 PackageIndex = 0; PackageIndex < PerMapPackagesToLoad.Num(); PackageIndex++)
		{
			PackagesToFullyLoad.Add(TEXT("Package"), *PerMapPackagesToLoad[PackageIndex]);
		}

		// all currently loaded public objects were referenced by script code, so mark it as referenced
		for(FObjectIterator ObjectIt;ObjectIt;++ObjectIt)
		{
			UObject* Object = *ObjectIt;

			// record all public referenced objects
//			if (Object->HasAnyFlags(RF_Public))
			{
				ReferenceObject(Object, AllReferencedPublicObjects, false);
			}
		}

		// go over all the packages that we want to fully load
		for (FConfigSectionMap::TIterator PackageIt(PackagesToFullyLoad); PackageIt; ++PackageIt)
		{
			// there may be multiple sublevels to load if this package is a persistent level with sublevels
			TArray<FString> PackagesToLoad;
			// start off just loading this package (more may be added in the loop)
			PackagesToLoad.Add(*PackageIt.Value().GetValue());

			for (int32 PackageIndex = 0; PackageIndex < PackagesToLoad.Num(); PackageIndex++)
			{
				// save a copy of the packagename (not a reference in case the PackgesToLoad array gets realloced)
				FString PackageName = PackagesToLoad[PackageIndex];
				FString PackageFilename;

				if( FPackageName::DoesPackageExist( PackageName, NULL, &PackageFilename ) == true )
				{
					SET_WARN_COLOR(COLOR_WHITE);
					UE_LOG(LogContentCommandlet, Warning, TEXT("Fully loading %s..."), *PackageFilename);
					CLEAR_WARN_COLOR();

	// @todo josh: track redirects in this package and then save the package instead of copy it if there were redirects
	// or make sure that the following redirects marks the package dirty (which maybe it shouldn't do in the editor?)

					// load the package fully
					UPackage* Package = LoadPackage(NULL, *PackageFilename, LOAD_None);

					BeginLoad();
					auto Linker = GetPackageLinker( NULL, *PackageFilename, LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
					EndLoad();

					// look for special package types
					bool bIsMap = Linker->ContainsMap();
					bool bIsScriptPackage = Linker->ContainsCode();

					// collect all public objects loaded
					for(FObjectIterator ObjectIt; ObjectIt; ++ObjectIt)
					{
						UObject* Object = *ObjectIt;

						// record all public referenced objects (skipping over top level packages)
						if (/*Object->HasAnyFlags(RF_Public) &&*/ Object->GetOuter() != NULL)
						{
							// is this public object in a fully loaded package?
							bool bIsObjectInFullyLoadedPackage = Object->IsIn(Package);

							if (bIsMap && bIsObjectInFullyLoadedPackage && Object->HasAnyFlags(RF_Public))
							{
								UE_LOG(LogContentCommandlet, Warning, TEXT("Clearing public flag on map object %s"), *Object->GetFullName());
								Object->ClearFlags(RF_Public);
								// mark that we need to save the package since we modified it (instead of copying it)
								Object->MarkPackageDirty();
							}
							else
							{
								// record that this object was referenced
								ReferenceObject(Object, AllReferencedPublicObjects, bIsObjectInFullyLoadedPackage);
							}
						}
					}

					// add any sublevels of this world to the list of levels to load
					for (TObjectIterator<UWorld> WorldIt; WorldIt; ++WorldIt)
					{
						UWorld*		World = *WorldIt;
						// iterate over streaming level objects loading the levels.
						for( int32 LevelIndex=0; LevelIndex<World->StreamingLevels.Num(); LevelIndex++ )
						{
							ULevelStreaming* StreamingLevel = World->StreamingLevels[LevelIndex];
							if( StreamingLevel )
							{
								FString SubLevelName = StreamingLevel->GetWorldAssetPackageName();
								// add this sublevel's package to the list of packages to load if it's not already in the master list of packages
								if (PackagesToFullyLoad.FindKey(SubLevelName) == NULL)
								{
									PackagesToLoad.AddUnique(SubLevelName);
								}
							}
						}
					}

					// save/copy the package if desired, and only if it's not a script package (script code is
					// not cutdown, so we always use original script code)
					if (bShouldSavePackages && !bIsScriptPackage)
					{
						// make the name of the location to put the package
						FString CutdownPackageName = MakeCutdownFilename(PackageFilename);
						
						// if the package was modified by loading it, then we should save the package
						if (Package->IsDirty())
						{
							// save the fully load packages
							UE_LOG(LogContentCommandlet, Warning, TEXT("Saving fully loaded package %s..."), *CutdownPackageName);
							if (!SavePackageHelper(Package, CutdownPackageName))
							{
								UE_LOG(LogContentCommandlet, Error, TEXT("Failed to save package %s..."), *CutdownPackageName);
							}
						}
						else
						{
							UE_LOG(LogContentCommandlet, Warning, TEXT("Copying fully loaded package %s..."), *CutdownPackageName);
							// copy the unmodified file (faster than saving) (0 is success)
							if (IFileManager::Get().Copy(*CutdownPackageName, *PackageFilename) != 0)
							{
								UE_LOG(LogContentCommandlet, Error, TEXT("Failed to copy package to %s..."), *CutdownPackageName);
							}
						}
					}

					// close this package
					CollectGarbage(RF_NoFlags);
				}
			}
		}

		// save out the referenced objects so we can restore
		FArchive* Ar = IFileManager::Get().CreateFileWriter(*(FPaths::ProjectDir() + TEXT("Wrangle.bin")));
		*Ar << AllReferencedPublicObjects;
		delete Ar;
	}

	// list of all objects that aren't needed
	TArray<FUnreferencedObject> UnnecessaryPublicObjects;
	TMap<FString, FPackageObjects> UnnecessaryObjectsByPackage;
	TMap<FString, bool> UnnecessaryObjects;
	TArray<FString> UnnecessaryPackages;

	// now go over all packages, quickly, looking for public objects NOT in the AllNeeded array
	TArray<FString> AllPackages;
	FEditorFileUtils::FindAllPackageFiles(AllPackages);

	if (bShouldDumpUnreferencedContent || bShouldSaveUnreferencedContent)
	{
		SET_WARN_COLOR(COLOR_WHITE);
		UE_LOG(LogContentCommandlet, Warning, TEXT(""));
		UE_LOG(LogContentCommandlet, Warning, TEXT("Looking for unreferenced objects:"));
		CLEAR_WARN_COLOR();

		// Iterate over all files doing stuff.
		for (int32 PackageIndex = 0; PackageIndex < AllPackages.Num(); PackageIndex++)
		{
			FString PackageFilename(AllPackages[PackageIndex]);
			FString PackageName = FPackageName::FilenameToLongPackageName(PackageFilename);

			// the list of objects in this package
			FPackageObjects* PackageObjs = NULL;

			// this will be set to true if every object in the package is unnecessary
			bool bAreAllObjectsUnnecessary = false;

			if (FPaths::GetExtension(PackageFilename, true) == FPackageName::GetMapPackageExtension() )
			{
				UE_LOG(LogContentCommandlet, Warning, TEXT("Skipping map %s..."), *PackageFilename);
				continue;
			}
			else
			{
				// get the objects referenced by this package
				PackageObjs = AllReferencedPublicObjects.Find(*PackageName);

				// if the were no objects referenced in this package, we can just skip it, 
				// and mark the whole package as unreferenced
				if (PackageObjs == NULL)
				{
					UE_LOG(LogContentCommandlet, Warning, TEXT("No objects in %s were referenced..."), *PackageFilename);
					new(UnnecessaryPublicObjects) FUnreferencedObject(PackageName, 
						TEXT("ENTIRE PACKAGE"), IFileManager::Get().FileSize(*PackageFilename));

					// all objects in this package are unnecasstry
					bAreAllObjectsUnnecessary = true;
				}
				else if (PackageObjs->bIsFullyLoadedPackage)
				{
					UE_LOG(LogContentCommandlet, Warning, TEXT("Skipping fully loaded package %s..."), *PackageFilename);
					continue;
				}
				else
				{
					UE_LOG(LogContentCommandlet, Warning, TEXT("Scanning %s..."), *PackageFilename);
				}
			}

			BeginLoad();
			auto Linker = GetPackageLinker( NULL, *PackageFilename, LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
			EndLoad();

			// go through the exports in the package, looking for public objects
			for (int32 ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ExportIndex++)
			{
				FObjectExport& Export = Linker->ExportMap[ExportIndex];
				FString ExportName = Linker->GetExportFullName(ExportIndex);

				// some packages may have brokenness in them so we want to just continue so we can wrangle
				if( Export.ObjectName == NAME_None )
				{
					UE_LOG(LogContentCommandlet, Warning, TEXT( "    Export.ObjectName == NAME_None  for Package: %s " ), *PackageFilename );
					continue;
				}

				// make sure its outer is a package, and this isn't a package
				if (Linker->GetExportClassName(ExportIndex) == NAME_Package || 
					(!Export.OuterIndex.IsNull() && Linker->GetExportClassName(Export.OuterIndex) != NAME_Package))
				{
					continue;
				}

				// was it not already referenced?
				// NULL means it wasn't in the reffed public objects map for the package
				if (bAreAllObjectsUnnecessary || PackageObjs->ReferencedObjects.Find(ExportName) == NULL)
				{
					// is it public?
					if ((Export.ObjectFlags & RF_Public) != 0 && !bAreAllObjectsUnnecessary)
					{
						// if so, then add it to list of unused pcreateexportublic items
						new(UnnecessaryPublicObjects) FUnreferencedObject(PackageName, ExportName, Export.SerialSize);
					}

					// look for existing entry
					FPackageObjects* ObjectsInPackage = UnnecessaryObjectsByPackage.Find(*PackageFilename);
					// if not found, make a new one
					if (ObjectsInPackage == NULL)
					{
						ObjectsInPackage = &UnnecessaryObjectsByPackage.Add(*PackageFilename, FPackageObjects());
					}

					// get object's class
					FString ClassName;
					if(Export.ClassIndex.IsImport())
					{
						ClassName = Linker->GetImportPathName(Export.ClassIndex);
					}
					else
					{
						ClassName = Linker->GetExportPathName(Export.ClassIndex);
					}
					UClass* Class = StaticLoadClass(UObject::StaticClass(), NULL, *ClassName, NULL, LOAD_None, NULL);
					// When wrangling content, you often are loading packages that have not been saved in ages and have a reference to a class
					// that no longer exists.  Instead of asserting, we will just continue
					if( bShouldSkipMissingClasses == true )
					{
						if( Class == NULL )
						{
							continue;
						}
					}
					else
					{
						check(Class);
					}

					// make sure it doesn't get GC'd
					Class->AddToRoot();
				
					// add this referenced object to the map
					ObjectsInPackage->ReferencedObjects.Add(*ExportName, Class);

					// add this to the map of all unnecessary objects
					UnnecessaryObjects.Add(*ExportName, true);
				}
			}

			// collect garbage every 20 packages (we aren't fully loading, so it doesn't need to be often)
			if ((PackageIndex % 20) == 0)
			{
				CollectGarbage(RF_NoFlags);
			}
		}
	}

	if (bShouldSavePackages)
	{
		int32 NumPackages = AllReferencedPublicObjects.Num();

		// go through all packages, and save out referenced objects
		SET_WARN_COLOR(COLOR_WHITE);
		UE_LOG(LogContentCommandlet, Warning, TEXT(""));
		UE_LOG(LogContentCommandlet, Warning, TEXT("Saving referenced objects in %d Packages:"), NumPackages);
		CLEAR_WARN_COLOR();
		int32 PackageIndex = 0;
		for (TMap<FString, FPackageObjects>::TIterator It(AllReferencedPublicObjects); It; ++It, PackageIndex++ )
		{
			// if the package was a fully loaded package, than we already saved it
			if (It.Value().bIsFullyLoadedPackage)
			{
				continue;
			}

			// package for all loaded objects
			UPackage* Package = NULL;
			
			// fully load all the referenced objects in the package
			for (TMap<FString, UClass*>::TIterator It2(It.Value().ReferencedObjects); It2; ++It2)
			{
				// get the full object name
				FString ObjectPathName = It2.Key();

				// skip over the class portion (the It2.Value() has the class pointer already)
				int32 Space = ObjectPathName.Find(TEXT(" "));
				check(Space);

				// get everything after the space
				ObjectPathName = ObjectPathName.Right(ObjectPathName.Len() - (Space + 1));

				// load the referenced object

				UObject* Object = StaticLoadObject(It2.Value(), NULL, *ObjectPathName, NULL, LOAD_NoWarn, NULL);

				// the object may not exist, because of attempting to load localized content
				if (Object)
				{
					check(Object->GetPathName() == ObjectPathName);

					// set the package if needed
					if (Package == NULL)
					{
						Package = Object->GetOutermost();
					}
					else
					{
						// make sure all packages are the same
						check(Package == Object->GetOutermost());
					}
				}
			}

			// make sure we found some objects in here
			// Don't worry about script packages
			if (Package)
			{
				// mark this package as fully loaded so it can be saved, even though we didn't fully load it
				// (which is the point of this commandlet)
				Package->MarkAsFullyLoaded();

				// get original path of package
				FString OriginalPackageFilename;

				//UE_LOG(LogContentCommandlet, Warning, TEXT( "*It.Key(): %s" ), *It.Key() );

				// we need to be able to find the original package
				if( FPackageName::DoesPackageExist(It.Key(), NULL, &OriginalPackageFilename) == false )
				{
					UE_LOG(LogContentCommandlet, Fatal, TEXT( "Could not find file in file cache: %s"), *It.Key() );
				}

				// any maps need to be fully referenced
				check( FPaths::GetExtension(OriginalPackageFilename, true) != FPackageName::GetMapPackageExtension() );

				// make the filename for the output package
				FString CutdownPackageName = MakeCutdownFilename(OriginalPackageFilename);

				UE_LOG(LogContentCommandlet, Warning, TEXT("Saving %s... [%d/%d]"), *CutdownPackageName, PackageIndex + 1, NumPackages);

				// save the package now that all needed objects in it are loaded.
				// At this point, any object still around should be saved so we pass all flags so all objects are saved
				SavePackageHelper(Package, *CutdownPackageName, RF_AllFlags, GWarn, NULL, SAVE_CutdownPackage);

				// close up this package
				CollectGarbage(RF_NoFlags);
			}
		}
	}

	if (bShouldDumpUnreferencedContent)
	{
		SET_WARN_COLOR(COLOR_WHITE);
		UE_LOG(LogContentCommandlet, Warning, TEXT(""));
		UE_LOG(LogContentCommandlet, Warning, TEXT("Unreferenced Public Objects:"));
		CLEAR_WARN_COLOR();

		// create a .csv
		FString CSVFilename = FString::Printf(TEXT("%sUnreferencedObjects-%s.csv"), *FPaths::ProjectLogDir(), *FDateTime::Now().ToString());
		FArchive* CSVFile = IFileManager::Get().CreateFileWriter(*CSVFilename);

		if (!CSVFile)
		{
			UE_LOG(LogContentCommandlet, Error, TEXT("Failed to open output file %s"), *CSVFilename);
		}

		for (int32 ObjectIndex = 0; ObjectIndex < UnnecessaryPublicObjects.Num(); ObjectIndex++)
		{
			FUnreferencedObject& Object = UnnecessaryPublicObjects[ObjectIndex];
			UE_LOG(LogContentCommandlet, Warning, TEXT("%s"), *Object.ObjectName);

			// dump out a line to the .csv file
			// @todo: sort by size to Excel's 65536 limit gets the biggest objects
			FString CSVLine = FString::Printf(TEXT("%s,%s,%d%s"), *Object.PackageName, *Object.ObjectName, Object.SerialSize, LINE_TERMINATOR);
			CSVFile->Serialize(TCHAR_TO_ANSI(*CSVLine), CSVLine.Len());
		}
	}

	// load every unnecessary object by package, rename it and any unnecessary objects if uses, to the 
	// an unnecessary package, and save it
	if (bShouldSaveUnreferencedContent)
	{
		int32 NumPackages = UnnecessaryObjectsByPackage.Num();
		SET_WARN_COLOR(COLOR_WHITE);
		UE_LOG(LogContentCommandlet, Warning, TEXT(""));
		UE_LOG(LogContentCommandlet, Warning, TEXT("Saving unreferenced objects [%d packages]:"), NumPackages);
		CLEAR_WARN_COLOR();

		// go through each package that has unnecessary objects in it
		int32 PackageIndex = 0;
		for (TMap<FString, FPackageObjects>::TIterator PackageIt(UnnecessaryObjectsByPackage); PackageIt; ++PackageIt, PackageIndex++)
		{
			//UE_LOG(LogContentCommandlet, Warning, TEXT("Processing %s"), *PackageIt.Key());
			UPackage* FullyLoadedPackage = NULL;
			// fully load unnecessary packages with no objects, 
			if (PackageIt.Value().ReferencedObjects.Num() == 0)
			{
				// just load it, and don't need a reference to it
				FullyLoadedPackage = LoadPackage(NULL, *PackageIt.Key(), LOAD_None);
			}
			else
			{
				// load every unnecessary object in this package
				for (TMap<FString, UClass*>::TIterator ObjectIt(PackageIt.Value().ReferencedObjects); ObjectIt; ++ObjectIt)
				{
					// get the full object name
					FString ObjectPathName = ObjectIt.Key();

					// skip over the class portion (the It2.Value() has the class pointer already)
					int32 Space = ObjectPathName.Find(TEXT(" "));
					check(Space > 0);

					// get everything after the space
					ObjectPathName = ObjectPathName.Right(ObjectPathName.Len() - (Space + 1));

					// load the unnecessary object
					UObject* Object = StaticLoadObject(ObjectIt.Value(), NULL, *ObjectPathName, NULL, LOAD_NoWarn, NULL);
					
					// this object should exist since it was gotten from a linker
					if (!Object)
					{
						UE_LOG(LogContentCommandlet, Error, TEXT("Failed to load object %s, it will be deleted permanently!"), *ObjectPathName);
					}
				}
			}

			// now find all loaded objects (in any package) that are in marked as unnecessary,
			// and rename them to their destination
			for (TObjectIterator<UObject> It; It; ++It)
			{
				// if was unnecessary...
				if (UnnecessaryObjects.Find(*It->GetFullName()))
				{
					// ... then rename it (its outer needs to be a package, everything else will have to be
					// moved by its outer getting moved)
					if (!It->IsA(UPackage::StaticClass()) &&
						It->GetOuter() &&
						It->GetOuter()->IsA(UPackage::StaticClass()) &&
						It->GetOutermost()->GetName().Left(4) != TEXT("NFS_"))
					{
						UPackage* NewPackage = CreatePackage(NULL, *(FString(TEXT("NFS_")) + It->GetOuter()->GetPathName()));
						//UE_LOG(LogContentCommandlet, Warning, TEXT("Renaming object from %s to %s.%s"), *It->GetPathName(), *NewPackage->GetPathName(), *It->GetName());

						// move the object if we can. IF the rename fails, then the object was already renamed to this spot, but not GC'd.
						// that's okay.
						if (It->Rename(*It->GetName(), NewPackage, REN_Test))
						{
							It->Rename(*It->GetName(), NewPackage, REN_None);
						}
					}

				}
			}

			// find the one we moved this packages objects to
			FString PackagePath = PackageIt.Key();
			FString PackageName = FPackageName::FilenameToLongPackageName(PackagePath);
			UPackage* MovedPackage = FindPackage(NULL, *FString::Printf(TEXT("%s/NFS_%s"), *FPackageName::GetLongPackagePath(PackageName), *FPackageName::GetLongPackageAssetName(PackageName)));
			check(MovedPackage);

			// convert the new name to a a NFS directory directory
			FString MovedFilename = MakeCutdownFilename(FString::Printf(TEXT("%s/NFS_%s"), *FPaths::GetPath(PackagePath), *FPaths::GetCleanFilename(PackagePath)), TEXT("NFSContent"));
			UE_LOG(LogContentCommandlet, Warning, TEXT("Saving package %s [%d/%d]"), *MovedFilename, PackageIndex, NumPackages);
			// finally save it out
			SavePackageHelper(MovedPackage, *MovedFilename);

			CollectGarbage(RF_NoFlags);
		}
	}

	return 0;
}

/* ==========================================================================================================
	UListMaterialsUsedWithMeshEmittersCommandlet
========================================================================================================== */

UListMaterialsUsedWithMeshEmittersCommandlet::UListMaterialsUsedWithMeshEmittersCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UListMaterialsUsedWithMeshEmittersCommandlet::ProcessParticleSystem( UParticleSystem* ParticleSystem , TArray<FString> &OutMaterials)
{
	for (int32 EmitterIndex = 0; EmitterIndex < ParticleSystem->Emitters.Num(); EmitterIndex++)
	{
		UParticleEmitter *Emitter = ParticleSystem->Emitters[EmitterIndex];
		if (Emitter && Emitter->LODLevels.Num() > 0)
		{
			UParticleLODLevel* LODLevel = Emitter->LODLevels[0];
			// Only process mesh emitters
			if (LODLevel && 
				LODLevel->TypeDataModule && 
				LODLevel->TypeDataModule->IsA( UParticleModuleTypeDataMesh::StaticClass())) // The type data module is mesh type data
			{

				// Attempt to find MeshMaterial module on emitter.
				UParticleModuleTypeDataMesh* MeshTypeData = Cast<UParticleModuleTypeDataMesh>(LODLevel->TypeDataModule);
				bool bFoundMaterials = false;
				for( int32 ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ++ModuleIdx )
				{
					if(LODLevel->Modules[ModuleIdx]->IsA(UParticleModuleMeshMaterial::StaticClass()))
					{
						UParticleModuleMeshMaterial* MaterialModule = Cast<UParticleModuleMeshMaterial>( LODLevel->Modules[ ModuleIdx ] );
						for(int32 MatIdx = 0; MatIdx < MaterialModule->MeshMaterials.Num(); MatIdx++ )
						{
							if(MaterialModule->MeshMaterials[MatIdx])
							{
								bFoundMaterials = true;
								if(!MaterialModule->MeshMaterials[MatIdx]->GetMaterial()->bUsedWithMeshParticles)
								{
									OutMaterials.AddUnique(MaterialModule->MeshMaterials[MatIdx]->GetPathName());
								}
							}
						}
					}
				}

				// Check override material only if we've not found materials on a MeshMaterial module within the emitter
				if(!bFoundMaterials && MeshTypeData->bOverrideMaterial)
				{
					UMaterialInterface* OverrideMaterial = LODLevel->RequiredModule->Material;
					if(OverrideMaterial && !OverrideMaterial->GetMaterial()->bUsedWithMeshParticles)
					{
						OutMaterials.AddUnique(OverrideMaterial->GetMaterial()->GetPathName());
					}
				}

				// Find materials on the static mesh
				else if (!bFoundMaterials)
				{
					if (MeshTypeData->Mesh)
					{
						for (int32 MaterialIdx = 0; MaterialIdx < MeshTypeData->Mesh->StaticMaterials.Num(); MaterialIdx++)
						{
							if(MeshTypeData->Mesh->StaticMaterials[MaterialIdx].MaterialInterface)
							{
								UMaterial* Mat = MeshTypeData->Mesh->StaticMaterials[MaterialIdx].MaterialInterface->GetMaterial();
								if(!Mat->bUsedWithMeshParticles)
								{
									OutMaterials.AddUnique(Mat->GetPathName());
								}
							}							
						}
					}
				}
			}
		}
	}
}

int32 UListMaterialsUsedWithMeshEmittersCommandlet::Main( const FString& Params )
{

	TArray<FString> FilesInPath;
	FEditorFileUtils::FindAllPackageFiles(FilesInPath);

	if( FilesInPath.Num() == 0 )
	{
		UE_LOG(LogContentCommandlet, Warning,TEXT("No packages found"));
		return 1;
	}

	TArray<FString> MaterialList;
	int32 GCIndex = 0;
	int32 TotalPackagesChecked = 0;

	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Update Registry Module
	UE_LOG(LogContentCommandlet, Display, TEXT("Searching Asset Registry for particle systems"));
	AssetRegistryModule.Get().SearchAllAssets(true);

	// Retrieve list of all assets, used to find unreferenced ones.
	TArray<FAssetData> AssetList;
	AssetRegistryModule.Get().GetAssetsByClass(UParticleSystem::StaticClass()->GetFName(), AssetList, true);

	for(int32 AssetIdx = 0; AssetIdx < AssetList.Num(); ++AssetIdx )
	{
		const FString Filename = AssetList[AssetIdx].ObjectPath.ToString();

		UE_LOG(LogContentCommandlet, Display, TEXT("Processing particle system (%i/%i):  %s "), AssetIdx, AssetList.Num(), *Filename );

		UPackage* Package = LoadPackage( NULL, *Filename, LOAD_Quiet );
		if ( Package == NULL )
		{
			UE_LOG(LogContentCommandlet, Error, TEXT("Error loading %s!"), *Filename );
			continue;
		}

		TotalPackagesChecked++;
		for (TObjectIterator<UParticleSystem> It; It; ++It )
		{
			UParticleSystem* ParticleSys = *It;
			if (ParticleSys->IsIn(Package) && !ParticleSys->IsTemplate())
			{
				// For any mesh emitters we append to MaterialList any materials that are referenced and don't have bUsedWithMeshParticles set.
				ProcessParticleSystem(ParticleSys, MaterialList);
			}
		}

		// Collect garbage every 10 packages instead of every package makes the commandlet run much faster
		if( (++GCIndex % 10) == 0 )
		{
			CollectGarbage(RF_NoFlags);
		}
	}

	if(MaterialList.Num() > 0)
	{
		// Now, dump out the list of materials that require updating.
		UE_LOG(LogContentCommandlet, Display, TEXT("-------------------------------------------------------------------"));
		UE_LOG(LogContentCommandlet, Display, TEXT("The following materials require bUsedWithMeshParticles to be enabled:"));
		for(int32 Index = 0; Index < MaterialList.Num(); ++Index)
		{
			UE_LOG(LogContentCommandlet, Error, TEXT("%s"), *MaterialList[Index] );
		}
		UE_LOG(LogContentCommandlet, Display, TEXT("-------------------------------------------------------------------"));
	}
	else
	{
		UE_LOG(LogContentCommandlet, Display, TEXT("No materials require updating!"));
	}
	return 0;
}


/* ==========================================================================================================
	UListStaticMeshesImportedFromSpeedTreesCommandlet
========================================================================================================== */

UListStaticMeshesImportedFromSpeedTreesCommandlet::UListStaticMeshesImportedFromSpeedTreesCommandlet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

int32 UListStaticMeshesImportedFromSpeedTreesCommandlet::Main(const FString& Params)
{

	TArray<FString> FilesInPath;
	FEditorFileUtils::FindAllPackageFiles(FilesInPath);

	if (FilesInPath.Num() == 0)
	{
		UE_LOG(LogContentCommandlet, Warning, TEXT("No packages found"));
		return 1;
	}

	TArray<FString> StaticMeshList;
	int32 GCIndex = 0;
	int32 TotalPackagesChecked = 0;

	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Update Registry Module
	UE_LOG(LogContentCommandlet, Display, TEXT("Searching Asset Registry for static mesh "));
	AssetRegistryModule.Get().SearchAllAssets(true);

	// Retrieve list of all assets, used to find unreferenced ones.
	TArray<FAssetData> AssetList;
	AssetRegistryModule.Get().GetAssetsByClass(UStaticMesh::StaticClass()->GetFName(), AssetList, true);

	for (int32 AssetIdx = 0; AssetIdx < AssetList.Num(); ++AssetIdx)
	{
		const FString Filename = AssetList[AssetIdx].ObjectPath.ToString();

		UE_LOG(LogContentCommandlet, Display, TEXT("Processing static mesh (%i/%i):  %s "), AssetIdx, AssetList.Num(), *Filename);

		UPackage* Package = LoadPackage(NULL, *Filename, LOAD_Quiet);
		if (Package == NULL)
		{
			UE_LOG(LogContentCommandlet, Error, TEXT("Error loading %s!"), *Filename);
			continue;
		}

		TotalPackagesChecked++;
		for (TObjectIterator<UStaticMesh> It; It; ++It)
		{
			UStaticMesh* StaticMesh = *It;
			if (StaticMesh->IsIn(Package) && !StaticMesh->IsTemplate())
			{
				// If the mesh was imported from a speedtree, we append the static mesh name to the list.
				if (StaticMesh->SpeedTreeWind.IsValid())
				{
					StaticMeshList.Add(StaticMesh->GetPathName());
				}
			}
		}

		// Collect garbage every 10 packages instead of every package makes the commandlet run much faster
		if ((++GCIndex % 10) == 0)
		{
			CollectGarbage(RF_NoFlags);
		}
	}

	if (StaticMeshList.Num() > 0)
	{
		// Now, dump out the list of materials that require updating.
		UE_LOG(LogContentCommandlet, Display, TEXT("-------------------------------------------------------------------"));
		UE_LOG(LogContentCommandlet, Display, TEXT("The following static meshes were imported from SpeedTrees:"));
		for (int32 Index = 0; Index < StaticMeshList.Num(); ++Index)
		{
			UE_LOG(LogContentCommandlet, Error, TEXT("%s"), *StaticMeshList[Index]);
		}
		UE_LOG(LogContentCommandlet, Display, TEXT("-------------------------------------------------------------------"));
	}
	else
	{
		UE_LOG(LogContentCommandlet, Display, TEXT("No static meshes were imported from speedtrees in this project."));
	}
	return 0;
}

