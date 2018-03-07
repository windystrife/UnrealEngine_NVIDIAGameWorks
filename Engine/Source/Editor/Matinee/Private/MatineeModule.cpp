// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MatineeModule.h"
#include "IMatinee.h"
#include "Matinee.h"
#include "Matinee/MatineeActor.h"

const FName MatineeAppIdentifier = FName(TEXT("MatineeApp"));


/*-----------------------------------------------------------------------------
   FMatineeModule
-----------------------------------------------------------------------------*/

class FMatineeModule : public IMatineeModule
{
public:
	
	/** Constructor, set up console commands and variables **/
	FMatineeModule()
	{
	}

	/** Called right after the module DLL has been loaded and the module object has been created */
	virtual void StartupModule() override
	{
		MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
		ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
	}

	/** Called before the module is unloaded, right before the module object is destroyed. */
	virtual void ShutdownModule() override
	{
		MenuExtensibilityManager.Reset();
		ToolBarExtensibilityManager.Reset();
	}

	DECLARE_DERIVED_EVENT(FMatineeModule, IMatineeModule::FMatineeEditorOpenedEvent, FMatineeEditorOpenedEvent);
	virtual FMatineeEditorOpenedEvent& OnMatineeEditorOpened() override
	{
		return MatineeEditorOpenedEvent;
	}

	virtual TSharedRef<IMatinee> CreateMatinee(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, AMatineeActor* MatineeActor) override
	{
		TSharedRef<FMatinee> MatineeEd(new FMatinee());
		MatineeEd->InitMatinee(Mode, InitToolkitHost, MatineeActor);
		MatineeEditorOpenedEvent.Broadcast();
		return MatineeEd;
	}

	/** Gets the extensibility managers for outside entities to extend matinee editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	FMatineeEditorOpenedEvent MatineeEditorOpenedEvent;
};

IMPLEMENT_MODULE(FMatineeModule, Matinee);
