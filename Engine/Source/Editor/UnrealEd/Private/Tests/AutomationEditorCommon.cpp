// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Serialization/FindReferencersArchive.h"
#include "Editor/UnrealEdEngine.h"
#include "Factories/Factory.h"
#include "Factories/TextureFactory.h"
#include "Engine/StaticMesh.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/StaticMeshActor.h"
#include "LevelEditorViewport.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "FileHelpers.h"
#include "UnrealEdGlobals.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"
#include "Tests/AutomationCommon.h"
#include "Toolkits/AssetEditorManager.h"
#include "LevelEditor.h"
#include "Interfaces/IMainFrameModule.h"
#include "ShaderCompiler.h"
#include "AssetSelection.h"
#include "ITargetDeviceProxy.h"
#include "ITargetDeviceServicesModule.h"
#include "ILauncherWorker.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "LightingBuildOptions.h"


#define COOK_TIMEOUT 3600

DEFINE_LOG_CATEGORY_STATIC(LogAutomationEditorCommon, Log, All);


UWorld* FAutomationEditorCommonUtils::CreateNewMap()
{
	// Change out of Matinee when opening new map, so we avoid editing data in the old one.
	if ( GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_InterpEdit) )
	{
		GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_InterpEdit);
	}

	// Also change out of Landscape mode to ensure all references are cleared.
	if ( GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Landscape) )
	{
		GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_Landscape);
	}

	// Also change out of Foliage mode to ensure all references are cleared.
	if ( GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Foliage) )
	{
		GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_Foliage);
	}

	// Change out of mesh paint mode when opening a new map.
	if ( GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_MeshPaint) )
	{
		GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_MeshPaint);
	}

	return GEditor->NewMap();
}

/**
* Imports an object using a given factory
*
* @param ImportFactory - The factory to use to import the object
* @param ObjectName - The name of the object to create
* @param PackagePath - The full path of the package file to create
* @param ImportPath - The path to the object to import
*/
UObject* FAutomationEditorCommonUtils::ImportAssetUsingFactory(UFactory* ImportFactory, const FString& ObjectName, const FString& PackagePath, const FString& ImportPath)
{
	UObject* ImportedAsset = NULL;

	UPackage* Pkg = CreatePackage(NULL, *PackagePath);
	if (Pkg)
	{
		// Make sure the destination package is loaded
		Pkg->FullyLoad();

		UClass* ImportAssetType = ImportFactory->ResolveSupportedClass();
		bool bDummy = false;

		//If we are a texture factory suppress some warning dialog that we don't want
		if (ImportFactory->IsA(UTextureFactory::StaticClass()))
		{
			UTextureFactory::SuppressImportOverwriteDialog();
		}

		bool OutCanceled = false;
		ImportedAsset = ImportFactory->ImportObject(ImportAssetType, Pkg, FName(*ObjectName), RF_Public | RF_Standalone, ImportPath, nullptr, OutCanceled);

		if (ImportedAsset != nullptr)
		{
			UE_LOG(LogAutomationEditorCommon, Display, TEXT("Imported %s"), *ImportPath);
		}
		else if (OutCanceled)
		{
			UE_LOG(LogAutomationEditorCommon, Display, TEXT("Canceled import of %s"), *ImportPath);
		}
		else
		{
			UE_LOG(LogAutomationEditorCommon, Error, TEXT("Failed to import asset using factory %s!"), *ImportFactory->GetName());
		}
	}
	else
	{
		UE_LOG(LogAutomationEditorCommon, Error, TEXT("Failed to create a package!"));
	}

	return ImportedAsset;
}

