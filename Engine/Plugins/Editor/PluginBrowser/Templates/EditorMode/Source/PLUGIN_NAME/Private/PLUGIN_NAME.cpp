// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PLUGIN_NAME.h"
#include "PLUGIN_NAMEEdMode.h"

#define LOCTEXT_NAMESPACE "FPLUGIN_NAMEModule"

void FPLUGIN_NAMEModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FEditorModeRegistry::Get().RegisterMode<FPLUGIN_NAMEEdMode>(FPLUGIN_NAMEEdMode::EM_PLUGIN_NAMEEdModeId, LOCTEXT("PLUGIN_NAMEEdModeName", "PLUGIN_NAMEEdMode"), FSlateIcon(), true);
}

void FPLUGIN_NAMEModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FEditorModeRegistry::Get().UnregisterMode(FPLUGIN_NAMEEdMode::EM_PLUGIN_NAMEEdModeId);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPLUGIN_NAMEModule, PLUGIN_NAME)