// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FQosInterface;

/** Logging related to parties */
QOS_API DECLARE_LOG_CATEGORY_EXTERN(LogQos, Display, All);

/**
 * Module for QoS service utilities
 */
class QOS_API FQosModule :
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
	static inline FQosModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FQosModule>("Qos");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("Qos");
	}

	/**
	 * Get the interface singleton
	 */
	TSharedRef<FQosInterface> GetQosInterface();

private:
	TSharedPtr<FQosInterface> QosInterface;

private:

	// IModuleInterface

	/**
	 * Called when voice module is loaded
	 * Initialize platform specific parts of template handling
	 */
	virtual void StartupModule() override;
	
	/**
	 * Called when voice module is unloaded
	 * Shutdown platform specific parts of template handling
	 */
	virtual void ShutdownModule() override;
};