/**
* Nulls out references to a given object
*
* @param InObject - Object to null references to
*/
void FAutomationEditorCommonUtils::NullReferencesToObject(UObject* InObject)
{
	TArray<UObject*> ReplaceableObjects;
	TMap<UObject*, UObject*> ReplacementMap;
	ReplacementMap.Add(InObject, NULL);
	ReplacementMap.GenerateKeyArray(ReplaceableObjects);

	// Find all the properties (and their corresponding objects) that refer to any of the objects to be replaced
	TMap< UObject*, TArray<UProperty*> > ReferencingPropertiesMap;
	for (FObjectIterator ObjIter; ObjIter; ++ObjIter)
	{
		UObject* CurObject = *ObjIter;

		// Find the referencers of the objects to be replaced
		FFindReferencersArchive FindRefsArchive(CurObject, ReplaceableObjects);

		// Inform the object referencing any of the objects to be replaced about the properties that are being forcefully
		// changed, and store both the object doing the referencing as well as the properties that were changed in a map (so that
		// we can correctly call PostEditChange later)
		TMap<UObject*, int32> CurNumReferencesMap;
		TMultiMap<UObject*, UProperty*> CurReferencingPropertiesMMap;
		if (FindRefsArchive.GetReferenceCounts(CurNumReferencesMap, CurReferencingPropertiesMMap) > 0)
		{
			TArray<UProperty*> CurReferencedProperties;
			CurReferencingPropertiesMMap.GenerateValueArray(CurReferencedProperties);
			ReferencingPropertiesMap.Add(CurObject, CurReferencedProperties);
			for (TArray<UProperty*>::TConstIterator RefPropIter(CurReferencedProperties); RefPropIter; ++RefPropIter)
			{
				CurObject->PreEditChange(*RefPropIter);
			}
		}

	}

	// Iterate over the map of referencing objects/changed properties, forcefully replacing the references and then
	// alerting the referencing objects the change has completed via PostEditChange
	int32 NumObjsReplaced = 0;
	for (TMap< UObject*, TArray<UProperty*> >::TConstIterator MapIter(ReferencingPropertiesMap); MapIter; ++MapIter)
	{
		++NumObjsReplaced;

		UObject* CurReplaceObj = MapIter.Key();
		const TArray<UProperty*>& RefPropArray = MapIter.Value();

		FArchiveReplaceObjectRef<UObject> ReplaceAr(CurReplaceObj, ReplacementMap, false, true, false);

		for (TArray<UProperty*>::TConstIterator RefPropIter(RefPropArray); RefPropIter; ++RefPropIter)
		{
			FPropertyChangedEvent PropertyEvent(*RefPropIter);
			CurReplaceObj->PostEditChangeProperty(PropertyEvent);
		}

		if (!CurReplaceObj->HasAnyFlags(RF_Transient) && CurReplaceObj->GetOutermost() != GetTransientPackage())
		{
			if (!CurReplaceObj->RootPackageHasAnyFlags(PKG_CompiledIn))
			{
				CurReplaceObj->MarkPackageDirty();
			}
		}
	}
}

/**
* gets a factory class based off an asset file extension
*
* @param AssetExtension - The file extension to use to find a supporting UFactory
*/
UClass* FAutomationEditorCommonUtils::GetFactoryClassForType(const FString& AssetExtension)
{
	// First instantiate one factory for each file extension encountered that supports the extension
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		if ((*ClassIt)->IsChildOf(UFactory::StaticClass()) && !((*ClassIt)->HasAnyClassFlags(CLASS_Abstract)))
		{
			UFactory* Factory = Cast<UFactory>((*ClassIt)->GetDefaultObject());
			if (Factory->bEditorImport)
			{
				TArray<FString> FactoryExtensions;
				Factory->GetSupportedFileExtensions(FactoryExtensions);

				// Case insensitive string compare with supported formats of this factory
				if (FactoryExtensions.Contains(AssetExtension))
				{
					return *ClassIt;
				}
			}
		}
	}

	return NULL;
}

/**
* Applies settings to an object by finding UProperties by name and calling ImportText
*
* @param InObject - The object to search for matching properties
* @param PropertyChain - The list UProperty names recursively to search through
* @param Value - The value to import on the found property
*/
void FAutomationEditorCommonUtils::ApplyCustomFactorySetting(UObject* InObject, TArray<FString>& PropertyChain, const FString& Value)
{
	const FString PropertyName = PropertyChain[0];
	PropertyChain.RemoveAt(0);

	UProperty* TargetProperty = FindField<UProperty>(InObject->GetClass(), *PropertyName);
	if (TargetProperty)
	{
		if (PropertyChain.Num() == 0)
		{
			TargetProperty->ImportText(*Value, TargetProperty->ContainerPtrToValuePtr<uint8>(InObject), 0, InObject);
		}
		else
		{
			UStructProperty* StructProperty = Cast<UStructProperty>(TargetProperty);
			UObjectProperty* ObjectProperty = Cast<UObjectProperty>(TargetProperty);

			UObject* SubObject = NULL;
			bool bValidPropertyType = true;

			if (StructProperty)
			{
				SubObject = StructProperty->Struct;
			}
			else if (ObjectProperty)
			{
				SubObject = ObjectProperty->GetObjectPropertyValue(ObjectProperty->ContainerPtrToValuePtr<UObject>(InObject));
			}
			else
			{
				//Unknown nested object type
				bValidPropertyType = false;
				UE_LOG(LogAutomationEditorCommon, Error, TEXT("ERROR: Unknown nested object type for property: %s"), *PropertyName);
			}

			if (SubObject)
			{
				ApplyCustomFactorySetting(SubObject, PropertyChain, Value);
			}
			else if (bValidPropertyType)
			{
				UE_LOG(LogAutomationEditorCommon, Error, TEXT("Error accessing null property: %s"), *PropertyName);
			}
		}
	}
	else
	{
		UE_LOG(LogAutomationEditorCommon, Error, TEXT("ERROR: Could not find factory property: %s"), *PropertyName);
	}
}

