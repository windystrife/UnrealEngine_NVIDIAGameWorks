// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PackFactory.cpp: Factory for importing asset and feature packs
=============================================================================*/

#include "Factories/PackFactory.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/LinkerLoad.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "SourceControlHelpers.h"
#include "ISourceControlModule.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/InputSettings.h"
#include "IPlatformFilePak.h"
#include "SourceCodeNavigation.h"
#include "Misc/HotReloadInterface.h"
#include "Misc/AES.h"
#include "GameProjectGenerationModule.h"
#include "Dialogs/SOutputLogDialog.h"
#include "UniquePtr.h"
#include "Logging/MessageLog.h"
#include "CoreDelegates.h"

DEFINE_LOG_CATEGORY_STATIC(LogPackFactory, Log, All);

UPackFactory::UPackFactory(const FObjectInitializer& PCIP)
	: Super(PCIP)
{
	// Since this factory can output multiple and any number of class it doesn't really have a 
	// SupportedClass per say, but one must be defined, so we just reference ourself
	SupportedClass = UPackFactory::StaticClass();

	Formats.Add(TEXT("upack;Asset Pack"));
	Formats.Add(TEXT("upack;Feature Pack"));

	bEditorImport = true;
}

namespace PackFactoryHelper
{
	// Utility function to copy a single pak entry out of the Source archive and in to the Destination archive using Buffer as temporary space
	bool BufferedCopyFile(FArchive& DestAr, FArchive& Source, const FPakEntry& Entry, TArray<uint8>& Buffer)
	{	
		// Align down
		const int64 BufferSize = Buffer.Num() & ~(FAES::AESBlockSize-1);
		int64 RemainingSizeToCopy = Entry.Size;
		while (RemainingSizeToCopy > 0)
		{
			const int64 SizeToCopy = FMath::Min(BufferSize, RemainingSizeToCopy);
			// If file is encrypted so we need to account for padding
			int64 SizeToRead = Entry.bEncrypted ? Align(SizeToCopy,FAES::AESBlockSize) : SizeToCopy;

			const ANSICHAR* Key = nullptr;
			FCoreDelegates::FPakEncryptionKeyDelegate& Delegate = FCoreDelegates::GetPakEncryptionKeyDelegate();
			if (Delegate.IsBound())
			{
				Key = Delegate.Execute();
			}

			Source.Serialize(Buffer.GetData(),SizeToRead);
			if (Entry.bEncrypted)
			{
				FAES::DecryptData(Buffer.GetData(),SizeToRead, Key);
			}
			DestAr.Serialize(Buffer.GetData(), SizeToCopy);
			RemainingSizeToCopy -= SizeToRead;
		}
		return true;
	}

