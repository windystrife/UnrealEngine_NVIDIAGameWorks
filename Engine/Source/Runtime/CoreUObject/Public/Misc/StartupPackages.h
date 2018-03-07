// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StartupPackages.h: Startup Package Functions
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

struct FStartupPackages
{
	/**
	 * Gets the list of packages that are precached at startup for seek free loading
	 *
	 * @param PackageNames The output list of package names
	 * @param EngineConfigFilename Optional engine config filename to use to lookup the package settings
	 */
	COREUOBJECT_API static void GetStartupPackageNames(TArray<FString>& PackageNames, const FString& EngineConfigFilename=GEngineIni, bool bIsCreatingHashes=false);

	/**
	 * Fully loads a list of packages.
	 * 
	 * @param PackageNames The list of package names to load
	 */
	COREUOBJECT_API static void LoadPackageList(const TArray<FString>& PackageNames);

	/**
	 * Load all startup packages. If desired preload async followed by serialization from memory.
	 *
	 * The list of additional packages can be found at Engine.StartupPackages and if seekfree loading
	 * is enabled, the startup map is going to be preloaded as well.
	 *
	 * Returns false if any modules failed to load.
	 */
	COREUOBJECT_API static bool LoadAll();
};