/**
* Applies the custom factory settings
*
* @param InFactory - The factory to apply custom settings to
* @param FactorySettings - An array of custom settings to apply to the factory
*/
void FAutomationEditorCommonUtils::ApplyCustomFactorySettings(UFactory* InFactory, const TArray<FImportFactorySettingValues>& FactorySettings)
{
	bool bCallConfigureProperties = true;

	for (int32 i = 0; i < FactorySettings.Num(); ++i)
	{
		if (FactorySettings[i].SettingName.Len() > 0 && FactorySettings[i].Value.Len() > 0)
		{
			//Check if we are setting an FBX import type override.  If we are, we don't want to call ConfigureProperties because that enables bDetectImportTypeOnImport
			if (FactorySettings[i].SettingName.Contains(TEXT("MeshTypeToImport")))
			{
				bCallConfigureProperties = false;
			}

			TArray<FString> PropertyChain;
			FactorySettings[i].SettingName.ParseIntoArray(PropertyChain, TEXT("."), false);
			ApplyCustomFactorySetting(InFactory, PropertyChain, FactorySettings[i].Value);
		}
	}

	if (bCallConfigureProperties)
	{
		InFactory->ConfigureProperties();
	}
}

/**
* Writes a number to a text file.
* @param InTestName is the folder that has the same name as the test. (For Example: "Performance").
* @param InItemBeingTested is the name for the thing that is being tested. (For Example: "MapName").
* @param InFileName is the name of the file with an extension
* @param InNumberToBeWritten is the float number that is expected to be written to the file.
* @param Delimiter is the delimiter to be used. TEXT(",")
*/
void FAutomationEditorCommonUtils::WriteToTextFile(const FString& InTestName, const FString& InTestItem, const FString& InFileName, const float& InEntry, const FString& Delimiter)
{
	//Performance file locations and setups.
	FString FileSaveLocation = FPaths::Combine(*FPaths::AutomationLogDir(), *InTestName, *InTestItem, *InFileName);

	if (FPaths::FileExists(FileSaveLocation))
	{
		//The text files existing content.
		FString TextFileContents;

		//Write to the text file the combined contents from the text file with the number to write.
		FFileHelper::LoadFileToString(TextFileContents, *FileSaveLocation);
		FString FileSetup = TextFileContents + Delimiter + FString::SanitizeFloat(InEntry);
		FFileHelper::SaveStringToFile(FileSetup, *FileSaveLocation);
		return;
	}

	FFileHelper::SaveStringToFile(FString::SanitizeFloat(InEntry), *FileSaveLocation);
}

/**
* Returns the sum of the numbers available in an array of float.
* @param InFloatArray is the name of the array intended to be used.
* @param bisAveragedInstead will return the average of the available numbers instead of the sum.
*/
float FAutomationEditorCommonUtils::TotalFromFloatArray(const TArray<float>& InFloatArray, bool bIsAveragedInstead)
{
	//Total Value holds the sum of all the numbers available in the array.
	float TotalValue = 0;

	//Get the sum of the array.
	for (int32 I = 0; I < InFloatArray.Num(); ++I)
	{
		TotalValue += InFloatArray[I];
	}

	//If bAverageInstead equals true then only the average is returned.
	if (bIsAveragedInstead)
	{
		UE_LOG(LogEditorAutomationTests, VeryVerbose, TEXT("Average value of the Array is %f"), (TotalValue / InFloatArray.Num()));
		return (TotalValue / InFloatArray.Num());
	}

	UE_LOG(LogEditorAutomationTests, VeryVerbose, TEXT("Total Value of the Array is %f"), TotalValue);
	return TotalValue;
}

/**
* Returns the largest value from an array of float numbers.
* @param InFloatArray is the name of the array intended to be used.
*/
float FAutomationEditorCommonUtils::LargestValueInFloatArray(const TArray<float>& InFloatArray)
{
	//Total Value holds the sum of all the numbers available in the array.
	float LargestValue = 0;

	//Find the largest value
	for (int32 I = 0; I < InFloatArray.Num(); ++I)
	{
		if (LargestValue < InFloatArray[I])
		{
			LargestValue = InFloatArray[I];
		}
	}
	UE_LOG(LogEditorAutomationTests, VeryVerbose, TEXT("The Largest value of the array is %f"), LargestValue);
	return LargestValue;
}

