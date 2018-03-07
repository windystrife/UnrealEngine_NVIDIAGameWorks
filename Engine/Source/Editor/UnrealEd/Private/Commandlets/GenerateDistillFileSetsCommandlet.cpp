// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Commandlets/GenerateDistillFileSetsCommandlet.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "UObject/UObjectHash.h"
#include "Misc/PackageName.h"
#include "Settings/ProjectPackagingSettings.h"
#include "FileHelpers.h"
#include "RedirectCollector.h"
#include "Editor.h"
#include "Engine/AssetManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGenerateDistillFileSetsCommandlet, Log, All);

UGenerateDistillFileSetsCommandlet::UGenerateDistillFileSetsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

int32 UGenerateDistillFileSetsCommandlet::Main( const FString& InParams )
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;
	UCommandlet::ParseCommandLine(*InParams, Tokens, Switches);

	TArray<FString> MapList;
	for ( int32 MapIdx = 0; MapIdx < Tokens.Num(); ++MapIdx )
	{
		const FString& Map = Tokens[MapIdx];
		if ( FPackageName::IsShortPackageName(Map) )
		{
			FString LongPackageName;
			if ( FPackageName::SearchForPackageOnDisk(Map, &LongPackageName) )
			{
				MapList.Add(LongPackageName);
			}
			else
			{
				UE_LOG(LogGenerateDistillFileSetsCommandlet, Error, TEXT("Unable to find package for map %s."), *Map);
				return 1;
			}
		}
		else
		{
			MapList.Add(Map);
		}
	}

	const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();

	if ( MapList.Num() <= 0 )
	{
		// No map tokens were supplied on the command line, so assume all maps
		TArray<FString> AllPackageFilenames;
		FEditorFileUtils::FindAllPackageFiles(AllPackageFilenames);
		for (int32 PackageIndex = 0; PackageIndex < AllPackageFilenames.Num(); PackageIndex++)
		{
			const FString& Filename = AllPackageFilenames[PackageIndex];
			if (FPaths::GetExtension(Filename, true) == FPackageName::GetMapPackageExtension() )
			{
				FString LongPackageName;
				if ( FPackageName::TryConvertFilenameToLongPackageName(Filename, LongPackageName) )
				{
					// Warn about maps in "NoShip" or "TestMaps" folders.  Those should have been filtered out during the Distill process!
					if( !Filename.Contains( "/NoShip/") && !Filename.Contains( "/TestMaps/"))
					{
						// @todo plugins add support for plugins?
						if ( LongPackageName.StartsWith(TEXT("/Game")) )
						{
							UE_LOG(LogGenerateDistillFileSetsCommandlet, Display, TEXT( "Discovered map package %s..." ), *LongPackageName );
							MapList.Add(LongPackageName);
						}
					}
					else
					{
						UE_LOG(LogGenerateDistillFileSetsCommandlet, Display, TEXT("Skipping map package %s in TestMaps or NoShip folder"), *Filename);
					}
				}
				else
				{
					UE_LOG(LogGenerateDistillFileSetsCommandlet, Warning, TEXT("Failed to determine package name for map file %s."), *Filename);
				}
			}
		}
	}
	else
	{
		// Add the default map section
		TArray<FString> AlwaysCookMapList;
		GEditor->LoadMapListFromIni(TEXT("AlwaysCookMaps"), AlwaysCookMapList);
		MapList.Append(AlwaysCookMapList);

		// Add Maps to cook from project packaging settings if any exist
		for (const FFilePath& MapToCook : PackagingSettings->MapsToCook)
		{
			MapList.AddUnique(MapToCook.FilePath);
		}
	}

	// Add any assets from the asset manager
	if (UAssetManager::IsValid())
	{
		UAssetManager& Manager = UAssetManager::Get();
		TArray<FPrimaryAssetTypeInfo> TypeInfos;
		Manager.GetPrimaryAssetTypeInfoList(TypeInfos);

		for (const FPrimaryAssetTypeInfo& TypeInfo : TypeInfos)
		{
			TArray<FAssetData> AssetDataList;

			Manager.GetPrimaryAssetDataList(TypeInfo.PrimaryAssetType, AssetDataList);

			for (const FAssetData& AssetData : AssetDataList)
			{
				FString PackageName = AssetData.PackageName.ToString();
				// Warn about maps in "NoShip" or "TestMaps" folders.
				if (PackageName.Contains("/NoShip/") || PackageName.Contains("/TestMaps/"))
				{
					UE_LOG(LogGenerateDistillFileSetsCommandlet, Display, TEXT("Skipping map package %s in TestMaps or NoShip folder"), *PackageName);
					continue;
				}
				MapList.AddUnique(AssetData.PackageName.ToString());
			}
		}
	}
	
	const FString TemplateFileSwitch = TEXT("Template=");
	const FString OutputFileSwitch = TEXT("Output=");
	const FString TemplateFolderSwitch = TEXT("TemplateFolder=");
	const FString OutputFolderSwitch = TEXT("OutputFolder=");
	FString TemplateFilename;
	FString OutputFilename;
	FString TemplateFolder;
	FString OutputFolder;


	for (int32 SwitchIdx = 0; SwitchIdx < Switches.Num(); ++SwitchIdx)
	{
		const FString& Switch = Switches[SwitchIdx];
		if ( Switch.StartsWith(TemplateFileSwitch) )
		{
			Switch.Split(TEXT("="), NULL, &TemplateFilename);
			TemplateFilename = TemplateFilename.TrimQuotes();
		}
		else if ( Switch.StartsWith(OutputFileSwitch) )
		{
			Switch.Split(TEXT("="), NULL, &OutputFilename);
			OutputFilename = OutputFilename.TrimQuotes();
		}
		else if ( Switch.StartsWith(TemplateFolderSwitch) )
		{
			Switch.Split(TEXT("="), NULL, &TemplateFolder);
			TemplateFolder = TemplateFolder.TrimQuotes();
			FPaths::NormalizeFilename(TemplateFolder);
			if ( !TemplateFolder.EndsWith(TEXT("/")) )
			{
				TemplateFolder += TEXT("/");
			}
			UE_LOG(LogGenerateDistillFileSetsCommandlet, Display, TEXT("Using template folder: %s"), *TemplateFolder);
		}
		else if ( Switch.StartsWith(OutputFolderSwitch) )
		{
			Switch.Split(TEXT("="), NULL, &OutputFolder);
			OutputFolder = OutputFolder.TrimQuotes();
			FPaths::NormalizeFilename(OutputFolder);
			if ( !OutputFolder.EndsWith(TEXT("/")) )
			{
				OutputFolder += TEXT("/");
			}
			UE_LOG(LogGenerateDistillFileSetsCommandlet, Display, TEXT("Using output folder: %s"), *OutputFolder);
		}
	}

	if ( OutputFilename.IsEmpty() )
	{
		UE_LOG(LogGenerateDistillFileSetsCommandlet, Error, TEXT("You must supply -Output=OutputFilename. These files are relative to the Game/Build directory."));
		return 1;
	}
	if (OutputFolder.IsEmpty())
	{
		OutputFolder = FPaths::ProjectDir() + TEXT("Build/");
	}
	OutputFilename = OutputFolder + OutputFilename;

	bool bSimpleTxtOutput = false;
	// Load the template file
	FString TemplateFileContents;

	if (TemplateFilename.IsEmpty())
	{
		UE_LOG(LogGenerateDistillFileSetsCommandlet, Log, TEXT("No template specified, assuming a simple txt output."));
		bSimpleTxtOutput = true;
	}
	// If no folder was specified, filenames are relative to the build dir.
	else 
	{
		if (TemplateFolder.IsEmpty())
		{
			TemplateFolder = FPaths::ProjectDir() + TEXT("Build/");
		}
		TemplateFilename = TemplateFolder + TemplateFilename;

		if (!FFileHelper::LoadFileToString(TemplateFileContents, *TemplateFilename))
		{
			UE_LOG(LogGenerateDistillFileSetsCommandlet, Error, TEXT("Failed to load template file '%s'"), *TemplateFilename);
			return 1;
		}
	}




	// Form a full unique package list
	TSet<FString> AllPackageNames;

	//@todo SLATE: This is a hack to ensure all slate referenced assets get cooked.
	// Slate needs to be refactored to properly identify required assets at cook time.
	// Simply jamming everything in a given directory into the cook list is error-prone
	// on many levels - assets not required getting cooked/shipped; assets not put under 
	// the correct folder; etc.
	{
		TArray<FString> UIContentPaths;
		if (GConfig->GetArray(TEXT("UI"), TEXT("ContentDirectories"), UIContentPaths, GEditorIni) > 0)
		{
			for (int32 DirIdx = 0; DirIdx < UIContentPaths.Num(); DirIdx++)
			{
				FString ContentPath = FPackageName::LongPackageNameToFilename(UIContentPaths[DirIdx]);

				TArray<FString> Files;
				IFileManager::Get().FindFilesRecursive(Files, *ContentPath, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
				for (int32 Index = 0; Index < Files.Num(); Index++)
				{
					FString StdFile = Files[Index];
					FPaths::MakeStandardFilename(StdFile);
					StdFile = FPackageName::FilenameToLongPackageName(StdFile);
					AllPackageNames.Add(StdFile);
				}
			}

		}
	}

	// Load all maps
	{
		for ( auto MapIt = MapList.CreateConstIterator(); MapIt; ++MapIt )
		{
			const FString& MapPackage = *MapIt;
			UE_LOG(LogGenerateDistillFileSetsCommandlet, Display, TEXT( "Loading %s..." ), *MapPackage );
			UPackage* Package = LoadPackage( NULL, *MapPackage, LOAD_None );
			if( Package != NULL )
			{
				GRedirectCollector.ResolveAllSoftObjectPaths();

				AllPackageNames.Add(Package->GetName());

				UE_LOG(LogGenerateDistillFileSetsCommandlet, Display, TEXT( "Finding content referenced by %s..." ), *MapPackage );
				TArray<UObject *> ObjectsInOuter;
				GetObjectsWithOuter(NULL, ObjectsInOuter, false);
				for (int32 Index = 0; Index < ObjectsInOuter.Num(); Index++)
				{
					FString OtherName = ObjectsInOuter[Index]->GetOutermost()->GetName();
					if (!AllPackageNames.Contains(OtherName))
					{
						AllPackageNames.Add(OtherName);
						UE_LOG(LogGenerateDistillFileSetsCommandlet, Log, TEXT("Package: %s"), *OtherName);
					}
				}
				UE_LOG(LogGenerateDistillFileSetsCommandlet, Display, TEXT( "Collecting garbage..." ) );
				CollectGarbage(RF_NoFlags);
			}
		}
	}

	// Add assets from additional directories to always cook
	const FString AbsoluteGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	for (const auto& DirToCook : PackagingSettings->DirectoriesToAlwaysCook)
	{
		FString DirectoryPath = AbsoluteGameContentDir / DirToCook.Path;
		UE_LOG(LogGenerateDistillFileSetsCommandlet, Log, TEXT( "Examining directory to always cook: %s..." ), *DirToCook.Path );

		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *DirectoryPath, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
		for (int32 Index = 0; Index < Files.Num(); Index++)
		{
			FString StdFile = Files[Index];
			FPaths::MakeStandardFilename(StdFile);
			StdFile = FPackageName::FilenameToLongPackageName(StdFile);
			AllPackageNames.Add(StdFile);
			UE_LOG(LogGenerateDistillFileSetsCommandlet, Log, TEXT( "Package: %s" ), *StdFile );
		}
	}

	// Sort the results to make it easier to diff files. No necessary but useful sometimes.
	TArray<FString> SortedPackageNames = AllPackageNames.Array();
	SortedPackageNames.Sort();

	// For the list of FileSets to include in the distill
	FString AllFileSets;
	const FString FileSetPathRoot = TEXT("Content");
	for (auto PackageIt = SortedPackageNames.CreateConstIterator(); PackageIt; ++PackageIt)
	{
		const FString& PackageName = *PackageIt;
		// @todo plugins add support for plugins?
		if ( PackageName.StartsWith(TEXT("/Game")) )
		{
			const FString PathWithoutRoot( PackageName.Mid( 5 ) );
			const FString FileSetPath = FileSetPathRoot + PathWithoutRoot;
			if (bSimpleTxtOutput)
			{
				FString ActualFile;
				if (FPackageName::DoesPackageExist(PackageName, NULL, &ActualFile))
				{
					ActualFile = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ActualFile);
					AllFileSets += FString::Printf(TEXT("%s") LINE_TERMINATOR, *ActualFile);
					UE_LOG(LogGenerateDistillFileSetsCommandlet, Log, TEXT("File: %s"), *ActualFile);
				}
			}
			else
			{
				AllFileSets += FString::Printf(TEXT("<FileSet Path=\"%s.*\" bIsRecursive=\"false\"/>") LINE_TERMINATOR, *FileSetPath);
			}
		}
	}

	// Write the output file
	FString OutputFileContents;
	if (bSimpleTxtOutput)
	{
		OutputFileContents = AllFileSets;
	}
	else
	{
		OutputFileContents = TemplateFileContents.Replace(TEXT("%INSTALLEDCONTENTFILESETS%"), *AllFileSets, ESearchCase::CaseSensitive);
		if (FApp::HasProjectName())
		{
			UE_LOG(LogGenerateDistillFileSetsCommandlet, Display, TEXT("Replacing %%GAMENAME%% with (%s)..."), FApp::GetProjectName());
			OutputFileContents = OutputFileContents.Replace(TEXT("%GAMENAME%"), FApp::GetProjectName(), ESearchCase::CaseSensitive);
		}
		else
		{
			UE_LOG(LogGenerateDistillFileSetsCommandlet, Warning, TEXT("Failed to replace %%GAMENAME%% since we are running without a game name."));
		}
	}

	if ( !FFileHelper::SaveStringToFile(OutputFileContents, *OutputFilename) )
	{
		UE_LOG(LogGenerateDistillFileSetsCommandlet, Error, TEXT("Failed to save output file '%s'"), *OutputFilename);
		return 1;
	}

	return 0;
}
