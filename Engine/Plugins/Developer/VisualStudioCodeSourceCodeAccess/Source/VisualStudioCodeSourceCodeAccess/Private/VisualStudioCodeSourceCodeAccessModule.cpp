// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VisualStudioCodeSourceCodeAccessModule.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE( FVisualStudioCodeSourceCodeAccessModule, VisualStudioCodeSourceCodeAccess );

#define LOCTEXT_NAMESPACE "VisualStudioCodeSourceCodeAccessor"

FVisualStudioCodeSourceCodeAccessModule::FVisualStudioCodeSourceCodeAccessModule()
	: VisualStudioCodeSourceCodeAccessor(MakeShareable(new FVisualStudioCodeSourceCodeAccessor()))
{
}

void FVisualStudioCodeSourceCodeAccessModule::StartupModule()
{
	VisualStudioCodeSourceCodeAccessor->Startup();

	// Bind our source control provider to the editor
	IModularFeatures::Get().RegisterModularFeature(TEXT("SourceCodeAccessor"), &VisualStudioCodeSourceCodeAccessor.Get() );
}

void FVisualStudioCodeSourceCodeAccessModule::ShutdownModule()
{
	// unbind provider from editor
	IModularFeatures::Get().UnregisterModularFeature(TEXT("SourceCodeAccessor"), &VisualStudioCodeSourceCodeAccessor.Get());

	VisualStudioCodeSourceCodeAccessor->Shutdown();
}

FVisualStudioCodeSourceCodeAccessor& FVisualStudioCodeSourceCodeAccessModule::GetAccessor()
{
	return VisualStudioCodeSourceCodeAccessor.Get();
}

#undef LOCTEXT_NAMESPACE