/**
* Returns the contents of a text file as an array of FString.
* @param InFileLocation -  is the location of the file.
* @param OutArray - The name of the array where the 
*/
void FAutomationEditorCommonUtils::CreateArrayFromFile(const FString& InFileLocation, TArray<FString>& OutArray)
{
	FString RawData;

	if (FPaths::FileExists(*InFileLocation))
	{
		UE_LOG(LogEditorAutomationTests, VeryVerbose, TEXT("Loading and parsing the data from '%s' into an array."), *InFileLocation);
		FFileHelper::LoadFileToString(RawData, *InFileLocation);
		RawData.ParseIntoArray(OutArray, TEXT(","), false);
	}

	UE_LOG(LogEditorAutomationTests, Warning, TEXT("Unable to create an array.  '%s' does not exist."), *InFileLocation);
	RawData = TEXT("0");
	OutArray.Add(RawData);
}

/**
* Returns true if the archive/file can be written to otherwise false..
* @param InFilePath - is the location of the file.
* @param InArchiveName - is the name of the archive to be used.
*/
bool FAutomationEditorCommonUtils::IsArchiveWriteable(const FString& InFilePath, const FArchive* InArchiveName)
{
	if (!InArchiveName)
	{
		UE_LOG(LogEditorAutomationTests, Error, TEXT("Failed to write to the csv file: %s"), *FPaths::ConvertRelativePathToFull(InFilePath));
		return false;
	}
	return true;
}


void FAutomationEditorCommonUtils::GetLaunchOnDeviceID(FString& OutDeviceID, const FString& InMapName)
{
	UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
	check(AutomationTestSettings);

	OutDeviceID = "None";

	FString LaunchOnDeviceId;
	for (auto LaunchIter = AutomationTestSettings->LaunchOnSettings.CreateConstIterator(); LaunchIter; LaunchIter++)
	{
		FString LaunchOnSettings = LaunchIter->DeviceID;
		FString LaunchOnMap = FPaths::GetBaseFilename(LaunchIter->LaunchOnTestmap.FilePath);
		if (LaunchOnMap.Equals(InMapName))
		{
			// shared devices section
			ITargetDeviceServicesModule* TargetDeviceServicesModule = static_cast<ITargetDeviceServicesModule*>(FModuleManager::Get().LoadModule(TEXT("TargetDeviceServices")));
			// for each platform...
			TArray<TSharedPtr<ITargetDeviceProxy>> DeviceProxies;
			TargetDeviceServicesModule->GetDeviceProxyManager()->GetProxies(FName(*LaunchOnSettings), true, DeviceProxies);
			// for each proxy...
			for (auto DeviceProxyIt = DeviceProxies.CreateIterator(); DeviceProxyIt; ++DeviceProxyIt)
			{
				TSharedPtr<ITargetDeviceProxy> DeviceProxy = *DeviceProxyIt;
				if (DeviceProxy->IsConnected())
				{
					OutDeviceID = DeviceProxy->GetTargetDeviceId((FName)*LaunchOnSettings);
					break;
				}
			}
		}
	}
}

void FAutomationEditorCommonUtils::GetLaunchOnDeviceID(FString& OutDeviceID, const FString& InMapName, const FString& InDeviceName)
{
	UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
	check(AutomationTestSettings);

	//Output device name will default to "None".
	OutDeviceID = "None";

	// shared devices section
	ITargetDeviceServicesModule* TargetDeviceServicesModule = static_cast<ITargetDeviceServicesModule*>(FModuleManager::Get().LoadModule(TEXT("TargetDeviceServices")));
	// for each platform...
	TArray<TSharedPtr<ITargetDeviceProxy>> DeviceProxies;
	TargetDeviceServicesModule->GetDeviceProxyManager()->GetProxies(FName(*InDeviceName), true, DeviceProxies);
	// for each proxy...
	for (auto DeviceProxyIt = DeviceProxies.CreateIterator(); DeviceProxyIt; ++DeviceProxyIt)
	{
		TSharedPtr<ITargetDeviceProxy> DeviceProxy = *DeviceProxyIt;
		if (DeviceProxy->IsConnected())
		{
			OutDeviceID = DeviceProxy->GetTargetDeviceId((FName)*InDeviceName);
			break;
		}
	}
}

bool FAutomationEditorCommonUtils::SetOrthoViewportView(const FVector& ViewLocation, const FRotator& ViewRotation)
{
	for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); i++)
	{
		FLevelEditorViewportClient* ViewportClient = GEditor->LevelViewportClients[i];
		if (!ViewportClient->IsOrtho())
		{
			ViewportClient->SetViewLocation(ViewLocation);
			ViewportClient->SetViewRotation(ViewRotation);
			return true;
		}
	}

	UE_LOG(LogEditorAutomationTests, Log, TEXT("An ortho viewport was not found.  May affect the test results."));
	return false;
}

