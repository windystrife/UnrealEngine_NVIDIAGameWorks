// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"

struct SLATE_API FLayoutSaveRestore
{
	/** Gets the ini section label for the additional layout configs */
	static const FString& GetAdditionalLayoutConfigIni();

	/** Write the layout out into a named config file.
	 *
	 * @param ConfigFileName file to be save to
	 * @param LayoutToSave the layout to save.
	 */
	static void SaveToConfig( FString ConfigFileName, const TSharedRef<FTabManager::FLayout>& LayoutToSave );

	/** Given a named DefualtLayout, return any saved version of it from the given ini file, otherwise return the default, also default to open tabs based on bool
	 *
	 * @param ConfigFileName file to be used to load an existing layout
	 * @param DefaultLayout the layout to be used if the file does not exist.
	 *
	 * @return Loaded layout or the default
	 */
	static TSharedRef<FTabManager::FLayout> LoadFromConfig( const FString& ConfigFileName, const TSharedRef<FTabManager::FLayout>& DefaultLayout );

	/**
	 * Migrates the layout configuration from one config file to another.
	 *
	 * @param OldConfigFileName The name of the old configuration file.
	 * @param NewConfigFileName The name of the new configuration file.
	 */
	static void MigrateConfig( const FString& OldConfigFileName, const FString& NewConfigFileName );

private:
	/**
	 * Make a Json string friendly for writing out to UE .ini config files.
	 * The opposite of GetLayoutStringFromIni.
	 */
	static FString PrepareLayoutStringForIni(const FString& LayoutString);

	/**
	 * Convert from UE .ini Json string to a vanilla Json string.
	 * The opposite of PrepareLayoutStringForIni.
	 */
	static FString GetLayoutStringFromIni(const FString& LayoutString);
};
