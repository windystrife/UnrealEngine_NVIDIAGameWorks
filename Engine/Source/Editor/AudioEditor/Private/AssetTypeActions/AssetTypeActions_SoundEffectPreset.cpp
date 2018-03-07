// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_SoundEffectPreset.h"
#include "Developer/AssetTools/Public/IAssetTools.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorManager.h"
#include "Toolkits/SimpleAssetEditor.h"

FAssetTypeActions_SoundEffectPreset::FAssetTypeActions_SoundEffectPreset(USoundEffectPreset* InEffectPreset)
	: EffectPreset(InEffectPreset)
{
}

FText FAssetTypeActions_SoundEffectPreset::GetName() const
{
	return EffectPreset->GetAssetActionName();
}

UClass* FAssetTypeActions_SoundEffectPreset::GetSupportedClass() const
{
	return EffectPreset->GetSupportedClass();
}