//////////////////////////////////////////////////////////////////////
//Asset Path Commands

/**
* Converts a package path to an asset path
*
* @param PackagePath - The package path to convert
*/
FString FAutomationEditorCommonUtils::ConvertPackagePathToAssetPath(const FString& PackagePath)
{
	const FString Filename = FPaths::ConvertRelativePathToFull(PackagePath);
	FString EngineFileName = Filename;
	FString GameFileName = Filename;
	if (FPaths::MakePathRelativeTo(EngineFileName, *FPaths::EngineContentDir()) && !EngineFileName.Contains(TEXT("../")))
	{
		const FString ShortName = FPaths::GetBaseFilename(EngineFileName);
		const FString PathName = FPaths::GetPath(EngineFileName);
		const FString AssetName = FString::Printf(TEXT("/Engine/%s/%s.%s"), *PathName, *ShortName, *ShortName);
		return AssetName;
	}
	else if (FPaths::MakePathRelativeTo(GameFileName, *FPaths::ProjectContentDir()) && !GameFileName.Contains(TEXT("../")))
	{
		const FString ShortName = FPaths::GetBaseFilename(GameFileName);
		const FString PathName = FPaths::GetPath(GameFileName);
		const FString AssetName = FString::Printf(TEXT("/Game/%s/%s.%s"), *PathName, *ShortName, *ShortName);
		return AssetName;
	}
	else
	{
		UE_LOG(LogAutomationEditorCommon, Error, TEXT("PackagePath (%s) is invalid for the current project"), *PackagePath);
		return TEXT("");
	}
}

/**
* Gets the asset data from a package path
*
* @param PackagePath - The package path used to look up the asset data
*/
FAssetData FAutomationEditorCommonUtils::GetAssetDataFromPackagePath(const FString& PackagePath)
{
	FString AssetPath = FAutomationEditorCommonUtils::ConvertPackagePathToAssetPath(PackagePath);
	if (AssetPath.Len() > 0)
	{
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		return AssetRegistry.GetAssetByObjectPath(*AssetPath);
	}

	return FAssetData();
}

//////////////////////////////////////////////////////////////////////
//Find Asset Commands

/**
* Generates a list of assets from the ENGINE and the GAME by a specific type.
* This is to be used by the GetTest() function.
*/
void FAutomationEditorCommonUtils::CollectTestsByClass(UClass * Class, TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> ObjectList;
	AssetRegistryModule.Get().GetAssetsByClass(Class->GetFName(), ObjectList);

	for (TObjectIterator<UClass> AllClassesIt; AllClassesIt; ++AllClassesIt)
	{
		UClass* ClassList = *AllClassesIt;
		FName ClassName = ClassList->GetFName();
	}

	for (auto ObjIter = ObjectList.CreateConstIterator(); ObjIter; ++ObjIter)
	{
		const FAssetData& Asset = *ObjIter;
		FString Filename = Asset.ObjectPath.ToString();
		//convert to full paths
		Filename = FPackageName::LongPackageNameToFilename(Filename);
		if (FAutomationTestFramework::Get().ShouldTestContent(Filename))
		{
			FString BeautifiedFilename = Asset.AssetName.ToString();
			OutBeautifiedNames.Add(BeautifiedFilename);
			OutTestCommands.Add(Asset.ObjectPath.ToString());
		}
	}
}

/**
* Generates a list of assets from the GAME by a specific type.
* This is to be used by the GetTest() function.
*/
void FAutomationEditorCommonUtils::CollectGameContentTestsByClass(UClass * Class, bool bRecursiveClass, TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands)
{
	//Setting the Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	//Variable setups
	TArray<FAssetData> ObjectList;
	FARFilter AssetFilter;

	//Generating the list of assets.
	//This list is being filtered by the game folder and class type.  The results are placed into the ObjectList variable.
	AssetFilter.ClassNames.Add(Class->GetFName());

	//removed path as a filter as it causes two large lists to be sorted.  Filtering on "game" directory on iteration
	//AssetFilter.PackagePaths.Add("/Game");
	AssetFilter.bRecursiveClasses = bRecursiveClass;
	AssetFilter.bRecursivePaths = true;
	AssetRegistryModule.Get().GetAssets(AssetFilter, ObjectList);

	//Loop through the list of assets, make their path full and a string, then add them to the test.
	for (auto ObjIter = ObjectList.CreateConstIterator(); ObjIter; ++ObjIter)
	{
		const FAssetData& Asset = *ObjIter;
		FString Filename = Asset.ObjectPath.ToString();

		if (Filename.StartsWith("/Game"))
		{
			//convert to full paths
			Filename = FPackageName::LongPackageNameToFilename(Filename);
			if (FAutomationTestFramework::Get().ShouldTestContent(Filename))
			{
				FString BeautifiedFilename = Asset.AssetName.ToString();
				OutBeautifiedNames.Add(BeautifiedFilename);
				OutTestCommands.Add(Asset.ObjectPath.ToString());
			}
		}
	}
}

