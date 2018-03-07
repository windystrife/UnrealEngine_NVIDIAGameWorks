// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoundUtilitiesEditorModule.h"
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_SoundSimple.h"
#include "AssetTypeActions_Base.h"
#include "AudioEditorModule.h"


IMPLEMENT_MODULE(FSoundUtilitiesEditorModule, FSynthesisEditor)

void FSoundUtilitiesEditorModule::StartupModule()
{
	SoundWaveAssetActionExtender = TSharedPtr<ISoundWaveAssetActionExtensions>(new FSoundWaveAssetActionExtender());

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Now that we've loaded this module, we need to register our effect preset actions
	IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>("AudioEditor");
	AudioEditorModule->AddSoundWaveActionExtender(SoundWaveAssetActionExtender);

	// Register asset actions
	AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundSimple));
}

void FSoundUtilitiesEditorModule::ShutdownModule()
{
}

