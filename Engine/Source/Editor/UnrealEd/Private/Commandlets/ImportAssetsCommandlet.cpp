// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/ImportAssetsCommandlet.h"
#include "AutomatedAssetImportData.h"
#include "Modules/ModuleManager.h"
#include "Factories/Factory.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Factories/ImportSettings.h"
#include "ISourceControlModule.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Misc/FeedbackContext.h"
#include "HAL/PlatformFilemanager.h"

static void PrintUsage()
{
	UE_LOG(LogAutomatedImport, Display, TEXT("LogAutomatedImport Usage: LogAutomatedImport {arglist}"));
	UE_LOG(LogAutomatedImport, Display, TEXT("Arglist:"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-help or -?"));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tDisplays this help"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-source=\"path\""));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tThe source file to import.  This must be specified when importing a single asset\n[IGNORED when using -importparams]"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-dest=\"path\""));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tThe destination path in the project's content directory to import to.\nThis must be specified when importing a single asset\n[IGNORED when using -importparams]"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-factory={factory class name}"));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tForces the asset to be opened with a specific UFactory class type.  If not specified import type will be auto detected.\n[IGNORED when using -importparams]"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-importsettings=\"path to import settings json file\""));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tPath to a json file that has asset import parameters when importing multiple files. If this argument is used all other import arguments are ignored as they are specified in the json file"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-replaceexisting"));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tWhether or not to replace existing assets when importing"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-nosourcecontrol"));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tDisables source control.  Prevents checking out, adding files, and submitting files"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-submitdesc"));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tSubmit description/comment to use checking in to source control.  If this is empty no files will be submitted"));

	UE_LOG(LogAutomatedImport, Display, TEXT("-skipreadonly"));
	UE_LOG(LogAutomatedImport, Display, TEXT("\tIf an asset cannot be saved because it is read only, the commandlet will not clear the read only flag and will not save the file"));
}

UImportAssetsCommandlet::UImportAssetsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UImportAssetsCommandlet::ParseParams(const FString& InParams)
{
	TArray<FString> Tokens;
	TArray<FString> Params;
	TMap<FString, FString> ParamVals;

	ParseCommandLine(*InParams, Tokens, Params, ParamVals);

	const bool bEnoughParams = ParamVals.Num() > 1;
	if( Params.Contains(TEXT("?")) || Params.Contains(TEXT("help") ) )
	{
		bShowHelp = true;
	}

	bAllowSourceControl = !Params.Contains(TEXT("nosourcecontrol"));

	GlobalImportData = NewObject<UAutomatedAssetImportData>(this);

	GlobalImportData->bSkipReadOnly = Params.Contains(TEXT("skipreadonly"));

	FString SourcePathParam = ParamVals.FindRef(TEXT("source"));
	if(!SourcePathParam.IsEmpty())
	{
		GlobalImportData->Filenames.Add(SourcePathParam);
	}
	
	GlobalImportData->DestinationPath = ParamVals.FindRef(TEXT("dest"));

	GlobalImportData->FactoryName = ParamVals.FindRef(TEXT("factoryname"));

	GlobalImportData->bReplaceExisting = Params.Contains(TEXT("replaceexisting"));

	GlobalImportData->LevelToLoad = ParamVals.FindRef(TEXT("level"));

	if (!GlobalImportData->LevelToLoad.IsEmpty())
	{
		FText FailReason;
		if (!FPackageName::IsValidLongPackageName(GlobalImportData->LevelToLoad, false, &FailReason))
		{
			UE_LOG(LogAutomatedImport, Error, TEXT("Invalid level specified: %s"), *FailReason.ToString());
		}
	}

	ImportSettingsPath = ParamVals.FindRef(TEXT("importsettings"));

	GlobalImportData->Initialize(nullptr);

	if(ImportSettingsPath.IsEmpty() && (GlobalImportData->Filenames.Num() == 0 || GlobalImportData->DestinationPath.IsEmpty()))
	{
		UE_LOG(LogAutomatedImport, Error, TEXT("Invalid Arguments.  Missing, Source (-source), Destination (-dest), or Import settings file (-importsettings)"));
	}

	return bEnoughParams;
}

