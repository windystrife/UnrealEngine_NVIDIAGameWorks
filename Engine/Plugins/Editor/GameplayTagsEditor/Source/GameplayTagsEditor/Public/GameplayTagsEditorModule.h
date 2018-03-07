// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "IPropertyTypeCustomization.h"


/**
 * The public interface to this module
 */
class IGameplayTagsEditorModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IGameplayTagsEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IGameplayTagsEditorModule >( "GameplayTagsEditor" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "GameplayTagsEditor" );
	}

	/** Tries to add a new gameplay tag to the ini lists */
	GAMEPLAYTAGSEDITOR_API virtual bool AddNewGameplayTagToINI(const FString& NewTag, const FString& Comment = TEXT(""), FName TagSourceName = NAME_None) = 0;

	/** Tries to delete a tag from the library. This will pop up special UI or error messages as needed. It will also delete redirectors if that is specified. */
	GAMEPLAYTAGSEDITOR_API virtual bool DeleteTagFromINI(const FString& TagToDelete) = 0;

	/** Tries to rename a tag, leaving a rediretor in the ini, and adding the new tag if it does not exist yet */
	GAMEPLAYTAGSEDITOR_API virtual bool RenameTagInINI(const FString& TagToRename, const FString& TagToRenameTo) = 0;

	/** Adds a transient gameplay tag (only valid for the current editor session) */
	GAMEPLAYTAGSEDITOR_API virtual bool AddTransientEditorGameplayTag(const FString& NewTransientTag) = 0;
};

/** This is public so that child structs of FGameplayTag can use the details customization */
struct GAMEPLAYTAGSEDITOR_API FGameplayTagCustomizationPublic
{
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
};