	// Utility function to uncompress and copy a single pak entry out of the Source archive and in to the Destination archive using PersistentBuffer as temporary space
	bool UncompressCopyFile(FArchive& DestAr, FArchive& Source, const FPakEntry& Entry, TArray<uint8>& PersistentBuffer)
	{
		if (Entry.UncompressedSize == 0)
		{
			return false;
		}

		int64 WorkingSize = Entry.CompressionBlockSize;
		int32 MaxCompressionBlockSize = FCompression::CompressMemoryBound((ECompressionFlags)Entry.CompressionMethod, WorkingSize, FPlatformMisc::GetPlatformCompression()->GetCompressionBitWindow());
		WorkingSize += MaxCompressionBlockSize;
		if (PersistentBuffer.Num() < WorkingSize)
		{
			PersistentBuffer.SetNumUninitialized(WorkingSize);
		}

		uint8* UncompressedBuffer = PersistentBuffer.GetData() + MaxCompressionBlockSize;

		for (uint32 BlockIndex=0, BlockIndexNum=Entry.CompressionBlocks.Num(); BlockIndex < BlockIndexNum; ++BlockIndex)
		{
			uint32 CompressedBlockSize = Entry.CompressionBlocks[BlockIndex].CompressedEnd - Entry.CompressionBlocks[BlockIndex].CompressedStart;
			uint32 UncompressedBlockSize = (uint32)FMath::Min<int64>(Entry.UncompressedSize - Entry.CompressionBlockSize*BlockIndex, Entry.CompressionBlockSize);
			Source.Seek(Entry.CompressionBlocks[BlockIndex].CompressedStart);
			uint32 SizeToRead = Entry.bEncrypted ? Align(CompressedBlockSize, FAES::AESBlockSize) : CompressedBlockSize;
			Source.Serialize(PersistentBuffer.GetData(), SizeToRead);

			if (Entry.bEncrypted)
			{
				const ANSICHAR* Key = nullptr;
				FCoreDelegates::FPakEncryptionKeyDelegate& Delegate = FCoreDelegates::GetPakEncryptionKeyDelegate();
				if (Delegate.IsBound())
				{
					Key = Delegate.Execute();
				}

				FAES::DecryptData(PersistentBuffer.GetData(), SizeToRead, Key);
			}

			if(!FCompression::UncompressMemory((ECompressionFlags)Entry.CompressionMethod,UncompressedBuffer,UncompressedBlockSize,PersistentBuffer.GetData(),CompressedBlockSize, false, FPlatformMisc::GetPlatformCompression()->GetCompressionBitWindow()))
			{
				return false;
			}
			DestAr.Serialize(UncompressedBuffer,UncompressedBlockSize);
		}

		return true;
	}

	// Utility function to extract a pak entry out of the memory reader containing the pak file and place in the destination archive.
	// Uses Buffer or PersistentCompressionBuffer depending on whether the entry is compressed or not.
	void ExtractFile(const FPakEntry& Entry, FBufferReader& PakReader, TArray<uint8>& Buffer, TArray<uint8>& PersistentCompressionBuffer, FArchive& DestAr)
	{
		if (Entry.CompressionMethod == COMPRESS_None)
		{
			PackFactoryHelper::BufferedCopyFile(DestAr, PakReader, Entry, Buffer);
		}
		else
		{
			PackFactoryHelper::UncompressCopyFile(DestAr, PakReader, Entry, PersistentCompressionBuffer);
		}
	}

	// Utility function to extract a pak entry out of the memory reader containing the pak file and place in a string.
	// Uses Buffer or PersistentCompressionBuffer depending on whether the entry is compressed or not.
	void ExtractFileToString(const FPakEntry& Entry, FBufferReader& PakReader, TArray<uint8>& Buffer, TArray<uint8>& PersistentCompressionBuffer, FString& FileContents)
	{
		TArray<uint8> Contents;
		FMemoryWriter MemWriter(Contents);

		ExtractFile(Entry, PakReader, Buffer, PersistentCompressionBuffer, MemWriter);

		// Add a line feed at the end because the FString archive read will consume the last byte
		Contents.Add('\n');

		// Insert the length of the string to the front of the memory chunk so we can use FString archive read
		const int32 StringLength = Contents.Num();
		Contents.InsertUninitialized(0,sizeof(int32));
		*(reinterpret_cast<int32*>(Contents.GetData())) = StringLength;

		FMemoryReader MemReader(Contents);
		MemReader << FileContents;
	}

	struct FPackConfigParameters
	{
		FPackConfigParameters()
			: bContainsSource(false)
			, bCompileSource(true)
		{
		}

		uint8 bContainsSource:1;
		uint8 bCompileSource:1;
		FString GameName;
		FString InstallMessage;
		TArray<FString> AdditionalFilesToAdd;
	};

