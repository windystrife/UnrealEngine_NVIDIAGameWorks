// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_SoundSubmix.h"
#include "Sound/SoundSubmix.h"
#include "AudioEditorModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundSubmix::GetSupportedClass() const
{
	return USoundSubmix::StaticClass();
}

void FAssetTypeActions_SoundSubmix::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		USoundSubmix* SoundSubmix = Cast<USoundSubmix>(*ObjIt);
		if (SoundSubmix != nullptr)
		{
			IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>( "AudioEditor" );
			AudioEditorModule->CreateSoundSubmixEditor(Mode, EditWithinLevelEditor, SoundSubmix);
		}
	}
}

#undef LOCTEXT_NAMESPACE