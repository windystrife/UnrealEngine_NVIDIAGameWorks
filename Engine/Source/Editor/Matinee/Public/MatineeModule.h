// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Toolkits/AssetEditorToolkit.h"

class AMatineeActor;
class IMainFrameModule;
class IMatinee;

extern const FName MatineeAppIdentifier;

//class FMatinee;

/*-----------------------------------------------------------------------------
   IMatineeModule
-----------------------------------------------------------------------------*/

class IMatineeModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	/** Creates a new Matinee instance */
	virtual TSharedRef<IMatinee> CreateMatinee(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, AMatineeActor* MatineeActor) = 0;

	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline IMatineeModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IMatineeModule >("Matinee");
	}

	/**
	* Delegate for binding functions to be called when the Matinee editor is created.
	*/
	DECLARE_EVENT(IMainFrameModule, FMatineeEditorOpenedEvent);
	virtual FMatineeEditorOpenedEvent& OnMatineeEditorOpened() = 0;
};
