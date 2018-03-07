// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NullSourceCodeAccessModule.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"

IMPLEMENT_MODULE( FNullSourceCodeAccessModule, NullSourceCodeAccess );

void FNullSourceCodeAccessModule::StartupModule()
{
	// Bind our source control provider to the editor
	IModularFeatures::Get().RegisterModularFeature(TEXT("SourceCodeAccessor"), &NullSourceCodeAccessor );
}

void FNullSourceCodeAccessModule::ShutdownModule()
{
	// unbind provider from editor
	IModularFeatures::Get().UnregisterModularFeature(TEXT("SourceCodeAccessor"), &NullSourceCodeAccessor);
}

FNullSourceCodeAccessor& FNullSourceCodeAccessModule::GetAccessor()
{
	return NullSourceCodeAccessor;
}
