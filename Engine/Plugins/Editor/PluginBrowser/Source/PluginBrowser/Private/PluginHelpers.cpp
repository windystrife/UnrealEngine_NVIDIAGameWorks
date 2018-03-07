// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PluginHelpers.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// For plugin asset renaming
#include "PluginBrowserModule.h"
#include "Interfaces/IPluginManager.h"

#include "IAssetRegistry.h"
#include "AssetRegistryModule.h"
#include "AssetData.h"
#include "PackageName.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "PluginHelpers"

// The text macro to replace with the actual plugin name when copying files
const FString PLUGIN_NAME = TEXT("PLUGIN_NAME");

bool FPluginHelpers::CopyPluginTemplateFolder(const TCHAR* DestinationDirectory, const TCHAR* Source, const FString& PluginName)
{
	check(DestinationDirectory);
	check(Source);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	FString DestDir(DestinationDirectory);
	FPaths::NormalizeDirectoryName(DestDir);

	FString SourceDir(Source);
	FPaths::NormalizeDirectoryName(SourceDir);

	// Does Source dir exist?
	if (!PlatformFile.DirectoryExists(*SourceDir))
	{
		return false;
	}

	// Destination directory exists already or can be created ?
	if (!PlatformFile.DirectoryExists(*DestDir) &&
		!PlatformFile.CreateDirectory(*DestDir))
	{
		return false;
	}

	// Copy all files and directories, renaming specific sections to the plugin name
	struct FCopyPluginFilesAndDirs : public IPlatformFile::FDirectoryVisitor
	{
		IPlatformFile& PlatformFile;
		const TCHAR* SourceRoot;
		const TCHAR* DestRoot;
		const FString& PluginName;
		TArray<FString> NameReplacementFileTypes;
		TArray<FString> IgnoredFileTypes;
		TArray<FString> CopyUnmodifiedFileTypes;

		FCopyPluginFilesAndDirs(IPlatformFile& InPlatformFile, const TCHAR* InSourceRoot, const TCHAR* InDestRoot, const FString& InPluginName)
			: PlatformFile(InPlatformFile)
			, SourceRoot(InSourceRoot)
			, DestRoot(InDestRoot)
			, PluginName(InPluginName)
		{
			// Which file types we want to replace instances of PLUGIN_NAME with the new Plugin Name
			NameReplacementFileTypes.Add(TEXT("cs"));
			NameReplacementFileTypes.Add(TEXT("cpp"));
			NameReplacementFileTypes.Add(TEXT("h"));
			NameReplacementFileTypes.Add(TEXT("vcxproj"));

			// Which file types do we want to ignore
			IgnoredFileTypes.Add(TEXT("opensdf"));
			IgnoredFileTypes.Add(TEXT("sdf"));
			IgnoredFileTypes.Add(TEXT("user"));
			IgnoredFileTypes.Add(TEXT("suo"));

			// Which file types do we want to copy completely unmodified
			CopyUnmodifiedFileTypes.Add(TEXT("uasset"));
			CopyUnmodifiedFileTypes.Add(TEXT("umap"));
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			FString NewName(FilenameOrDirectory);
			// change the root and rename paths/files
			NewName.RemoveFromStart(SourceRoot);
			NewName = NewName.Replace(*PLUGIN_NAME, *PluginName, ESearchCase::CaseSensitive);
			NewName = FPaths::Combine(DestRoot, *NewName);

			if (bIsDirectory)
			{
				// create new directory structure
				if (!PlatformFile.CreateDirectoryTree(*NewName) && !PlatformFile.DirectoryExists(*NewName))
				{
					return false;
				}
			}
			else
			{
				FString NewExt = FPaths::GetExtension(FilenameOrDirectory);

				if (!IgnoredFileTypes.Contains(NewExt))
				{
					if (CopyUnmodifiedFileTypes.Contains(NewExt))
					{
						// Copy unmodified files with the original name, but do rename the directories
						FString CleanFilename = FPaths::GetCleanFilename(FilenameOrDirectory);
						FString CopyToPath = FPaths::GetPath(NewName);

						NewName = FPaths::Combine(CopyToPath, CleanFilename);
					}
					
					if (PlatformFile.FileExists(*NewName))
					{
						// Delete destination file if it exists
						PlatformFile.DeleteFile(*NewName);
					}

					// If file of specified extension - open the file as text and replace PLUGIN_NAME in there before saving
					if (NameReplacementFileTypes.Contains(NewExt))
					{
						FString OutFileContents;
						if (!FFileHelper::LoadFileToString(OutFileContents, FilenameOrDirectory))
						{
							return false;
						}

						OutFileContents = OutFileContents.Replace(*PLUGIN_NAME, *PluginName, ESearchCase::CaseSensitive);

						// For some content, we also want to export a PLUGIN_NAME_API text macro, which requires that the plugin name
						// be all capitalized

						FString PluginNameAPI = PluginName + TEXT("_API");

						OutFileContents = OutFileContents.Replace(*PluginNameAPI, *PluginNameAPI.ToUpper(), ESearchCase::CaseSensitive);

						if (!FFileHelper::SaveStringToFile(OutFileContents, *NewName))
						{
							return false;
						}
					}
					else
					{
						// Copy file from source
						if (!PlatformFile.CopyFile(*NewName, FilenameOrDirectory))
						{
							// Not all files could be copied
							return false;
						}
					}
				}
			}
			return true; // continue searching
		}
	};

	// copy plugin files and directories visitor
	FCopyPluginFilesAndDirs CopyFilesAndDirs(PlatformFile, *SourceDir, *DestDir, PluginName);

	// create all files subdirectories and files in subdirectories!
	return PlatformFile.IterateDirectoryRecursively(*SourceDir, CopyFilesAndDirs);
}

