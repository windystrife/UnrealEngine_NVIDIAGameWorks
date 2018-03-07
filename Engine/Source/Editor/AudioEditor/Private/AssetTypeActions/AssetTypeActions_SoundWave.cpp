// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundWave.h"
#include "Factories/DialogueWaveFactory.h"
#include "Misc/PackageName.h"
#include "AssetData.h"
#include "Sound/SoundWave.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "Factories/SoundCueFactoryNew.h"
#include "EditorFramework/AssetImportData.h"
#include "Sound/DialogueVoice.h"
#include "Sound/DialogueWave.h"
#include "Sound/SoundCue.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "AudioEditorModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundWave::GetSupportedClass() const
{
	return USoundWave::StaticClass();
}

void FAssetTypeActions_SoundWave::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	FAssetTypeActions_SoundBase::GetActions(InObjects, MenuBuilder);

	TArray<TWeakObjectPtr<USoundWave>> SoundNodes = GetTypedWeakObjectPtrs<USoundWave>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SoundWave_CreateCue", "Create Cue"),
		LOCTEXT("SoundWave_CreateCueTooltip", "Creates a sound cue using this sound wave."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.SoundCue"),
		FUIAction(
		FExecuteAction::CreateSP( this, &FAssetTypeActions_SoundWave::ExecuteCreateSoundCue, SoundNodes ),
		FCanExecuteAction()
			)
		);

	MenuBuilder.AddSubMenu(
		LOCTEXT("SoundWave_CreateDialogue", "Create Dialogue"),
		LOCTEXT("SoundWave_CreateDialogueTooltip", "Creates a dialogue wave using this sound wave."),
		FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_SoundWave::FillVoiceMenu, SoundNodes));

	// Check for any sound wave asset action extensions
	IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>("AudioEditor");
	TArray<TSharedPtr<ISoundWaveAssetActionExtensions>> Extensions;
	AudioEditorModule->GetSoundWaveActionExtenders(Extensions);
	
	for (TSharedPtr<ISoundWaveAssetActionExtensions> Extension : Extensions)
	{
		Extension->GetExtendedActions(SoundNodes, MenuBuilder);
	}

}

void FAssetTypeActions_SoundWave::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto SoundWave = CastChecked<USoundWave>(Asset);
		SoundWave->AssetImportData->ExtractFilenames(OutSourceFilePaths);
	}
}

void FAssetTypeActions_SoundWave::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

void FAssetTypeActions_SoundWave::ExecuteCreateSoundCue(TArray<TWeakObjectPtr<USoundWave>> Objects)
{
	const FString DefaultSuffix = TEXT("_Cue");

	if ( Objects.Num() == 1 )
	{
		auto Object = Objects[0].Get();

		if ( Object )
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
			Factory->InitialSoundWave = Object;

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
			if ( Object )
			{
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
				Factory->InitialSoundWave = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USoundCue::StaticClass(), Factory);

				if ( NewAsset )
				{
					ObjectsToSync.Add(NewAsset);
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

void FAssetTypeActions_SoundWave::ExecuteCreateDialogueWave(const struct FAssetData& AssetData, TArray<TWeakObjectPtr<USoundWave>> Objects)
{
	const FString DefaultSuffix = TEXT("_Dialogue");

	UDialogueVoice* DialogueVoice = Cast<UDialogueVoice>(AssetData.GetAsset());

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
			UDialogueWaveFactory* Factory = NewObject<UDialogueWaveFactory>();
			Factory->InitialSoundWave = Object;
			Factory->InitialSpeakerVoice = DialogueVoice;
			Factory->HasSetInitialTargetVoice = true;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), UDialogueWave::StaticClass(), Factory);
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
				UDialogueWaveFactory* Factory = NewObject<UDialogueWaveFactory>();
				Factory->InitialSoundWave = Object;
				Factory->InitialSpeakerVoice = DialogueVoice;
				Factory->HasSetInitialTargetVoice = true;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UDialogueWave::StaticClass(), Factory);

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

void FAssetTypeActions_SoundWave::FillVoiceMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USoundWave>> Objects)
{
	TArray<const UClass*> AllowedClasses;
	AllowedClasses.Add(UDialogueVoice::StaticClass());

	TSharedRef<SWidget> VoicePicker = PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
		FAssetData(),
		false, 
		AllowedClasses,
		PropertyCustomizationHelpers::GetNewAssetFactoriesForClasses(AllowedClasses),
		FOnShouldFilterAsset(),
		FOnAssetSelected::CreateSP(this, &FAssetTypeActions_SoundWave::ExecuteCreateDialogueWave, Objects),
		FSimpleDelegate());

	MenuBuilder.AddWidget(VoicePicker, FText::GetEmpty(), false);
}

TSharedPtr<SWidget> FAssetTypeActions_SoundWave::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	auto OnGetDisplayBrushLambda = [this, AssetData]() -> const FSlateBrush*
	{
		UObject* Asset = AssetData.GetAsset();
		if (Asset)
		{
			USoundBase* Sound = CastChecked<USoundBase>(AssetData.GetAsset());

			if (IsSoundPlaying(Sound))
			{
				return FEditorStyle::GetBrush("MediaAsset.AssetActions.Stop.Large");
			}
		}

		return FEditorStyle::GetBrush("MediaAsset.AssetActions.Play.Large");
	};

	auto OnClickedLambda = [this, AssetData]() -> FReply
	{
		USoundBase* Sound = CastChecked<USoundBase>(AssetData.GetAsset());
		if (IsSoundPlaying(Sound))
		{
			StopSound();
		}
		else
		{
			PlaySound(Sound);
		}
		return FReply::Handled();
	};

	auto OnToolTipTextLambda = [this, AssetData]() -> FText
	{
		USoundBase* Sound = CastChecked<USoundBase>(AssetData.GetAsset());
		if (IsSoundPlaying(Sound))
		{
			return LOCTEXT("Blueprint_StopSoundToolTip", "Stop selected Sound Wave");
		}

		return LOCTEXT("Blueprint_PlaySoundToolTip", "Play selected Sound Wave");
	};

	TSharedPtr<SBox> Box;
	SAssignNew(Box, SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2));

	auto OnGetVisibilityLambda = [this, Box, AssetData]() -> EVisibility
	{
		UObject* Asset = AssetData.GetAsset();
		if (Asset)
		{
			USoundBase* Sound = CastChecked<USoundBase>(Asset);
			if (Box.IsValid())
			{
				if (Box->IsHovered() || IsSoundPlaying(Sound))
				{
					return EVisibility::Visible;
				}

			}
		}

		return EVisibility::Hidden;
	};

	TSharedPtr<SButton> Widget;
	SAssignNew(Widget, SButton)
		.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
		.ToolTipText_Lambda(OnToolTipTextLambda)
		.Cursor(EMouseCursor::Default) // The outer widget can specify a DragHand cursor, so we need to override that here
		.ForegroundColor(FSlateColor::UseForeground())
		.IsFocusable(false)
		.OnClicked_Lambda(OnClickedLambda)
		.Visibility_Lambda(OnGetVisibilityLambda)
		[
			SNew(SImage)
			.Image_Lambda(OnGetDisplayBrushLambda)
		];

	Box->SetContent(Widget.ToSharedRef());
	Box->SetVisibility(EVisibility::Visible);

	return Box;
}

#undef LOCTEXT_NAMESPACE
