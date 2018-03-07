// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Misc/CoreMisc.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/** Logging related to parties */
LOBBY_API DECLARE_LOG_CATEGORY_EXTERN(LogLobby, Display, All);

/** Lobby module stats */
DECLARE_STATS_GROUP(TEXT("Lobby"), STATGROUP_Lobby, STATCAT_Advanced);
/** Total async thread time */
//DECLARE_CYCLE_STAT_EXTERN(TEXT("LobbyStat1"), STAT_LobbyStat, STATGROUP_Lobby, Lobby_API);

/**
 * Module for lobbies via online beacon
 */
class FLobbyModule : 
	public IModuleInterface, public FSelfRegisteringExec
{

public:

	// FSelfRegisteringExec
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FLobbyModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FLobbyModule>("Lobby");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("Lobby");
	}

private:

	// IModuleInterface

	/**
	 * Called when lobby module is loaded
	 * Initialize platform specific parts of Lobby handling
	 */
	virtual void StartupModule() override;
	
	/**
	 * Called when lobby module is unloaded
	 * Shutdown platform specific parts of Lobby handling
	 */
	virtual void ShutdownModule() override;
};

