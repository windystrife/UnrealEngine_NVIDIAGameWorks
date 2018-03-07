// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"

class ULocationServicesIOSImpl;
/**
 * The public interface to this module
 */
class FLocationServicesIOSImplModule :
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
	static inline FLocationServicesIOSImplModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FLocationServicesIOSImplModule >( "LocationServicesIOSImpl" );
	}

private:
	ULocationServicesIOSImpl* ImplInstance;
	
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

