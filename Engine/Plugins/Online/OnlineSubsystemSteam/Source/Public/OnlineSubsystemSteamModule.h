// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

#define LOADING_STEAM_CLIENT_LIBRARY_DYNAMICALLY				(PLATFORM_WINDOWS || PLATFORM_MAC || (PLATFORM_LINUX && !IS_MONOLITHIC))
#define LOADING_STEAM_SERVER_LIBRARY_DYNAMICALLY				((PLATFORM_WINDOWS && PLATFORM_32BITS) || (PLATFORM_LINUX && !IS_MONOLITHIC) || PLATFORM_MAC)
#define LOADING_STEAM_LIBRARIES_DYNAMICALLY						(LOADING_STEAM_CLIENT_LIBRARY_DYNAMICALLY || LOADING_STEAM_SERVER_LIBRARY_DYNAMICALLY)

/**
 * Online subsystem module class  (STEAM Implementation)
 * Code related to the loading of the STEAM module
 */
class FOnlineSubsystemSteamModule : public IModuleInterface
{
private:

	/** Class responsible for creating instance(s) of the subsystem */
	class FOnlineFactorySteam* SteamFactory;

#if LOADING_STEAM_LIBRARIES_DYNAMICALLY
	/** Handle to the STEAM API dll */
	void* SteamDLLHandle;
	/** Handle to the STEAM dedicated server support dlls */
	void* SteamServerDLLHandle;
#endif	//LOADING_STEAM_LIBRARIES_DYNAMICALLY

	/**
	 *	Load the required modules for Steam
	 */
	void LoadSteamModules();

	/** 
	 *	Unload the required modules for Steam
	 */
	void UnloadSteamModules();

public:

	FOnlineSubsystemSteamModule()
        : SteamFactory(NULL)
#if LOADING_STEAM_LIBRARIES_DYNAMICALLY
		, SteamDLLHandle(NULL)
		, SteamServerDLLHandle(NULL)
#endif	//LOADING_STEAM_LIBRARIES_DYNAMICALLY
	{}

	virtual ~FOnlineSubsystemSteamModule() {}

	// IModuleInterface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	virtual bool SupportsAutomaticShutdown() override
	{
		return false;
	}

	/**
	 * Are the Steam support Dlls loaded
	 */
	bool AreSteamDllsLoaded() const;
};
