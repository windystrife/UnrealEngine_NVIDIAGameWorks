// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ModuleDescriptor.h"
#include "AddToProjectConfig.h"

struct FSlateBrush;
struct FTemplateCategory;

/** Context information used when validating that source code is being placed in the correct place for a given module */
struct FModuleContextInfo
{
	/** Path to the Source folder of the module */
	FString ModuleSourcePath;

	/** Name of the module */
	FString ModuleName;

	/** Type of this module, eg, Runtime, Editor, etc */
	enum EHostType::Type ModuleType;
};

/**
 * Game Project Generation module
 */
class FGameProjectGenerationModule : public IModuleInterface
{

public:
	typedef TMap<FName, TSharedPtr<FTemplateCategory>> FTemplateCategoryMap;

	/**
	 * Called right after the plugin DLL has been loaded and the plugin object has been created
	 */
	virtual void StartupModule();

	/**
	 * Called before the plugin is unloaded, right before the plugin object is destroyed.
	 */
	virtual void ShutdownModule();

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FGameProjectGenerationModule& Get()
	{
		static const FName ModuleName = "GameProjectGeneration";
		return FModuleManager::LoadModuleChecked< FGameProjectGenerationModule >( ModuleName );
	}

	/** Creates the game project dialog */
	virtual TSharedRef<class SWidget> CreateGameProjectDialog(bool bAllowProjectOpening, bool bAllowProjectCreate);

	/** Creates a new class dialog for creating classes based on the passed-in class. */
	virtual TSharedRef<class SWidget> CreateNewClassDialog(const UClass* InClass);
	
	/** 
	 * Opens a dialog to add code files to the current project. 
	 *
	 * @param	Config		Dialog configuration options
	 */
	virtual void OpenAddCodeToProjectDialog(const FAddToProjectConfig& Config = FAddToProjectConfig());

	/** 
	 * Opens a dialog to add a new blueprint to the current project. 
	 *
	 * @param	Config		Dialog configuration options
	 */
	virtual void OpenAddBlueprintToProjectDialog(const FAddToProjectConfig& Config);
	
	/** Delegate for when the AddCodeToProject dialog is opened */
	DECLARE_EVENT(FGameProjectGenerationModule, FAddCodeToProjectDialogOpenedEvent);
	FAddCodeToProjectDialogOpenedEvent& OnAddCodeToProjectDialogOpened() { return AddCodeToProjectDialogOpenedEvent; }

	/** Tries to make the project file writable. Prompts to check out as necessary. */
	virtual void TryMakeProjectFileWriteable(const FString& ProjectFile);

	/** Prompts the user to update his project file, if necessary. */
	virtual void CheckForOutOfDateGameProjectFile();

	/** Updates the currently loaded project. Returns true if the project was updated successfully or if no update was needed */
	virtual bool UpdateGameProject(const FString& ProjectFile, const FString& EngineIdentifier, FText& OutFailReason);

	/** Updates the current code project */
	virtual bool UpdateCodeProject(FText& OutFailReason, FText& OutFailLog);

	/** Gets the current projects source file count */
	virtual bool ProjectHasCodeFiles();

	/** Returns the path to the module's include header */
	virtual FString DetermineModuleIncludePath(const FModuleContextInfo& ModuleInfo, const FString& FileRelativeTo);

	/** Get the information about any modules referenced in the .uproject file of the currently loaded project */
	virtual TArray<FModuleContextInfo> GetCurrentProjectModules();

	/** Returns true if the specified class is a valid base class for the given module */
	virtual bool IsValidBaseClassForCreation(const UClass* InClass, const FModuleContextInfo& InModuleInfo);

	/** Returns true if the specified class is a valid base class for any of the given modules */
	virtual bool IsValidBaseClassForCreation(const UClass* InClass, const TArray<FModuleContextInfo>& InModuleInfoArray);

	/** Gets file and size info about the source directory */
	virtual void GetProjectSourceDirectoryInfo(int32& OutNumFiles, int64& OutDirectorySize);

	/** Warn the user if the project filename is invalid in case they renamed it outside the editor */
	virtual void CheckAndWarnProjectFilenameValid();

	/** Generate basic project source code */
	virtual bool GenerateBasicSourceCode(TArray<FString>& OutCreatedFiles, FText& OutFailReason);

	/**
	 * Update the list of supported target platforms based upon the parameters provided
	 * This will take care of checking out and saving the updated .uproject file automatically
	 * 
	 * @param	InPlatformName		Name of the platform to target (eg, WindowsNoEditor)
	 * @param	bIsSupported		true if the platform should be supported by this project, false if it should not
	 */
	virtual void UpdateSupportedTargetPlatforms(const FName& InPlatformName, const bool bIsSupported);

	/** Clear the list of supported target platforms */
	virtual void ClearSupportedTargetPlatforms();

	/** Check to see if the project requires a code build */
	virtual bool ProjectRequiresBuild(const FName InPlatformInfoName);

public:

	/** (Un)register a new type of template category to be shown on the new project page */
	virtual bool RegisterTemplateCategory(FName Type, FText Name, FText Description, const FSlateBrush* Icon, const FSlateBrush* Image);
	virtual void UnRegisterTemplateCategory(FName Type);

	// Non DLL-exposed access to template categories
	TSharedPtr<const FTemplateCategory> GetCategory(FName Type) const { return TemplateCategories.FindRef(Type); }

private:
	FAddCodeToProjectDialogOpenedEvent AddCodeToProjectDialogOpenedEvent;

	/** Map of template categories from type to ptr */
	FTemplateCategoryMap TemplateCategories;
};
