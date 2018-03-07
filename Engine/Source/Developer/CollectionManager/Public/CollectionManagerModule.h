// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class ICollectionManager;

class FCollectionManagerModule : public IModuleInterface
{
public:
	virtual void StartupModule();
	virtual void ShutdownModule();

	virtual ICollectionManager& Get() const;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FCollectionManagerModule& GetModule()
	{
		static const FName CollectionManagerModuleName("CollectionManager");
		return FModuleManager::LoadModuleChecked<FCollectionManagerModule>(CollectionManagerModuleName);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsModuleAvailable()
	{
		static const FName CollectionManagerModuleName("CollectionManager");
		return FModuleManager::Get().IsModuleLoaded(CollectionManagerModuleName);
	}

private:
	ICollectionManager* CollectionManager;
	class FCollectionManagerConsoleCommands* ConsoleCommands;
};
