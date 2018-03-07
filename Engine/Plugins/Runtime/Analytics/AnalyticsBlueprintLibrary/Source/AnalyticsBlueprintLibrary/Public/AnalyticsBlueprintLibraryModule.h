// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * The public interface to this module
 */
class FAnalyticsBlueprintLibraryModule :
	public IModuleInterface
{
	//--------------------------------------------------------------------------
	// Module functionality
	//--------------------------------------------------------------------------
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FAnalyticsBlueprintLibraryModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FAnalyticsBlueprintLibraryModule >( "AnalyticsBlueprintLibrary" );
	}

private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

