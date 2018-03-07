// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundBase.h"
#include "Sound/SoundBase.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Editor.h"
#include "EditorStyleSet.h"
#include "Components/AudioComponent.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundBase::GetSupportedClass() const
{
	return USoundBase::StaticClass();
}

void FAssetTypeActions_SoundBase::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto Sounds = GetTypedWeakObjectPtrs<USoundBase>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Sound_PlaySound", "Play"),
		LOCTEXT("Sound_PlaySoundTooltip", "Plays the selected sound."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "MediaAsset.AssetActions.Play.Small"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_SoundBase::ExecutePlaySound, Sounds ),
			FCanExecuteAction::CreateSP( this, &FAssetTypeActions_SoundBase::CanExecutePlayCommand, Sounds )
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Sound_StopSound", "Stop"),
		LOCTEXT("Sound_StopSoundTooltip", "Stops the selected sounds."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "MediaAsset.AssetActions.Stop.Small"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_SoundBase::ExecuteStopSound, Sounds ),
			FCanExecuteAction()
			)
		);
}

void FAssetTypeActions_SoundBase::AssetsActivated( const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType )
{
	if (ActivationType == EAssetTypeActivationMethod::Previewed)
	{
		USoundBase* TargetSound = NULL;

		for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			TargetSound = Cast<USoundBase>(*ObjIt);
			if ( TargetSound )
			{
				// Only target the first valid sound cue
				break;
			}
		}

		UAudioComponent* PreviewComp = GEditor->GetPreviewAudioComponent();
		if ( PreviewComp && PreviewComp->IsPlaying() )
		{
			// Already previewing a sound, if it is the target cue then stop it, otherwise play the new one
			if ( !TargetSound || PreviewComp->Sound == TargetSound )
			{
				StopSound();
			}
			else
			{
				PlaySound(TargetSound);
			}
		}
		else
		{
			// Not already playing, play the target sound cue if it exists
			PlaySound(TargetSound);
		}
	}
	else
	{
		FAssetTypeActions_Base::AssetsActivated(InObjects, ActivationType);
	}
}

void FAssetTypeActions_SoundBase::PlaySound(USoundBase* Sound) const
{
	if ( Sound )
	{
		GEditor->PlayPreviewSound(Sound);
	}
	else
	{
		StopSound();
	}
}

void FAssetTypeActions_SoundBase::StopSound() const
{
	GEditor->ResetPreviewAudioComponent();
}

bool FAssetTypeActions_SoundBase::IsSoundPlaying(USoundBase* Sound) const
{
	UAudioComponent* PreviewComp = GEditor->GetPreviewAudioComponent();
	return PreviewComp && PreviewComp->Sound == Sound && PreviewComp->IsPlaying();
}

void FAssetTypeActions_SoundBase::ExecutePlaySound(TArray<TWeakObjectPtr<USoundBase>> Objects) const
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		USoundBase* Sound = (*ObjIt).Get();
		if (Sound)
		{
			// Only play the first valid sound
			PlaySound(Sound);
			break;
		}
	}
}

void FAssetTypeActions_SoundBase::ExecuteStopSound(TArray<TWeakObjectPtr<USoundBase>> Objects) const
{
	StopSound();
}

TSharedPtr<SWidget> FAssetTypeActions_SoundBase::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	auto OnGetDisplayBrushLambda = [this, AssetData]() -> const FSlateBrush*
	{
		UObject* Asset = AssetData.GetAsset();
		if (Asset)
		{
			USoundBase* Sound = CastChecked<USoundBase>(Asset);
			if (IsSoundPlaying(Sound))
			{
				return FEditorStyle::GetBrush("MediaAsset.AssetActions.Stop.Large");
			}
		}

		return FEditorStyle::GetBrush("MediaAsset.AssetActions.Play.Large");
	};

	auto OnClickedLambda = [this, AssetData]() -> FReply
	{
		UObject* Asset = AssetData.GetAsset();
		if (Asset)
		{
			USoundBase* Sound = CastChecked<USoundBase>(Asset);

			if (IsSoundPlaying(Sound))
			{
				StopSound();
			}
			else
			{
				PlaySound(Sound);
			}
		}
		return FReply::Handled();
	};

	auto OnToolTipTextLambda = [this, AssetData]() -> FText
	{
		UObject* Asset = AssetData.GetAsset();
		if (Asset)
		{
			USoundBase* Sound = CastChecked<USoundBase>(Asset);
			if (IsSoundPlaying(Sound))
			{
				return LOCTEXT("Thumbnail_StopSoundToolTip", "Stop selected sound");
			}
		}

		return LOCTEXT("Thumbnail_PlaySoundToolTip", "Play selected sound");
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

bool FAssetTypeActions_SoundBase::CanExecutePlayCommand(TArray<TWeakObjectPtr<USoundBase>> Objects) const
{
	return Objects.Num() == 1;
}

#undef LOCTEXT_NAMESPACE
