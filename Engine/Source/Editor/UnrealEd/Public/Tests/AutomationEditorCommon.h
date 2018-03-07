// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AssetData.h"
#include "Tests/AutomationTestSettings.h"

class UFactory;

//////////////////////////////////////////////////////////////////////////
// FAutomationEditorCommonUtils
class UNREALED_API FAutomationEditorCommonUtils
{
public:
	
	/**
	* Creates a new map for editing.  Also clears editor tools that could cause issues when changing maps
	*
	* @return - The UWorld for the new map
	*/
	static UWorld* CreateNewMap();

	/**
	* Imports an object using a given factory
	*
	* @param ImportFactory - The factory to use to import the object
	* @param ObjectName - The name of the object to create
	* @param PackagePath - The full path of the package file to create
	* @param ImportPath - The path to the object to import
	*/
	static UObject* ImportAssetUsingFactory(UFactory* ImportFactory, const FString& ObjectName, const FString& PackageName, const FString& ImportPath);

	/**
	* Nulls out references to a given object
	*
	* @param InObject - Object to null references to
	*/
	static void NullReferencesToObject(UObject* InObject);

	/**
	* gets a factory class based off an asset file extension
	*
	* @param AssetExtension - The file extension to use to find a supporting UFactory
	*/
	static UClass* GetFactoryClassForType(const FString& AssetExtension);

	/**
	* Applies settings to an object by finding UProperties by name and calling ImportText
	*
	* @param InObject - The object to search for matching properties
	* @param PropertyChain - The list UProperty names recursively to search through
	* @param Value - The value to import on the found property
	*/
	static void ApplyCustomFactorySetting(UObject* InObject, TArray<FString>& PropertyChain, const FString& Value);

	/**
	* Applies the custom factory settings
	*
	* @param InFactory - The factory to apply custom settings to
	* @param FactorySettings - An array of custom settings to apply to the factory
	*/
	static void ApplyCustomFactorySettings(UFactory* InFactory, const TArray<FImportFactorySettingValues>& FactorySettings);

	/**
	* Writes a number to a text file.
	*
	* @param InTestName - is the folder that has the same name as the test. (For Example: "Performance").
	* @param InItemBeingTested - is the name for the thing that is being tested. (For Example: "MapName").
	* @param InFileName - is the name of the file with an extension
	* @param InNumberToBeWritten - is the float number that is expected to be written to the file.
	* @param Delimiter - is the delimiter to be used. TEXT(",")
	*/
	static void WriteToTextFile(const FString& InTestName, const FString& InTestItem, const FString& InFileName, const float& InEntry, const FString& Delimiter);

	/**
	* Returns the sum of the numbers available in an array of float.

	* @param InFloatArray - is the name of the array intended to be used.
	* @param bisAveragedInstead - will return the average of the available numbers instead of the sum.
	*/
	static float TotalFromFloatArray(const TArray<float>& InFloatArray, bool bisAveragedInstead);

	/**
	* Returns the largest value from an array of float numbers.

	* @param InFloatArray - is the name of the array intended to be used.
	*/
	static float LargestValueInFloatArray(const TArray<float>& InFloatArray);

	/**
	* Returns the contents of a text file as an array of FString.

	* @param InFileLocation - is the location of the file.
	* @param OutArray - The name of the array that will store the data.
	*/
	static void CreateArrayFromFile(const FString& InFileLocation, TArray<FString>& OutArray);

	/**
	* Returns true if the archive/file can be written to otherwise false.

	* @param InFilePath - is the location of the file.
	* @param InArchiveName - is the name of the archive to be used.
	*/
	static bool IsArchiveWriteable(const FString& InFilePath, const FArchive* InArchiveName);

	/**
	* Returns the first DeviceID 'Platform@Device'
	* Searches the automation preferences for the platform to use.

	* @param OutDeviceID - The variable that will hold the device ID.
	* @param InMapName - The map name to check against in the automation preferences.
	*/
	static void GetLaunchOnDeviceID(FString& OutDeviceID, const FString& InMapName);

