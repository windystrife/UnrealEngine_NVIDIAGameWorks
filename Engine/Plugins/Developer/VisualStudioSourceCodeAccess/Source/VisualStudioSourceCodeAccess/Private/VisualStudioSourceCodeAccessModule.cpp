// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VisualStudioSourceCodeAccessModule.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE( FVisualStudioSourceCodeAccessModule, VisualStudioSourceCodeAccess );

#define LOCTEXT_NAMESPACE "VisualStudioSourceCodeAccessor"

FVisualStudioSourceCodeAccessModule::FVisualStudioSourceCodeAccessModule()
	: VisualStudioSourceCodeAccessor(MakeShareable(new FVisualStudioSourceCodeAccessor()))
{
}

void FVisualStudioSourceCodeAccessModule::StartupModule()
{
	VisualStudioSourceCodeAccessor->Startup();

	// Add all the explicit version wrappers. If one of these is selected, UBT will generate project files in the appropriate format. Editor behavior is still to detect which version to use
	// from the solution on disk.
	RegisterWrapper("VisualStudio2017", LOCTEXT("VisualStudio2017", "Visual Studio 2017"), LOCTEXT("UsingVisualStudio2017", "Open source code files in Visual Studio 2017"));
	RegisterWrapper("VisualStudio2015", LOCTEXT("VisualStudio2015", "Visual Studio 2015"), LOCTEXT("UsingVisualStudio2015", "Open source code files in Visual Studio 2015"));

	// Bind our source control provider to the editor
	IModularFeatures::Get().RegisterModularFeature(TEXT("SourceCodeAccessor"), &VisualStudioSourceCodeAccessor.Get() );
}

void FVisualStudioSourceCodeAccessModule::ShutdownModule()
{
	// unbind all the explicit version wrappers
	for (TSharedRef<FVisualStudioSourceCodeAccessorWrapper> &Wrapper : Wrappers)
	{
		IModularFeatures::Get().UnregisterModularFeature(TEXT("SourceCodeAccessor"), &Wrapper.Get());
	}

	// unbind provider from editor
	IModularFeatures::Get().UnregisterModularFeature(TEXT("SourceCodeAccessor"), &VisualStudioSourceCodeAccessor.Get());

	VisualStudioSourceCodeAccessor->Shutdown();
}

FVisualStudioSourceCodeAccessor& FVisualStudioSourceCodeAccessModule::GetAccessor()
{
	return VisualStudioSourceCodeAccessor.Get();
}

void FVisualStudioSourceCodeAccessModule::RegisterWrapper(FName Name, FText NameText, FText DescriptionText)
{
	TSharedRef<FVisualStudioSourceCodeAccessorWrapper> Wrapper = MakeShareable(new FVisualStudioSourceCodeAccessorWrapper(Name, NameText, DescriptionText, VisualStudioSourceCodeAccessor));
	Wrappers.Add(Wrapper);
	IModularFeatures::Get().RegisterModularFeature(TEXT("SourceCodeAccessor"), &Wrapper.Get());
}

#undef LOCTEXT_NAMESPACE