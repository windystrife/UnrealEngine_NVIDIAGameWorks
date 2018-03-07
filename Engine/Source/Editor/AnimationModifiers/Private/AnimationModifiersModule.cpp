// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationModifiersModule.h"
#include "IAssetTools.h"

#include "Animation/AnimSequence.h"
#include "AnimationModifier.h"

#include "SAnimationModifiersTab.h"
#include "AnimationModifierDetailCustomization.h"
#include "AnimationModifiersTabSummoner.h"

#include "ModuleManager.h"
#include "PropertyEditorModule.h" 

#define LOCTEXT_NAMESPACE "AnimationModifiersModule"

IMPLEMENT_MODULE(FAnimationModifiersModule, AnimationModifiers);

void FAnimationModifiersModule::StartupModule()
{
	// Register class/struct customizations
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.RegisterCustomClassLayout("AnimationModifier", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimationModifierDetailCustomization::MakeInstance));
	
	// Add application mode extender
	Extender = FWorkflowApplicationModeExtender::CreateRaw(this, &FAnimationModifiersModule::ExtendApplicationMode);
	FWorkflowCentricApplication::GetModeExtenderList().Add(Extender);
}

TSharedRef<FApplicationMode> FAnimationModifiersModule::ExtendApplicationMode(const FName ModeName, TSharedRef<FApplicationMode> InMode)
{
	// For skeleton and animation editor modes add our custom tab factory to it
	if (ModeName == TEXT("SkeletonEditorMode") || ModeName == TEXT("AnimationEditorMode"))
	{
		InMode->AddTabFactory(FCreateWorkflowTabFactory::CreateStatic(&FAnimationModifiersTabSummoner::CreateFactory));
		RegisteredApplicationModes.Add(InMode);
	}
	
	return InMode;
}

void FAnimationModifiersModule::ShutdownModule()
{
	// Make sure we unregister the class layout 
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.UnregisterCustomClassLayout("AnimationModifier");

	// Remove extender delegate
	FWorkflowCentricApplication::GetModeExtenderList().RemoveAll([this](FWorkflowApplicationModeExtender& StoredExtender) { return StoredExtender.GetHandle() == Extender.GetHandle(); });

	// During shutdown clean up all factories from any modes which are still active/alive
	for (TWeakPtr<FApplicationMode> WeakMode : RegisteredApplicationModes)
	{
		if (WeakMode.IsValid())
		{
			TSharedPtr<FApplicationMode> Mode = WeakMode.Pin();
			Mode->RemoveTabFactory(FAnimationModifiersTabSummoner::AnimationModifiersName);
		}
	}

	RegisteredApplicationModes.Empty();
}

#undef LOCTEXT_NAMESPACE // "AnimationModifiersModule"