	/**
	* Returns the DeviceID 'Platform@Device'
	* Searches the automation preferences for the platform to use.

	* @param OutDeviceID - The variable that will hold the device ID.
	* @param InMapName - The map name to check against in the automation preferences.
	* @param InDeviceName - Device Name
	*/
	static void GetLaunchOnDeviceID(FString& OutDeviceID, const FString& InMapName, const FString& InDeviceName);

	/**
	* Sets the first found ortho viewport camera to the desired location and rotation.

	* @param ViewLocation - Desired location for the viewport view.
	* @param ViewRotation - Desired rotation of the viewport view.
	*/
	static bool SetOrthoViewportView(const FVector& ViewLocation, const FRotator& ViewRotation);

	/**
	* Converts a package path to an asset path
	*
	* @param PackagePath - The package path to convert
	*/
	static FString ConvertPackagePathToAssetPath(const FString& PackagePath);

	/**
	* Gets the asset data from a package path
	*
	* @param PackagePath - The package path used to look up the asset data
	*/
	static FAssetData GetAssetDataFromPackagePath(const FString& PackagePath);

	/**
	* Loads the map specified by an automation test
	*
	* @param MapName - Map to load
	*/
	static void LoadMap(const FString& MapName);

	/**
	* Run PIE
	*/
	static void RunPIE();
	
	/**
	* Generates a list of assets from the ENGINE and the GAME by a specific type.
	* This is to be used by the GetTest() function.
	*/
	static void CollectTestsByClass(UClass * Class, TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands);
	
	/**
	* Generates a list of assets from the GAME by a specific type.
	* This is to be used by the GetTest() function.
	*/
	static void CollectGameContentTestsByClass(UClass * Class, bool bRecursiveClass, TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands);
		
	/**
	* Generates a list of assets from the GAME by a specific type.
	* This is to be used by the GetTest() function.
	*/
	static void CollectGameContentTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands);
};


//////////////////////////////////////////////////////////////////////////
//Common latent commands used for automated editor testing.

/**
* Creates a latent command which the user can either undo or redo an action.
* True will trigger Undo, False will trigger a redo
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(UNREALED_API, FUndoRedoCommand, bool, bUndo);

/**
* Open editor for a particular asset
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(UNREALED_API, FOpenEditorForAssetCommand, FString, AssetName);

/**
* Close all asset editors
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND(UNREALED_API, FCloseAllAssetEditorsCommand);

/**
* Start PIE session
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(UNREALED_API, FStartPIECommand, bool, bSimulateInEditor);

/**
* End PlayMap session
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND(UNREALED_API, FEndPlayMapCommand);

/**
* Loads a map
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(UNREALED_API, FEditorLoadMap, FString, MapName);

/**
* Waits for shaders to finish compiling before moving on to the next thing.
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND(UNREALED_API, FWaitForShadersToFinishCompiling);

/**
* Latent command that changes the editor viewport to the first available bookmarked view.
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND(UNREALED_API, FChangeViewportToFirstAvailableBookmarkCommand);

/**
* Latent command that adds a static mesh to the worlds origin.
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND(UNREALED_API, FAddStaticMeshCommand);

/**
* Latent command that builds the lighting for the current level.
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND(UNREALED_API, FBuildLightingCommand);

/**
* Latent command that saves a copy of the level to a transient folder.
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(UNREALED_API, FSaveLevelCommand, FString, MapName);

/**
* Triggers a launch on using a specified device ID.
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(UNREALED_API, FLaunchOnCommand, FString, InLauncherDeviceID);

/**
* Wait for a cook by the book to finish.  Will time out after 3600 seconds (1 hr).
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND(UNREALED_API, FWaitToFinishCookByTheBookCommand);

/**
* Wait for a build and deploy to finish before moving on.
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND(UNREALED_API, FWaitToFinishBuildDeployCommand);

/**
* Latent command to delete a directory.
*/
DEFINE_EXPORTED_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(UNREALED_API, FDeleteDirCommand, FString, InFolderLocation);