void FAutomationEditorCommonUtils::LoadMap(const FString& MapName)
{
	bool bLoadAsTemplate = false;
	bool bShowProgress = false;
	FEditorFileUtils::LoadMap(MapName, bLoadAsTemplate, bShowProgress);
}

void FAutomationEditorCommonUtils::RunPIE()
{
	bool bInSimulateInEditor = true;
	//once in the editor
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());

	//wait between tests
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));

	//once not in the editor
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
}

/**
* Generates a list of assets from the GAME by a specific type.
* This is to be used by the GetTest() function.
*/
void FAutomationEditorCommonUtils::CollectGameContentTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands)
{
	//Setting the Asset Registry
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	//Variable setups
	TArray<FAssetData> ObjectList;
	FARFilter AssetFilter;
	
	//removed path as a filter as it causes two large lists to be sorted.  Filtering on "game" directory on iteration
	AssetFilter.PackagePaths.Add("/Game");
	AssetFilter.bRecursiveClasses = true;
	AssetFilter.bRecursivePaths = true;
	AssetRegistryModule.Get().GetAssets(AssetFilter, ObjectList);

	//Loop through the list of assets, make their path full and a string, then add them to the test.
	for (auto ObjIter = ObjectList.CreateConstIterator(); ObjIter; ++ObjIter)
	{
		const FAssetData& Asset = *ObjIter;
		if (Asset.GetClass() == nullptr)
		{
			// a nullptr class is bad !
			UE_LOG(LogAutomationEditorCommon, Warning, TEXT("GetClass for %s (%s) returned nullptr. Asset ignored"), *Asset.AssetName.ToString(), *Asset.ObjectPath.ToString());
		}
		else 
		{
			FString Filename = Asset.ObjectPath.ToString();

			if (Filename.StartsWith("/Game"))
			{
				//convert to full paths
				Filename = FPackageName::LongPackageNameToFilename(Filename);
				if (FAutomationTestFramework::Get().ShouldTestContent(Filename))
				{
					FString BeautifiedFilename = FString::Printf(TEXT("%s.%s"), *Asset.GetClass()->GetFName().ToString(), *Asset.AssetName.ToString());
					OutBeautifiedNames.Add(BeautifiedFilename);
					OutTestCommands.Add(Asset.ObjectPath.ToString());
				}
			}
		}
	}
}








///////////////////////////////////////////////////////////////////////
// Common Latent commands

//Latent Undo and Redo command
//If bUndo is true then the undo action will occur otherwise a redo will happen.
bool FUndoRedoCommand::Update()
{
	if ( bUndo == true )
	{
		//Undo
		GEditor->UndoTransaction();
	}
	else
	{
		//Redo
		GEditor->RedoTransaction();
	}

	return true;
}

/**
* Open editor for a particular asset
*/
bool FOpenEditorForAssetCommand::Update()
{
	UObject* Object = StaticLoadObject(UObject::StaticClass(), NULL, *AssetName);
	if ( Object )
	{
		FAssetEditorManager::Get().OpenEditorForAsset(Object);
		//This checks to see if the asset sub editor is loaded.
		if ( FAssetEditorManager::Get().FindEditorForAsset(Object, true) != NULL )
		{
			UE_LOG(LogEditorAutomationTests, Log, TEXT("Verified asset editor for: %s."), *AssetName);
			UE_LOG(LogEditorAutomationTests, Display, TEXT("The editor successfully loaded for: %s."), *AssetName);
			return true;
		}
	}
	else
	{
		UE_LOG(LogEditorAutomationTests, Error, TEXT("Failed to find object: %s."), *AssetName);
	}
	return true;
}

/**
* Close all sub-editors
*/
bool FCloseAllAssetEditorsCommand::Update()
{
	FAssetEditorManager::Get().CloseAllAssetEditors();

	//Get all assets currently being tracked with open editors and make sure they are not still opened.
	if ( FAssetEditorManager::Get().GetAllEditedAssets().Num() >= 1 )
	{
		UE_LOG(LogEditorAutomationTests, Warning, TEXT("Not all of the editors were closed."));
		return true;
	}

	UE_LOG(LogEditorAutomationTests, Log, TEXT("Verified asset editors were closed"));
	UE_LOG(LogEditorAutomationTests, Display, TEXT("The asset editors closed successfully"));
	return true;
}

