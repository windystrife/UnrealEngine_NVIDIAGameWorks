// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class ISlackIncomingWebhookInterface;

/**
 * ISlackIntegrationsModule interface for public access to this module
 */
class ISlackIntegrationsModule : public IModuleInterface
{
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline ISlackIntegrationsModule& Get()
	{
		return FModuleManager::LoadModuleChecked< ISlackIntegrationsModule >( "SlackIntegrations" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "SlackIntegrations" );
	}

	/** Get the incoming webhook interface for sending messages to Slack */
	virtual ISlackIncomingWebhookInterface& GetIncomingWebhookInterface() = 0;
};