bool FPluginHelpers::FixupPluginTemplateAssets(const FString& PluginName)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);

	struct FFixupPluginAssets : public IPlatformFile::FDirectoryVisitor
	{
		IPlatformFile& PlatformFile;
		const FString& PluginName;
		const FString& PluginBaseDir;

		TArray<FString> FilesToScan;

		FFixupPluginAssets(IPlatformFile& InPlatformFile, const FString& InPluginName, const FString& InPluginBaseDir)
			: PlatformFile(InPlatformFile)
			, PluginName(InPluginName)
			, PluginBaseDir(InPluginBaseDir)
		{
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if (!bIsDirectory)
			{
				FString Extension = FPaths::GetExtension(FilenameOrDirectory);

				// Only interested in fixing up uassets and umaps...anything else we leave alone
				if (Extension == TEXT("uasset") || Extension == TEXT("umap"))
				{
					FilesToScan.Add(FilenameOrDirectory);
				}
			}

			return true;
		}

		/**
		 * Fixes up any assets that contain the PLUGIN_NAME text macro, since those need to be renamed by the engine for the change to
		 * stick (as opposed to just renaming the file)
		 */
		void PerformFixup()
		{
			TArray<FAssetRenameData> AssetRenameData;

			if (FilesToScan.Num() > 0)
			{
				IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
				AssetRegistry.ScanFilesSynchronous(FilesToScan);

				for (const FString& File : FilesToScan)
				{
					TArray<FAssetData> Assets;

					FString PackageName;
					if (FPackageName::TryConvertFilenameToLongPackageName(File, PackageName))
					{
						AssetRegistry.GetAssetsByPackageName(*PackageName, Assets);
					}

					for (FAssetData Asset : Assets)
					{
						FString AssetName = Asset.AssetName.ToString().Replace(*PLUGIN_NAME, *PluginName, ESearchCase::CaseSensitive);
						FString AssetPath = Asset.PackagePath.ToString().Replace(*PLUGIN_NAME, *PluginName, ESearchCase::CaseSensitive);

						FAssetRenameData RenameData(Asset.GetAsset(), AssetPath, AssetName);

						AssetRenameData.Add(RenameData);
					}
				}

				if (AssetRenameData.Num() > 0)
				{
					FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
					AssetToolsModule.Get().RenameAssets(AssetRenameData);
				}
			}
		}
	};

	bool bFixupSuccessful = false;

	if (Plugin.IsValid())
	{
		FString BaseDir = Plugin->GetBaseDir();

		FFixupPluginAssets FixupPluginAssets(PlatformFile, PluginName, Plugin->GetBaseDir());

		bFixupSuccessful = PlatformFile.IterateDirectoryRecursively(*BaseDir, FixupPluginAssets);

		if (bFixupSuccessful)
		{
			FixupPluginAssets.PerformFixup();
		}
	}

	return bFixupSuccessful;
}

#undef LOCTEXT_NAMESPACE