/**
* Start PIE session
*/
bool FStartPIECommand::Update()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<class ILevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveViewport();

	GUnrealEd->RequestPlaySession(false, ActiveLevelViewport, bSimulateInEditor, NULL, NULL, -1, false);
	return true;
}

/**
* End PlayMap session
*/
bool FEndPlayMapCommand::Update()
{
	GUnrealEd->RequestEndPlayMap();
	return true;
}

/**
* This this command loads a map into the editor.
*/
bool FEditorLoadMap::Update()
{
	//Get the base filename for the map that will be used.
	FString ShortMapName = FPaths::GetBaseFilename(MapName);

	//Get the current number of seconds before loading the map.
	double MapLoadStartTime = FPlatformTime::Seconds();

	//Load the map
	FAutomationEditorCommonUtils::LoadMap(MapName);

	//This is the time it took to load the map in the editor.
	double MapLoadTime = FPlatformTime::Seconds() - MapLoadStartTime;

	//Gets the main frame module to get the name of our current level.
	const IMainFrameModule& MainFrameModule = FModuleManager::GetModuleChecked< IMainFrameModule >("MainFrame");
	FString LoadedMapName = MainFrameModule.GetLoadedLevelName();

	UE_LOG(LogEditorAutomationTests, Log, TEXT("%s has been loaded."), *ShortMapName);

	//Log out to a text file the time it takes to load the map.
	FAutomationEditorCommonUtils::WriteToTextFile(TEXT("Performance"), LoadedMapName, TEXT("RAWMapLoadTime.txt"), MapLoadTime, TEXT(","));

	UE_LOG(LogEditorAutomationTests, Display, TEXT("%s took %.3f to load."), *LoadedMapName, MapLoadTime);

	return true;
}

/**
* This will cause the test to wait for the shaders to finish compiling before moving on.
*/
bool FWaitForShadersToFinishCompiling::Update()
{
	UE_LOG(LogEditorAutomationTests, Log, TEXT("Waiting for %i shaders to finish."), GShaderCompilingManager->GetNumRemainingJobs());
	GShaderCompilingManager->FinishAllCompilation();
	UE_LOG(LogEditorAutomationTests, Log, TEXT("Done waiting for shaders to finish."));
	return true;
}

/**
* Latent command that changes the editor viewport to the first available bookmarked view.
*/
bool FChangeViewportToFirstAvailableBookmarkCommand::Update()
{
	FEditorModeTools EditorModeTools;
	FLevelEditorViewportClient* ViewportClient;
	uint32 ViewportIndex = 0;

	UE_LOG(LogEditorAutomationTests, Log, TEXT("Attempting to change the editor viewports view to the first set bookmark."));

	//Move the perspective viewport view to show the test.
	for ( int32 i = 0; i < GEditor->LevelViewportClients.Num(); i++ )
	{
		ViewportClient = GEditor->LevelViewportClients[i];

		for ( ViewportIndex = 0; ViewportIndex <= AWorldSettings::MAX_BOOKMARK_NUMBER; ViewportIndex++ )
		{
			if ( EditorModeTools.CheckBookmark(ViewportIndex, ViewportClient) )
			{
				UE_LOG(LogEditorAutomationTests, VeryVerbose, TEXT("Changing a viewport view to the set bookmark %i"), ViewportIndex);
				EditorModeTools.JumpToBookmark(ViewportIndex, true, ViewportClient);
				break;
			}
		}
	}
	return true;
}

/**
* Latent command that adds a static mesh to the worlds origin.
*/
bool FAddStaticMeshCommand::Update()
{
	//Gather assets.
	UObject* Cube = (UStaticMesh*)StaticLoadObject(UStaticMesh::StaticClass(), NULL, TEXT("/Engine/EngineMeshes/Cube.Cube"), NULL, LOAD_None, NULL);
	//Add Cube mesh to the world
	AStaticMeshActor* StaticMesh = Cast<AStaticMeshActor>(FActorFactoryAssetProxy::AddActorForAsset(Cube));
	StaticMesh->TeleportTo(FVector(0.0f, 0.0f, 0.0f), FRotator(0, 0, 0));
	StaticMesh->SetActorRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));

	UE_LOG(LogEditorAutomationTests, Log, TEXT("Static Mesh cube has been added to 0, 0, 0."))

		return true;
}

