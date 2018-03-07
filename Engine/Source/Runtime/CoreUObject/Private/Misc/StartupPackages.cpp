// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StartupPackages.cpp: Startup Package Functions
=============================================================================*/

#include "Misc/StartupPackages.h"
#include "Misc/CommandLine.h"
#include "Stats/Stats.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/Package.h"
#include "UObject/LinkerLoad.h"

void FStartupPackages::GetStartupPackageNames(TArray<FString>& PackageNames, const FString& EngineConfigFilename, bool bIsCreatingHashes)
{
	// if we aren't cooking, we actually just want to use the cooked startup package as the only startup package
	if (FPlatformProperties::RequiresCookedData())
	{
		// look for any packages that we want to force preload at startup
		FConfigSection* PackagesToPreload = GConfig->GetSectionPrivate(TEXT("Engine.StartupPackages"), 0, 1, EngineConfigFilename);
		if (PackagesToPreload)
		{
			// go through list and add to the array
			for( FConfigSectionMap::TIterator It(*PackagesToPreload); It; ++It )
			{
				if (It.Key() == TEXT("Package"))
				{
					// add this package to the list to be fully loaded later
					PackageNames.Add(It.Value().GetValue());
				}
			}
		}
	}
}

void FStartupPackages::LoadPackageList(const TArray<FString>& PackageNames)
{
	// Iterate over all native script packages and fully load them.
	for( int32 PackageIndex=0; PackageIndex<PackageNames.Num(); PackageIndex++ )
	{
		UObject* Package = LoadPackage(NULL, *PackageNames[PackageIndex], LOAD_None);
	}
}

bool FStartupPackages::LoadAll()
{
	bool bReturn = true;

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading Startup Packages"), STAT_StartupPackages, STATGROUP_LoadTime);

	// Get list of startup packages.
	TArray<FString> StartupPackages;
	
	// if the user wants to skip loading these, then don't (can be helpful for deleting objects in startup packages in the editor, etc)
	if (!FParse::Param(FCommandLine::Get(), TEXT("NoLoadStartupPackages")))
	{
		FStartupPackages::GetStartupPackageNames(StartupPackages);
	}

	return bReturn;
}