	// Takes a string that represents the contents of a config file and sets up the supported config properties based on it
	// Currently we support Action and Axis Mappings and a GameName (for setting up redirects)
	void ProcessPackConfig(const FString& ConfigString, FPackConfigParameters& ConfigParameters)
	{
		FConfigFile PackConfig;
		PackConfig.ProcessInputFileContents(ConfigString);

		// Input Settings
		static UArrayProperty* ActionMappingsProp = FindFieldChecked<UArrayProperty>(UInputSettings::StaticClass(), GET_MEMBER_NAME_CHECKED(UInputSettings, ActionMappings));
		static UArrayProperty* AxisMappingsProp = FindFieldChecked<UArrayProperty>(UInputSettings::StaticClass(), GET_MEMBER_NAME_CHECKED(UInputSettings, AxisMappings));

		UInputSettings* InputSettingsCDO = GetMutableDefault<UInputSettings>();
		bool bCheckedOut = false;

		FConfigSection* InputSettingsSection = PackConfig.Find("InputSettings");
		if (InputSettingsSection)
		{
			TArray<FInputActionKeyMapping> ActionMappingsToAdd;
			TArray<FInputAxisKeyMapping> AxisMappingsToAdd;

			for (auto SettingPair : *InputSettingsSection)
			{
				struct FMatchMappingByName
				{
					FMatchMappingByName(const FName InName)
						: Name(InName)
					{
					}

					bool operator() (const FInputActionKeyMapping& ActionMapping)
					{
						return ActionMapping.ActionName == Name;
					}

					bool operator() (const FInputAxisKeyMapping& AxisMapping)
					{
						return AxisMapping.AxisName == Name;
					}

					FName Name;
				};

				if (SettingPair.Key.ToString().Contains("ActionMappings"))
				{
					FInputActionKeyMapping ActionKeyMapping;
					ActionMappingsProp->Inner->ImportText(*SettingPair.Value.GetValue(), &ActionKeyMapping, PPF_None, nullptr);

					if (InputSettingsCDO->ActionMappings.FindByPredicate(FMatchMappingByName(ActionKeyMapping.ActionName)) == nullptr)
					{
						ActionMappingsToAdd.Add(ActionKeyMapping);
					}
				}
				else if (SettingPair.Key.ToString().Contains("AxisMappings"))
				{
					FInputAxisKeyMapping AxisKeyMapping;
					AxisMappingsProp->Inner->ImportText(*SettingPair.Value.GetValue(), &AxisKeyMapping, PPF_None, nullptr);

					if (InputSettingsCDO->AxisMappings.FindByPredicate(FMatchMappingByName(AxisKeyMapping.AxisName)) == nullptr)
					{
						AxisMappingsToAdd.Add(AxisKeyMapping);
					}
				}
			}

			if (ActionMappingsToAdd.Num() > 0 || AxisMappingsToAdd.Num() > 0)
			{
				if (ISourceControlModule::Get().IsEnabled())
				{
					FText ErrorMessage;

					const FString InputSettingsFilename = FPaths::ConvertRelativePathToFull(InputSettingsCDO->GetDefaultConfigFilename());
					if (!SourceControlHelpers::CheckoutOrMarkForAdd(InputSettingsFilename, FText::FromString(InputSettingsFilename), NULL, ErrorMessage))
					{
						UE_LOG(LogPackFactory, Error, TEXT("%s"), *ErrorMessage.ToString());
					}
				}

				for (const FInputActionKeyMapping& ActionKeyMapping : ActionMappingsToAdd)
				{
					InputSettingsCDO->AddActionMapping(ActionKeyMapping);
				}
				for (const FInputAxisKeyMapping& AxisKeyMapping : AxisMappingsToAdd)
				{
					InputSettingsCDO->AddAxisMapping(AxisKeyMapping);
				}
					
				InputSettingsCDO->SaveKeyMappings();
				InputSettingsCDO->UpdateDefaultConfigFile();
			}
		}

		FConfigSection* RedirectsSection = PackConfig.Find("Redirects");
		if (RedirectsSection)
		{	
			if (FConfigValue* GameName = RedirectsSection->Find("GameName"))
			{
				ConfigParameters.GameName = GameName->GetValue();
			}
		}

		FConfigSection* AdditionalFilesSection = PackConfig.Find("AdditionalFilesToAdd");
		if (AdditionalFilesSection)
		{
			for (auto FilePair : *AdditionalFilesSection)
			{
				if (FilePair.Key.ToString().Contains("Files"))
				{
					FString Filename = FPaths::GetCleanFilename(FilePair.Value.GetValue());
					FString Directory = FPaths::RootDir() / FPaths::GetPath(FilePair.Value.GetValue());
					FPaths::MakeStandardFilename(Directory);
					FPakFile::MakeDirectoryFromPath(Directory);

					if (Filename.Contains(TEXT("*")))
					{
						TArray<FString> FoundFiles;
						IFileManager::Get().FindFilesRecursive(FoundFiles, *Directory, *Filename, true, false);
						ConfigParameters.AdditionalFilesToAdd.Append(FoundFiles);
						if (!ConfigParameters.bContainsSource)
						{
							for (const FString& FoundFile : FoundFiles)
							{
								if (FoundFile.StartsWith(TEXT("Source/")) || FoundFile.Contains(TEXT("/Source/")))
								{
									ConfigParameters.bContainsSource = true;
									break;
								}
							}
						}
					}
					else
					{
						ConfigParameters.AdditionalFilesToAdd.Add(Directory / Filename);
						if (!ConfigParameters.bContainsSource && (ConfigParameters.AdditionalFilesToAdd.Last().StartsWith(TEXT("Source/")) || ConfigParameters.AdditionalFilesToAdd.Last().Contains(TEXT("/Source/"))))
						{
							ConfigParameters.bContainsSource = true;
						}
					}
				}
			}
		}

		FConfigSection* FeaturePackSettingsSection = PackConfig.Find("FeaturePackSettings");
		if (FeaturePackSettingsSection)
		{
			if (FConfigValue* CompileSource = FeaturePackSettingsSection->Find("CompileSource"))
			{
				ConfigParameters.bCompileSource = FCString::ToBool(*CompileSource->GetValue());
			}
			if (FConfigValue* InstallMessage = FeaturePackSettingsSection->Find("InstallMessage"))
			{
				ConfigParameters.InstallMessage = InstallMessage->GetValue();
			}
		}
	}
}