/**
* Latent command that builds lighting for the current level.
*/
bool FBuildLightingCommand::Update()
{
	//If we are running with -NullRHI then we have to skip this step.
	if ( GUsingNullRHI )
	{
		UE_LOG(LogEditorAutomationTests, Log, TEXT("SKIPPED Build Lighting Step.  You're currently running with -NullRHI."));
		return true;
	}

	if ( GUnrealEd->WarnIfLightingBuildIsCurrentlyRunning() )
	{
		UE_LOG(LogEditorAutomationTests, Warning, TEXT("Lighting is already being built."));
		return true;
	}

	UWorld* CurrentWorld = GEditor->GetEditorWorldContext().World();
	GUnrealEd->Exec(CurrentWorld, TEXT("MAP REBUILD"));

	FLightingBuildOptions LightingBuildOptions;

	// Retrieve settings from ini.
	GConfig->GetBool(TEXT("LightingBuildOptions"), TEXT("OnlyBuildSelected"), LightingBuildOptions.bOnlyBuildSelected, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LightingBuildOptions"), TEXT("OnlyBuildCurrentLevel"), LightingBuildOptions.bOnlyBuildCurrentLevel, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LightingBuildOptions"), TEXT("OnlyBuildSelectedLevels"), LightingBuildOptions.bOnlyBuildSelectedLevels, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LightingBuildOptions"), TEXT("OnlyBuildVisibility"), LightingBuildOptions.bOnlyBuildVisibility, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LightingBuildOptions"), TEXT("UseErrorColoring"), LightingBuildOptions.bUseErrorColoring, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("LightingBuildOptions"), TEXT("ShowLightingBuildInfo"), LightingBuildOptions.bShowLightingBuildInfo, GEditorPerProjectIni);
	int32 QualityLevel;
	GConfig->GetInt(TEXT("LightingBuildOptions"), TEXT("QualityLevel"), QualityLevel, GEditorPerProjectIni);
	QualityLevel = FMath::Clamp<int32>(QualityLevel, Quality_Preview, Quality_Production);
	LightingBuildOptions.QualityLevel = Quality_Production;

	UE_LOG(LogEditorAutomationTests, Log, TEXT("Building lighting in Production Quality."));
	GUnrealEd->BuildLighting(LightingBuildOptions);

	return true;
}


bool FSaveLevelCommand::Update()
{
	if ( !GUnrealEd->IsLightingBuildCurrentlyExporting() && !GUnrealEd->IsLightingBuildCurrentlyRunning() )
	{
		UWorld* World = GEditor->GetEditorWorldContext().World();
		ULevel* Level = World->GetCurrentLevel();
		MapName += TEXT("_Copy.umap");
		FString TempMapLocation = FPaths::Combine(*FPaths::ProjectContentDir(), TEXT("Maps"), TEXT("Automation_TEMP"), *MapName);
		FEditorFileUtils::SaveLevel(Level, TempMapLocation);

		return true;
	}
	return false;
}

bool FLaunchOnCommand::Update()
{
	GUnrealEd->AutomationPlayUsingLauncher(InLauncherDeviceID);
	return true;
}

bool FWaitToFinishCookByTheBookCommand::Update()
{
	if ( !GUnrealEd->CookServer->IsCookByTheBookRunning() )
	{
		if ( GUnrealEd->IsCookByTheBookInEditorFinished() )
		{
			UE_LOG(LogEditorAutomationTests, Log, TEXT("The cook by the book operation has finished."));
		}
		return true;
	}
	else if ( ( FPlatformTime::Seconds() - StartTime ) == COOK_TIMEOUT )
	{
		GUnrealEd->CancelCookByTheBookInEditor();
		UE_LOG(LogEditorAutomationTests, Error, TEXT("It has been an hour or more since the cook has started."));
		return false;
	}
	return false;
}

bool FDeleteDirCommand::Update()
{
	FString FullFolderPath = FPaths::ConvertRelativePathToFull(*InFolderLocation);
	if ( IFileManager::Get().DirectoryExists(*FullFolderPath) )
	{
		IFileManager::Get().DeleteDirectory(*FullFolderPath, false, true);
	}
	return true;
}

bool FWaitToFinishBuildDeployCommand::Update()
{
	if ( GEditor->LauncherWorker->GetStatus() == ELauncherWorkerStatus::Completed )
	{
		UE_LOG(LogEditorAutomationTests, Log, TEXT("The build game and deploy operation has finished."));
		return true;
	}
	else if ( GEditor->LauncherWorker->GetStatus() == ELauncherWorkerStatus::Canceled || GEditor->LauncherWorker->GetStatus() == ELauncherWorkerStatus::Canceling )
	{
		UE_LOG(LogEditorAutomationTests, Warning, TEXT("The build was canceled."));
		return true;
	}
	return false;
}
