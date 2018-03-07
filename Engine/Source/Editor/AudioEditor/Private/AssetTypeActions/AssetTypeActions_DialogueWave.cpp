// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_DialogueWave.h"
#include "Misc/PackageName.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Editor.h"
#include "EditorStyleSet.h"
#include "Factories/SoundCueFactoryNew.h"
#include "Sound/SoundCue.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_DialogueWave::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto DialogueWaves = GetTypedWeakObjectPtrs<UDialogueWave>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Sound_PlaySound", "Play"),
		LOCTEXT("Sound_PlaySoundTooltip", "Plays the selected sound."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "MediaAsset.AssetActions.Play.Small"),
		FUIAction(
		FExecuteAction::CreateSP(this, &FAssetTypeActions_DialogueWave::ExecutePlaySound, DialogueWaves),
		FCanExecuteAction::CreateSP(this, &FAssetTypeActions_DialogueWave::CanExecutePlayCommand, DialogueWaves)
		)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Sound_StopSound", "Stop"),
		LOCTEXT("Sound_StopSoundTooltip", "Stops the selected sounds."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "MediaAsset.AssetActions.Stop.Small"),
		FUIAction(
		FExecuteAction::CreateSP(this, &FAssetTypeActions_DialogueWave::ExecuteStopSound, DialogueWaves),
		FCanExecuteAction()
		)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("DialogueWave_CreateCue", "Create Cue"),
		LOCTEXT("DialogueWave_CreateCueTooltip", "Creates a sound cue using this dialogue wave."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.SoundCue"),
		FUIAction(
		FExecuteAction::CreateSP(this, &FAssetTypeActions_DialogueWave::ExecuteCreateSoundCue, DialogueWaves),
		FCanExecuteAction()
		)
		);
}

void FAssetTypeActions_DialogueWave::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto DialogueWave = Cast<UDialogueWave>(*ObjIt);
		if (DialogueWave != NULL)
		{
			FSimpleAssetEditor::CreateEditor(Mode, Mode == EToolkitMode::WorldCentric ? EditWithinLevelEditor : TSharedPtr<IToolkitHost>(), DialogueWave);
		}
	}
}

bool FAssetTypeActions_DialogueWave::CanExecutePlayCommand(TArray<TWeakObjectPtr<UDialogueWave>> Objects) const
{
	if (Objects.Num() != 1)
	{
		return false;
	}

	USoundBase* Sound = nullptr;

	auto DialogueWave = Objects[0].Get();
	for (int32 i = 0; i < DialogueWave->ContextMappings.Num(); ++i)
	{
		const FDialogueContextMapping& ContextMapping = DialogueWave->ContextMappings[i];

		Sound = DialogueWave->GetWaveFromContext(ContextMapping.Context);
		if (Sound != nullptr)
		{
			break;
		}
	}

	return Sound != nullptr;
}

void FAssetTypeActions_DialogueWave::ExecutePlaySound(TArray<TWeakObjectPtr<UDialogueWave>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UDialogueWave* DialogueWave = (*ObjIt).Get();
		if (DialogueWave)
		{
			// Only play the first valid sound
			PlaySound(DialogueWave);
			break;
		}
	}
}

void FAssetTypeActions_DialogueWave::ExecuteStopSound(TArray<TWeakObjectPtr<UDialogueWave>> Objects)
{
	StopSound();
}

void FAssetTypeActions_DialogueWave::PlaySound(UDialogueWave* DialogueWave)
{
	USoundBase* Sound = nullptr;

	for (int32 i = 0; i < DialogueWave->ContextMappings.Num(); ++i)
	{
		const FDialogueContextMapping& ContextMapping = DialogueWave->ContextMappings[i];

		Sound = DialogueWave->GetWaveFromContext(ContextMapping.Context);
		if (Sound != nullptr)
		{
			break;
		}
	}

	if (Sound)
	{
		GEditor->PlayPreviewSound(Sound);
	}
	else
	{
		StopSound();
	}
}

void FAssetTypeActions_DialogueWave::StopSound()
{
	GEditor->ResetPreviewAudioComponent();
}

void FAssetTypeActions_DialogueWave::ExecuteCreateSoundCue(TArray<TWeakObjectPtr<UDialogueWave>> Objects)
{
	const FString DefaultSuffix = TEXT("_Cue");

	if (Objects.Num() == 1)
	{
		auto Object = Objects[0].Get();

		if (Object)
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
			Factory->InitialDialogueWave = Object;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USoundCue::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;

		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if (Object)
			{
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
				Factory->InitialDialogueWave = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USoundCue::StaticClass(), Factory);

				if (NewAsset)
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if (ObjectsToSync.Num() > 0)
		{
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

#undef LOCTEXT_NAMESPACE