UObject* UPackFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		FileType,
	const uint8*&		Buffer,
	const uint8*		BufferEnd,
	FFeedbackContext*	Warn
)
{ 
	FBufferReader PakReader((void*)Buffer, BufferEnd-Buffer, false);
	FPakFile PakFile(&PakReader);

	UObject* ReturnAsset = nullptr;

	if (PakFile.IsValid())
	{
		static FString ContentFolder(TEXT("/Content/"));
		FString ContentDestinationRoot = FPaths::ProjectContentDir();

		const int32 ChopIndex = PakFile.GetMountPoint().Find(ContentFolder);
		if (ChopIndex != INDEX_NONE)
		{
			ContentDestinationRoot /= PakFile.GetMountPoint().RightChop(ChopIndex + ContentFolder.Len());
		}

		TArray<uint8> CopyBuffer;
		TArray<uint8> PersistentCompressionBuffer;
		CopyBuffer.AddUninitialized(8 * 1024 * 1024); // 8MB buffer for extracting
		int32 ErrorCount = 0;
		int32 FileCount = 0;

		FModuleContextInfo SourceModuleInfo;
		PackFactoryHelper::FPackConfigParameters ConfigParameters;

		TArray<FString> WrittenFiles;
		TArray<FString> WrittenSourceFiles;

		// Process the config files and identify if we have source files
		for (FPakFile::FFileIterator It(PakFile); It; ++It, ++FileCount)
		{
			if (It.Filename().StartsWith(TEXT("Config/")) || It.Filename().Contains(TEXT("/Config/")))
			{
				const FPakEntry& Entry = It.Info();
				PakReader.Seek(Entry.Offset);
				FPakEntry EntryInfo;
				EntryInfo.Serialize(PakReader, PakFile.GetInfo().Version);

				if (EntryInfo == Entry)
				{
					FString ConfigString;
					PackFactoryHelper::ExtractFileToString(Entry, PakReader, CopyBuffer, PersistentCompressionBuffer, ConfigString);
					PackFactoryHelper::ProcessPackConfig(ConfigString, ConfigParameters);
				}
				else
				{
					UE_LOG(LogPackFactory, Error, TEXT("Serialized hash mismatch for \"%s\"."), *It.Filename());
					ErrorCount++;
				}
			}
			else if (!ConfigParameters.bContainsSource && (It.Filename().StartsWith(TEXT("Source/")) || It.Filename().Contains(TEXT("/Source/"))))
			{
				ConfigParameters.bContainsSource = true;
			}
		}

		bool bProjectHadSourceFiles = false;

		// If we have source files, set up the project files if necessary and the game name redirects for blueprints saved with class
		// references to the module name from the source template
		if (ConfigParameters.bContainsSource)
		{
			FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));
			bProjectHadSourceFiles = GameProjectModule.Get().ProjectHasCodeFiles();

			if (!bProjectHadSourceFiles)
			{
				TArray<FString> StartupModuleNames;
				TArray<FString> CreatedFiles;
				FText OutFailReason;
				if ( GameProjectModule.Get().GenerateBasicSourceCode(CreatedFiles, OutFailReason) )
				{
					WrittenFiles.Append(CreatedFiles);
				}
				else
				{
					UE_LOG(LogPackFactory, Error, TEXT("Unable to create basic source code: '%s'"), *OutFailReason.ToString());
				}
			}

			for (const FModuleContextInfo& ModuleInfo : GameProjectModule.Get().GetCurrentProjectModules())
			{
				// Pick the module to insert the code in.  For now always pick the first Runtime module
				if (ModuleInfo.ModuleType == EHostType::Runtime)
				{
					SourceModuleInfo = ModuleInfo;

					// Setup the game name redirect
					if (!ConfigParameters.GameName.IsEmpty())
					{
						const FString EngineIniFilename = FPaths::ConvertRelativePathToFull(GetDefault<UEngine>()->GetDefaultConfigFilename());

						if (ISourceControlModule::Get().IsEnabled())
						{
							FText ErrorMessage;

							if (!SourceControlHelpers::CheckoutOrMarkForAdd(EngineIniFilename, FText::FromString(EngineIniFilename), NULL, ErrorMessage))
							{
								UE_LOG(LogPackFactory, Error, TEXT("%s"), *ErrorMessage.ToString());
							}
						}

						const FString RedirectsSection(TEXT("/Script/Engine.Engine"));
						const FString LongOldGameName = FString::Printf(TEXT("/Script/%s"), *ConfigParameters.GameName);
						const FString LongNewGameName = FString::Printf(TEXT("/Script/%s"), *ModuleInfo.ModuleName);
						
						FConfigCacheIni Config(EConfigCacheType::Temporary);
						FConfigFile& NewFile = Config.Add(EngineIniFilename, FConfigFile());
						FConfigCacheIni::LoadLocalIniFile(NewFile, TEXT("DefaultEngine"), false);
						FConfigSection* PackageRedirects = Config.GetSectionPrivate(*RedirectsSection, true, false, EngineIniFilename);

						PackageRedirects->Add(TEXT("+ActiveGameNameRedirects"), FString::Printf(TEXT("(OldGameName=\"%s\",NewGameName=\"%s\")"), *LongOldGameName, *LongNewGameName));
						PackageRedirects->Add(TEXT("+ActiveGameNameRedirects"), FString::Printf(TEXT("(OldGameName=\"%s\",NewGameName=\"%s\")"), *ConfigParameters.GameName, *LongNewGameName));

						NewFile.UpdateSections(*EngineIniFilename, *RedirectsSection);

						FString FinalIniFileName;
						GConfig->LoadGlobalIniFile(FinalIniFileName, *RedirectsSection, NULL, true);

						FLinkerLoad::AddGameNameRedirect(*LongOldGameName, *LongNewGameName);
						FLinkerLoad::AddGameNameRedirect(*ConfigParameters.GameName, *LongNewGameName);
					}
					break;
				}
			}
		}

		// Process everything else and copy out to disk
		for (FPakFile::FFileIterator It(PakFile); It; ++It, ++FileCount)
		{
			// config files already handled
			if (It.Filename().StartsWith(TEXT("Config/")) || It.Filename().Contains(TEXT("/Config/")))
			{
				continue;
			}

			// Media and manifest files don't get written out as part of the install
			if (It.Filename().Contains(TEXT("manifest.json")) || It.Filename().StartsWith(TEXT("Media/")) || It.Filename().Contains(TEXT("/Media/")))
			{
				continue;
			}

			const FPakEntry& Entry = It.Info();
			PakReader.Seek(Entry.Offset);
			FPakEntry EntryInfo;
			EntryInfo.Serialize(PakReader, PakFile.GetInfo().Version);

			if (EntryInfo == Entry)
			{
				if (It.Filename().StartsWith(TEXT("Source/")) || It.Filename().Contains(TEXT("/Source/")))
				{
					FString DestFilename = It.Filename();
					if (DestFilename.StartsWith(TEXT("Source/")))
					{
						DestFilename = DestFilename.RightChop(7);
					}
					else 
					{
						const int32 SourceIndex = DestFilename.Find(TEXT("/Source/"));
						if (SourceIndex != INDEX_NONE)
						{
							DestFilename = DestFilename.RightChop(SourceIndex + 8);
						}
					}

					DestFilename = SourceModuleInfo.ModuleSourcePath / DestFilename;
					UE_LOG(LogPackFactory, Log, TEXT("%s (%ld) -> %s"), *It.Filename(), Entry.Size, *DestFilename);

					FString SourceContents;
					PackFactoryHelper::ExtractFileToString(Entry, PakReader, CopyBuffer, PersistentCompressionBuffer, SourceContents);

					FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));

					// Add the PCH for the project above the default pack include
					const FString StringToReplace = FString::Printf(TEXT("%s.h"),*ConfigParameters.GameName);
					const FString StringToReplaceWith = FString::Printf(TEXT("%s\"%s#include \"%s"),
						*GameProjectModule.Get().DetermineModuleIncludePath(SourceModuleInfo, DestFilename),
						LINE_TERMINATOR,
						*StringToReplace);

					if (FFileHelper::SaveStringToFile(SourceContents, *DestFilename))
					{
						WrittenFiles.Add(*DestFilename);
						WrittenSourceFiles.Add(*DestFilename);
					}
					else
					{
						UE_LOG(LogPackFactory, Error, TEXT("Unable to write file \"%s\"."), *DestFilename);
						++ErrorCount;
					}
				}
				else
				{
					FString DestFilename = It.Filename();
					if (DestFilename.StartsWith(TEXT("Content/")))
					{
						DestFilename = DestFilename.RightChop(8);
					}
					else
					{
						const int32 ContentIndex = DestFilename.Find(ContentFolder);
						if (ContentIndex != INDEX_NONE)
						{
							DestFilename = DestFilename.RightChop(ContentIndex + 9);
						}
					}
					DestFilename = ContentDestinationRoot / DestFilename;
					UE_LOG(LogPackFactory, Log, TEXT("%s (%ld) -> %s"), *It.Filename(), Entry.Size, *DestFilename);

					TUniquePtr<FArchive> FileHandle(IFileManager::Get().CreateFileWriter(*DestFilename));

					if (FileHandle)
					{
						PackFactoryHelper::ExtractFile(Entry, PakReader, CopyBuffer, PersistentCompressionBuffer, *FileHandle);
						WrittenFiles.Add(*DestFilename);
					}
					else
					{
						UE_LOG(LogPackFactory, Error, TEXT("Unable to create file \"%s\"."), *DestFilename);
						++ErrorCount;
					}
				}
			}
			else
			{
				UE_LOG(LogPackFactory, Error, TEXT("Serialized hash mismatch for \"%s\"."), *It.Filename());
				ErrorCount++;
			}
		}

		UE_LOG(LogPackFactory, Log, TEXT("Finished extracting %d files (including %d errors)."), FileCount, ErrorCount);

		if (ConfigParameters.AdditionalFilesToAdd.Num() > 0)
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

			for (const FString& FileToCopy : ConfigParameters.AdditionalFilesToAdd)
			{
				if (FileToCopy.StartsWith(TEXT("Source/")) || FileToCopy.Contains(TEXT("/Source/")))
				{
					FString DestFilename = FileToCopy;
					if (DestFilename.StartsWith(TEXT("Source/")))
					{
						DestFilename = DestFilename.RightChop(7);
					}
					else 
					{
						const int32 SourceIndex = DestFilename.Find(TEXT("/Source/"));
						if (SourceIndex != INDEX_NONE)
						{
							DestFilename = DestFilename.RightChop(SourceIndex + 8);
						}
					}
					DestFilename = SourceModuleInfo.ModuleSourcePath / DestFilename;

					FString DestDirectory = FPaths::GetPath(DestFilename);

					if (PlatformFile.CreateDirectoryTree(*DestDirectory))
					{
						FString SourceContents;
						if (FFileHelper::LoadFileToString(SourceContents, *FileToCopy))
						{
							FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));
							
							// Add the PCH for the project above the default pack include
							const FString StringToReplace = FString::Printf(TEXT("%s.h"),*ConfigParameters.GameName);
							const FString StringToReplaceWith = FString::Printf(TEXT("%s\"%s#include \"%s"),
								*GameProjectModule.Get().DetermineModuleIncludePath(SourceModuleInfo, DestFilename),
								LINE_TERMINATOR,
								*StringToReplace);

							SourceContents = SourceContents.Replace(*StringToReplace, *StringToReplaceWith, ESearchCase::CaseSensitive);

							if (FFileHelper::SaveStringToFile(SourceContents, *DestFilename))
							{
								WrittenFiles.Add(*DestFilename);
								WrittenSourceFiles.Add(*DestFilename);
							}
							else
							{
								UE_LOG(LogPackFactory, Error, TEXT("Unable to write file \"%s\"."), *DestFilename);
								++ErrorCount;
							}
						}
						else
						{
							UE_LOG(LogPackFactory, Error, TEXT("Unable to read file \"%s\"."), *FileToCopy);
						}
					}
				}
				else
				{
					FString DestFilename = FileToCopy;
					if (DestFilename.StartsWith(TEXT("Content/")))
					{
						DestFilename = DestFilename.RightChop(8);
					}
					else
					{
						const int32 ContentIndex = DestFilename.Find(ContentFolder);
						if (ContentIndex != INDEX_NONE)
						{
							DestFilename = DestFilename.RightChop(ContentIndex + 9);
						}
					}
					DestFilename = ContentDestinationRoot / DestFilename;

					FString DestDirectory = FPaths::GetPath(DestFilename);

					if (PlatformFile.CreateDirectoryTree(*DestDirectory))
					{
						if (PlatformFile.CopyFile(*DestFilename, *FileToCopy))
						{
							WrittenFiles.Add(DestFilename);
							UE_LOG(LogPackFactory, Log, TEXT("Copied \"%s\" to \"%s\""), *FileToCopy, *DestFilename);
						}
						else
						{
							UE_LOG(LogPackFactory, Error, TEXT("Unable to copy file \"%s\" to \"%s\"."), *FileToCopy, *DestFilename);
						}
					}
					else
					{
						UE_LOG(LogPackFactory, Error, TEXT("Unable to create directory \"%s\"."), *FileToCopy, *DestFilename);
					}
				}
			}
		}

		if (WrittenFiles.Num() > 0)
		{
			// If we wrote out source files, kick off the hot reload process
			if (WrittenSourceFiles.Num() > 0)
			{
				// Update the game projects before we attempt to build
				FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));
				FText FailReason, FailLog;
				if (!GameProjectModule.UpdateCodeProject(FailReason, FailLog))
				{
					SOutputLogDialog::Open(NSLOCTEXT("PackFactory", "CreateBinary", "Create binary"), FailReason, FailLog, FText::GetEmpty());
				}

				if (ConfigParameters.bCompileSource)
				{
					// Compile the new code, either using the in editor hot-reload (if an existing module), or as a brand new module (if no existing code)
					IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
					if (bProjectHadSourceFiles)
					{
						// We can only hot-reload via DoHotReloadFromEditor when we already had code in our project
						if (!HotReloadSupport.IsCurrentlyCompiling())
						{
							const bool bWaitForCompletion = true;
							HotReloadSupport.DoHotReloadFromEditor(bWaitForCompletion);
						}
					}
					else
					{
						const bool bReloadAfterCompiling = true;
						const bool bForceCodeProject = true;
						const bool bFailIfGeneratedCodeChanges = false;
						if (!HotReloadSupport.RecompileModule(FApp::GetProjectName(), bReloadAfterCompiling, *GWarn, bFailIfGeneratedCodeChanges, bForceCodeProject))
						{
							FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("PackFactory", "FailedToCompileNewGameModule", "Failed to compile newly created game module."));
						}
					}
				}

				// Ask about editing code where applicable
				if (FSlateApplication::Get().SupportsSourceAccess() )
				{
					// Code successfully added, notify the user and ask about opening the IDE now
					const FText Message = NSLOCTEXT("PackFactory", "CodeAdded", "Added source file(s). Would you like to edit the code now?");
					if ( FMessageDialog::Open(EAppMsgType::YesNo, Message) == EAppReturnType::Yes )
					{
						FSourceCodeNavigation::OpenSourceFiles(WrittenSourceFiles);
					}
				}
			}
			
			// Find an asset to return (It will be marked as dirty)
			for (const FString& Filename : WrittenFiles)
			{
				static const FString AssetExtension(TEXT(".uasset"));
				if (Filename.EndsWith(AssetExtension))
				{
					FString GameFileName = Filename;
					if (FPaths::MakePathRelativeTo(GameFileName, *FPaths::ProjectContentDir()))
					{
						int32 SlashIndex = INDEX_NONE;
						GameFileName = FString(TEXT("/Game/")) / GameFileName.LeftChop(AssetExtension.Len());
						if (GameFileName.FindLastChar(TEXT('/'), SlashIndex))
						{
							const FString AssetName = GameFileName.RightChop(SlashIndex + 1);
							ReturnAsset = LoadObject<UObject>(nullptr, *(GameFileName + TEXT(".") + AssetName));
							if (ReturnAsset)
							{
								break;
							}
						}
					}
				}
			}

			// If source control is enabled mark all the added files for checkout/add
			if (ISourceControlModule::Get().IsEnabled() && GetDefault<UEditorLoadingSavingSettings>()->bSCCAutoAddNewFiles)
			{
				for (const FString& Filename : WrittenFiles)
				{
					FText ErrorMessage;
					if (!SourceControlHelpers::CheckoutOrMarkForAdd(Filename, FText::FromString(Filename), NULL, ErrorMessage))
					{
						UE_LOG(LogPackFactory, Error, TEXT("%s"), *ErrorMessage.ToString());
					}
				}
			}
		}

		if (!ConfigParameters.InstallMessage.IsEmpty())
		{
			FMessageLog("AssetTools").Warning(FText::FromString(ConfigParameters.InstallMessage));
			FMessageLog("AssetTools").Open();
		}
	}
	else
	{
		UE_LOG(LogPackFactory, Warning, TEXT("Invalid pak file."));
	}

	return ReturnAsset;
}
