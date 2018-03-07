// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IViewportInteractionModule.h"

class FViewportInteractionModule : public IViewportInteractionModule
{
public:
	
	FViewportInteractionModule();
	virtual ~FViewportInteractionModule();

	// FModuleInterface overrides
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual void PostLoadCallback() override;
	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}
	
	static void ToggleMode();

	void EnabledViewportWorldInteractionFromCommand(const bool bEnabled);
	bool EnabledViewportWorldInteractionFromCommand();

private:

	/** If we started the ViewportWorldInteraction from Toggle command. */
	bool bEnabledViewportWorldInteractionFromCommand;
};
