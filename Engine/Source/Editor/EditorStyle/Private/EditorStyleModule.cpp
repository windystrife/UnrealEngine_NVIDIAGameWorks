// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IEditorStyleModule.h"
#include "SlateEditorStyle.h"


/**
 * Implements the Editor style module, loaded by SlateApplication dynamically at startup.
 */
class FEditorStyleModule
	: public IEditorStyleModule
{
public:

	// IEditorStyleModule interface

	virtual void StartupModule( ) override
	{
		FSlateEditorStyle::Initialize();
	}

	virtual void ShutdownModule( ) override
	{
		FSlateEditorStyle::Shutdown();
	}

	virtual TSharedRef<class FSlateStyleSet> CreateEditorStyleInstance( ) const override
	{
		return FSlateEditorStyle::Create(FSlateEditorStyle::Settings);
	}

	// End IModuleInterface interface
};


IMPLEMENT_MODULE(FEditorStyleModule, EditorStyle)
