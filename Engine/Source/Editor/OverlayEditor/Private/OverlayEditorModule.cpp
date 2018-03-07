// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "OverlayEditorPrivate.h"
#include "IOverlayEditorModule.h"

DEFINE_LOG_CATEGORY(LogOverlayEditor);

/**
 * Implements the OverlayEditor module
 */
class FOverlayEditorModule
	: public IOverlayEditorModule
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override { }

	virtual void ShutdownModule() override { }

	// End IModuleInterface interface
};

IMPLEMENT_MODULE(FOverlayEditorModule, OverlayEditor);