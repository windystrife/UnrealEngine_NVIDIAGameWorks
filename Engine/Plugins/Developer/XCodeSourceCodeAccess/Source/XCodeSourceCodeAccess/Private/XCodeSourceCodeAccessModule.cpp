// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XCodeSourceCodeAccessModule.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE( FXCodeSourceCodeAccessModule, XCodeSourceCodeAccess );

void FXCodeSourceCodeAccessModule::StartupModule()
{
	// Bind our source control provider to the editor
	IModularFeatures::Get().RegisterModularFeature(TEXT("SourceCodeAccessor"), &XCodeSourceCodeAccessor );
}

void FXCodeSourceCodeAccessModule::ShutdownModule()
{
	// unbind provider from editor
	IModularFeatures::Get().UnregisterModularFeature(TEXT("SourceCodeAccessor"), &XCodeSourceCodeAccessor);
}