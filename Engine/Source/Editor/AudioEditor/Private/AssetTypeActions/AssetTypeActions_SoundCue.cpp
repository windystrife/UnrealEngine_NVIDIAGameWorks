// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundCue.h"
#include "Sound/SoundAttenuation.h"
#include "Misc/PackageName.h"
#include "Sound/SoundCue.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Factories/SoundAttenuationFactory.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AudioEditorModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundCue::GetSupportedClass() const
{
	return USoundCue::StaticClass();
}

void FAssetTypeActions_SoundCue::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto SoundCues = GetTypedWeakObjectPtrs<USoundCue>(InObjects);
	FAssetTypeActions_SoundBase::GetActions(InObjects, MenuBuilder);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SoundCue_ConsolidateAttenuation", "Consolidate Attenuation"),
		LOCTEXT("SoundCue_ConsolidateAttenuationTooltip", "Creates shared attenuation packages for sound cues with identical override attenuation settings."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_SoundCue::ExecuteConsolidateAttenuation, SoundCues ),
			FCanExecuteAction::CreateSP( this, &FAssetTypeActions_SoundCue::CanExecuteConsolidateCommand, SoundCues )
			)
		);
}

void FAssetTypeActions_SoundCue::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto SoundCue = Cast<USoundCue>(*ObjIt);
		if (SoundCue != NULL)
		{
			IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>( "AudioEditor" );
			AudioEditorModule->CreateSoundCueEditor(Mode, EditWithinLevelEditor, SoundCue);
		}
	}
}

bool FAssetTypeActions_SoundCue::CanExecuteConsolidateCommand(TArray<TWeakObjectPtr<USoundCue>> Objects) const
{
	return Objects.Num() > 1;
}

void FAssetTypeActions_SoundCue::ExecuteConsolidateAttenuation(TArray<TWeakObjectPtr<USoundCue>> Objects)
{
	TMap<FSoundAttenuationSettings*,TArray<USoundCue*>> UnmatchedAttenuations;

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		USoundCue* SoundCue = (*ObjIt).Get();
		bool bFound = false;
		if ( SoundCue && SoundCue->bOverrideAttenuation )
		{
			for (auto UnmatchedIt = UnmatchedAttenuations.CreateIterator(); UnmatchedIt; ++UnmatchedIt)
			{
				// Found attenuation settings to consolidate together
				if (SoundCue->AttenuationOverrides == *UnmatchedIt.Key())
				{
					UnmatchedIt.Value().Add(SoundCue);
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				UnmatchedAttenuations.FindOrAdd(&SoundCue->AttenuationOverrides).Add(SoundCue);
			}
		}
	}

	if (UnmatchedAttenuations.Num() > 0)
	{
		FString DefaultSuffix;
		TArray<UObject*> ObjectsToSync;

		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		USoundAttenuationFactory* Factory = NewObject<USoundAttenuationFactory>();

		for (auto UnmatchedIt = UnmatchedAttenuations.CreateConstIterator(); UnmatchedIt; ++UnmatchedIt)
		{
			if (UnmatchedIt.Value().Num() > 1)
			{
				FString Name;
				FString PackageName;
				CreateUniqueAssetName("/Game/Sounds/SoundAttenuations/SharedAttenuation", DefaultSuffix, PackageName, Name);

				USoundAttenuation* SoundAttenuation = Cast<USoundAttenuation>(AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USoundAttenuation::StaticClass(), Factory));
				if (SoundAttenuation)
				{
					SoundAttenuation->Attenuation = *UnmatchedIt.Key();

					for (int32 SoundCueIndex = 0; SoundCueIndex < UnmatchedIt.Value().Num(); ++SoundCueIndex)
					{
						USoundCue* SoundCue = UnmatchedIt.Value()[SoundCueIndex];
						SoundCue->bOverrideAttenuation = false;
						SoundCue->AttenuationSettings = SoundAttenuation;
						SoundCue->MarkPackageDirty();
					}
				}
			}
		}

		if ( ObjectsToSync.Num() > 0 )
		{
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

#undef LOCTEXT_NAMESPACE