bool UImportAssetsCommandlet::ParseImportSettings(const FString& InImportSettingsFile)
{
	bool bInvalidParse = false;
	bool bSuccess = false;
	FString JsonString;
	if(FFileHelper::LoadFileToString(JsonString, *InImportSettingsFile))
	{
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
		TSharedPtr<FJsonObject> RootObject;
		if(FJsonSerializer::Deserialize(JsonReader, RootObject) && RootObject.IsValid())
		{
			const TArray< TSharedPtr<FJsonValue> > ImportGroupsJsonArray = RootObject->GetArrayField(TEXT("ImportGroups"));
			for(const TSharedPtr<FJsonValue>& ImportGroupsJson : ImportGroupsJsonArray)
			{
				const TSharedPtr<FJsonObject> ImportGroupsJsonObject = ImportGroupsJson->AsObject();
				if(ImportGroupsJsonObject.IsValid())
				{
					// All import data is based off of the global data defaults
					UAutomatedAssetImportData* Data = DuplicateObject<UAutomatedAssetImportData>(GlobalImportData, this);
					
					// Parse data from the json object
					if(FJsonObjectConverter::JsonObjectToUStruct(ImportGroupsJsonObject.ToSharedRef(), UAutomatedAssetImportData::StaticClass(), Data, 0, 0 ))
					{
						Data->Initialize(ImportGroupsJsonObject);
						if(Data->IsValid())
						{
							ImportDataList.Add(Data);
						}
					}
					else
					{
						bInvalidParse = true;
					}
					
				}
				else
				{
					bInvalidParse = true;
				}
			}
		}
		else
		{
			UE_LOG(LogAutomatedImport, Error, TEXT("Json settings file was found but was invalid: %s"), *JsonReader->GetErrorMessage());
		}
	}
	else
	{
		UE_LOG(LogAutomatedImport, Error, TEXT("Import settings file %s could not be found"), *InImportSettingsFile);
	}

	return bSuccess;
}

static bool SavePackage(UPackage* Package, const FString& PackageFilename)
{
	return GEditor->SavePackage(Package, nullptr, RF_Standalone, *PackageFilename, GWarn);
}

bool UImportAssetsCommandlet::ImportAndSave(const TArray<UAutomatedAssetImportData*>& AssetImportList)
{
	bool bImportAndSaveSucceeded = true;

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	for(const UAutomatedAssetImportData* ImportData : AssetImportList)
	{
		UE_LOG(LogAutomatedImport, Log, TEXT("Importing group %s"), *ImportData->GetDisplayName() );

		UFactory* Factory = ImportData->Factory;
		const TSharedPtr<FJsonObject>* ImportSettingsJsonObject = nullptr;
		if(ImportData->ImportGroupJsonData.IsValid())
		{
			ImportData->ImportGroupJsonData->TryGetObjectField(TEXT("ImportSettings"), ImportSettingsJsonObject);
		}

		if(Factory != nullptr && ImportSettingsJsonObject)
		{
			IImportSettingsParser* ImportSettings = Factory->GetImportSettingsParser();
			if(ImportSettings)
			{
				ImportSettings->ParseFromJson(ImportSettingsJsonObject->ToSharedRef());
			}
		}
		else if(Factory == nullptr && ImportSettingsJsonObject)
		{
			UE_LOG(LogAutomatedImport, Warning, TEXT("A vaild factory name must be specfied in order to specify settings"));
		}

		// Load a level if specified
		bImportAndSaveSucceeded = LoadLevel(ImportData->LevelToLoad);

		// Clear dirty packages that were created as a result of loading the level.  We do not want to save these
		ClearDirtyPackages();

		TArray<UObject*> ImportedAssets = AssetToolsModule.Get().ImportAssetsAutomated(ImportData);
		if(ImportedAssets.Num() > 0 && bImportAndSaveSucceeded)
		{
			TArray<UPackage*> DirtyPackages;

			TArray<FSourceControlStateRef> PackageStates;

			FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);
			FEditorFileUtils::GetDirtyWorldPackages(DirtyPackages);

			bool bUseSourceControl = bHasSourceControl && SourceControlProvider.IsAvailable();
			if(bUseSourceControl)
			{
				SourceControlProvider.GetState(DirtyPackages, PackageStates, EStateCacheUsage::ForceUpdate);
			}

			for(int32 PackageIndex = 0; PackageIndex < DirtyPackages.Num(); ++PackageIndex)
			{
				UPackage* PackageToSave = DirtyPackages[PackageIndex];

				FString PackageFilename = SourceControlHelpers::PackageFilename(PackageToSave);

				bool bShouldAttemptToSave = false;
				bool bShouldAttemptToAdd = false;
				if(bUseSourceControl)
				{
					FSourceControlStateRef PackageSCState = PackageStates[PackageIndex];

					bool bPackageCanBeCheckedOut = false;
					if(PackageSCState->IsCheckedOutOther())
					{
						// Cannot checkout, file is already checked out
						UE_LOG(LogAutomatedImport, Error, TEXT("%s is already checked out by someone else, can not submit!"), *PackageFilename);
						bImportAndSaveSucceeded = false;
					}
					else if(!PackageSCState->IsCurrent())
					{
						// Cannot checkout, file is not at head revision
						UE_LOG(LogAutomatedImport, Error, TEXT("%s is not at the head revision and cannot be checked out"), *PackageFilename);
						bImportAndSaveSucceeded = false;
					}
					else if(PackageSCState->CanCheckout())
					{
						const bool bWasCheckedOut = SourceControlHelpers::CheckOutFile(PackageFilename);
						bShouldAttemptToSave = bWasCheckedOut;
						if(!bWasCheckedOut)
						{
							UE_LOG(LogAutomatedImport, Error, TEXT("%s could not be checked out"), *PackageFilename);
							bImportAndSaveSucceeded = false;
						}
					}
					else
					{
						// package was not checked out by another user and is at the current head revision and could not be checked out
						// this means it should be added after save because it doesnt exist
						bShouldAttemptToSave = true;
						bShouldAttemptToAdd = true;
					}
				}
				else
				{
					bool bIsReadOnly = IFileManager::Get().IsReadOnly(*PackageFilename);
					if(bIsReadOnly && ImportData->bSkipReadOnly)
					{
						bShouldAttemptToSave = false;
						if(bIsReadOnly)
						{
							UE_LOG(LogAutomatedImport, Error, TEXT("%s is read only and -skipreadonly was specified.  Will not save"), *PackageFilename);
							bImportAndSaveSucceeded = false;
						}
					}
					else if(bIsReadOnly)
					{
						bShouldAttemptToSave = FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*PackageFilename, false);
						if(!bShouldAttemptToSave)
						{
							UE_LOG(LogAutomatedImport, Error, TEXT("%s is read only and could not be made writable.  Will not save"), *PackageFilename);
							bImportAndSaveSucceeded = false;
						}
					}
					else
					{
						bShouldAttemptToSave = true;
					}
				}

				if(bShouldAttemptToSave)
				{
					SavePackage(PackageToSave, PackageFilename);

					if(bShouldAttemptToAdd)
					{
						const bool bWasAdded = SourceControlHelpers::MarkFileForAdd(PackageFilename);
						if(!bWasAdded)
						{
							UE_LOG(LogAutomatedImport, Error, TEXT("%s could not be added to source control"), *PackageFilename);
							bImportAndSaveSucceeded = false;
						}
					}
				}
			}
		}
		else
		{
			bImportAndSaveSucceeded = false;
			UE_LOG(LogAutomatedImport, Error, TEXT("Failed to import all assets in group %s"), *ImportData->GetDisplayName());
		}
	}

	return bImportAndSaveSucceeded;
}

