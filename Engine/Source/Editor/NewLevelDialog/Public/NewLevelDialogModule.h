// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Modules/ModuleInterface.h"

/**
 * New Level Dialog module
 */
class FNewLevelDialogModule : public IModuleInterface
{
public:

	/**
	 * Called right after the plugin DLL has been loaded and the plugin object has been created
	 */
	virtual void StartupModule();

	/**
	 * Called before the plugin is unloaded, right before the plugin object is destroyed.
	 */
	virtual void ShutdownModule();

	/**
	 * Creates and show a window with an SNewLevelDialog
	 * 
	 * @param	ParentWidget - The parent widget for the modal window showing the dialog
	 * @param	OutTemplateName	- (out) The package name of the template map selected by the user. Empty if blank map selected.
	 * @return	true if the user selected a valid item, false if the user canceled
	 */
	virtual bool CreateAndShowNewLevelDialog( const TSharedPtr<const SWidget> ParentWidget, FString& OutTemplateMapPackageName );

	/** New Level Dialog app identifier string */
	static const FName NewLevelDialogAppIdentifier;

};
