// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SynthesisEditorModule.h"
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "SynthComponents/EpicSynth1Component.h"
#include "Factories/Factory.h"
#include "AssetTypeActions_Base.h"
#include "AudioEditorModule.h"
#include "EpicSynth1PresetBank.h"

IMPLEMENT_MODULE(FSynthesisEditorModule, FSynthesisEditor)

void FSynthesisEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_ModularSynthPresetBank));

	// Now that we've loaded this module, we need to register our effect preset actions
	IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>("AudioEditor");
	AudioEditorModule->RegisterEffectPresetAssetActions();
}

void FSynthesisEditorModule::ShutdownModule()
{
}