bool UImportAssetsCommandlet::LoadLevel(const FString& LevelToLoad)
{
	bool bResult = false;

	if (!LevelToLoad.IsEmpty())
	{
		UE_LOG(LogAutomatedImport, Log, TEXT("Loading Map %s"), *LevelToLoad);

		FString Filename;
		if (FPackageName::TryConvertLongPackageNameToFilename(LevelToLoad, Filename))
		{
			UPackage* Package = LoadPackage(NULL, *Filename, 0);

			UWorld* World = UWorld::FindWorldInPackage(Package);
			if (World)
			{
				// Clean up any previous world.  The world should have already been saved
				UWorld* ExistingWorld = GEditor->GetEditorWorldContext().World();

				GEngine->DestroyWorldContext(ExistingWorld);
				ExistingWorld->DestroyWorld(true, World);

				GWorld = World;

				World->WorldType = EWorldType::Editor;

				FWorldContext& WorldContext = GEngine->CreateNewWorldContext(World->WorldType);
				WorldContext.SetCurrentWorld(World);

				// add the world to the root set so that the garbage collection to delete replaced actors doesn't garbage collect the whole world
				World->AddToRoot();

				// initialize the levels in the world
				World->InitWorld(UWorld::InitializationValues().AllowAudioPlayback(false));
				World->GetWorldSettings()->PostEditChange();
				World->UpdateWorldComponents(true, false);

				bResult = true;
			}
		}
	}
	else
	{
		// a map was not specified, ignore
		bResult = true;
	}

	if (!bResult)
	{
		UE_LOG(LogAutomatedImport, Error, TEXT("Could not find or load level %s"), *LevelToLoad);
	}

	return bResult;

}

void UImportAssetsCommandlet::ClearDirtyPackages()
{
	TArray<UPackage*> DirtyPackages;
	FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);
	FEditorFileUtils::GetDirtyWorldPackages(DirtyPackages);

	for(UPackage* Package : DirtyPackages)
	{
		Package->SetDirtyFlag(false);
	}
}

int32 UImportAssetsCommandlet::Main(const FString& InParams)
{
	bool bEnoughParams = ParseParams(InParams);

	int32 Result = 0;

	if(!bEnoughParams || bShowHelp)
	{
		PrintUsage();
	}
	else
	{
		// Hack:  A huge amount of packages are marked dirty on startup.  This is normally prevented in editor but commandlets have special powers.  
		// We only want to save assets that were created or modified at import time so clear all existing ones now
		ClearDirtyPackages();

		if(bAllowSourceControl)
		{
			ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
			SourceControlProvider.Init();

			bHasSourceControl = SourceControlProvider.IsEnabled();
			if(!bHasSourceControl)
			{
				UE_LOG(LogAutomatedImport, Error, TEXT("Could not connect to source control!"))
			}
		}
		else
		{
			bHasSourceControl = false;
		}


		if(!ImportSettingsPath.IsEmpty())
		{
			// Use settings file for importing assets
			ParseImportSettings(ImportSettingsPath);
		}
		else if(GlobalImportData->IsValid())
		{
			// Use single import path
			ImportDataList.Add(GlobalImportData);
		}

		if(!ImportAndSave(ImportDataList))
		{
			UE_LOG(LogAutomatedImport, Error, TEXT("Could not import all groups"));
		}
		else
		{
			Result = 0;
		}
		
	}
	return Result;
}
