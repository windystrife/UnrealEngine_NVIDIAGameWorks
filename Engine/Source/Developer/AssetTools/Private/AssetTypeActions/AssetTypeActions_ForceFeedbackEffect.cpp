// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_ForceFeedbackEffect.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "GenericPlatform/IInputInterface.h"
#include "AssetData.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_ForceFeedbackEffect::GetSupportedClass() const
{
	return UForceFeedbackEffect::StaticClass();
}

void FAssetTypeActions_ForceFeedbackEffect::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	TArray<TWeakObjectPtr<UForceFeedbackEffect>> Effects = GetTypedWeakObjectPtrs<UForceFeedbackEffect>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ForceFeedbackEffect_PlayEffect", "Play"),
		LOCTEXT("ForceFeedbackEffect_PlayEffectTooltip", "Plays the selected force feedback effect."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "MediaAsset.AssetActions.Play.Small"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_ForceFeedbackEffect::ExecutePlayEffect, Effects ),
			FCanExecuteAction::CreateSP( this, &FAssetTypeActions_ForceFeedbackEffect::CanExecutePlayCommand, Effects )
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ForceFeedbackEffect_StopEffect", "Stop"),
		LOCTEXT("ForceFeedbackEffect_StopEffectTooltip", "Stops the selected force feedback effect."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "MediaAsset.AssetActions.Stop.Small"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_ForceFeedbackEffect::ExecuteStopEffect, Effects ),
			FCanExecuteAction()
			)
		);
}

bool FAssetTypeActions_ForceFeedbackEffect::IsEffectPlaying(const TArray<TWeakObjectPtr<UForceFeedbackEffect>>& Objects) const
{
	for (const TWeakObjectPtr<UForceFeedbackEffect>& EffectPtr : Objects)
	{
		UForceFeedbackEffect* Effect = EffectPtr.Get();
		if (Effect && PreviewForceFeedbackEffect.ForceFeedbackEffect == Effect)
		{
			return true;
		}
	}

	return false;
}

bool FAssetTypeActions_ForceFeedbackEffect::CanExecutePlayCommand(TArray<TWeakObjectPtr<UForceFeedbackEffect>> Objects) const
{
	return Objects.Num() == 1;
}

void FAssetTypeActions_ForceFeedbackEffect::AssetsActivated( const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType )
{
	if (ActivationType == EAssetTypeActivationMethod::Previewed)
	{
		for (UObject* Object : InObjects)
		{
			UForceFeedbackEffect* TargetEffect = Cast<UForceFeedbackEffect>(Object);
			if ( TargetEffect )
			{
				// Only target the first valid sound cue
				TArray<TWeakObjectPtr<UForceFeedbackEffect>> EffectList;
				EffectList.Add(TWeakObjectPtr<UForceFeedbackEffect>(TargetEffect));
				if (IsEffectPlaying(EffectList))
				{
					ExecuteStopEffect(EffectList);
				}
				else
				{
					ExecutePlayEffect(EffectList);
				}

				break;
			}
		}
	}
	else
	{
		FAssetTypeActions_Base::AssetsActivated(InObjects, ActivationType);
	}
}

void FAssetTypeActions_ForceFeedbackEffect::ExecutePlayEffect(TArray<TWeakObjectPtr<UForceFeedbackEffect>> Objects)
{
	for (const TWeakObjectPtr<UForceFeedbackEffect>& EffectPtr : Objects)
	{
		UForceFeedbackEffect* Effect = EffectPtr.Get();
		if (Effect)
		{
			// Only play the first valid effect
			PlayEffect(Effect);
			break;
		}
	}
}

void FAssetTypeActions_ForceFeedbackEffect::ExecuteStopEffect(TArray<TWeakObjectPtr<UForceFeedbackEffect>> Objects)
{
	StopEffect();
}

void FAssetTypeActions_ForceFeedbackEffect::PlayEffect(UForceFeedbackEffect* Effect)
{
	if (Effect)
	{
		PreviewForceFeedbackEffect.ForceFeedbackEffect = Effect;
		PreviewForceFeedbackEffect.PlayTime = 0.f;
	}
	else
	{
		StopEffect();
	}
}

void FAssetTypeActions_ForceFeedbackEffect::StopEffect() 
{
	PreviewForceFeedbackEffect.ForceFeedbackEffect = nullptr;

	IInputInterface* InputInterface = FSlateApplication::Get().GetInputInterface();
	if (InputInterface)
	{
		InputInterface->SetForceFeedbackChannelValues(0, FForceFeedbackValues());
	}
}

TSharedPtr<SWidget> FAssetTypeActions_ForceFeedbackEffect::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	TArray<TWeakObjectPtr<UForceFeedbackEffect>> EffectList;
	EffectList.Add(TWeakObjectPtr<UForceFeedbackEffect>((UForceFeedbackEffect*)AssetData.GetAsset()));

	auto OnGetDisplayBrushLambda = [this, EffectList]() -> const FSlateBrush*
	{
		if (IsEffectPlaying(EffectList))
		{
			return FEditorStyle::GetBrush("MediaAsset.AssetActions.Stop.Large");
		}

		return FEditorStyle::GetBrush("MediaAsset.AssetActions.Play.Large");
	};

	auto IsEnabledLambda = [this, EffectList]() -> bool
	{
		return CanExecutePlayCommand(EffectList);
	};

	FAssetTypeActions_ForceFeedbackEffect* MutableThis = const_cast<FAssetTypeActions_ForceFeedbackEffect*>(this);
	auto OnClickedLambda = [MutableThis, EffectList]() -> FReply
	{
		if (MutableThis->IsEffectPlaying(EffectList))
		{
			MutableThis->ExecuteStopEffect(EffectList);
		}
		else
		{
			MutableThis->ExecutePlayEffect(EffectList);
		}
		return FReply::Handled();
	};

	auto OnToolTipTextLambda = [this, EffectList]() -> FText
	{
		if (IsEffectPlaying(EffectList))
		{
			return LOCTEXT("Thumbnail_StopForceFeedbackToolTip", "Stop selected force feedback effect");
		}

		return LOCTEXT("Thumbnail_PlayForceFeedbackToolTip", "Play selected force feedback effect");
	};

	TSharedRef<SBox> Box = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2));

	auto OnGetVisibilityLambda = [this, Box, EffectList]() -> EVisibility
	{
		if (Box->IsHovered() || IsEffectPlaying(EffectList))
		{
			return EVisibility::Visible;
		}

		return EVisibility::Hidden;
	};

	TSharedRef<SButton> BoxContent = SNew(SButton)
		.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
		.ToolTipText_Lambda(OnToolTipTextLambda)
		.Cursor(EMouseCursor::Default) // The outer widget can specify a DragHand cursor, so we need to override that here
		.ForegroundColor(FSlateColor::UseForeground())
		.IsFocusable(false)
		.IsEnabled_Lambda(IsEnabledLambda)
		.OnClicked_Lambda(OnClickedLambda)
		.Visibility_Lambda(OnGetVisibilityLambda)
		[
			SNew(SBox)
			.MinDesiredWidth(16)
			.MinDesiredHeight(16)
			[
				SNew(SImage)
				.Image_Lambda(OnGetDisplayBrushLambda)
			]
		];

	Box->SetContent(BoxContent);
	Box->SetVisibility(EVisibility::Visible);

	return Box;
}

bool FPreviewForceFeedbackEffect::IsTickable() const
{
	return (ForceFeedbackEffect != nullptr);
}

void FPreviewForceFeedbackEffect::Tick( float DeltaTime )
{
	FForceFeedbackValues ForceFeedbackValues;

	if (!Update(DeltaTime, ForceFeedbackValues))
	{
		ForceFeedbackEffect = nullptr;
	}

	IInputInterface* InputInterface = FSlateApplication::Get().GetInputInterface();
	if (InputInterface)
	{
		InputInterface->SetForceFeedbackChannelValues(0, ForceFeedbackValues);
	}
}

TStatId FPreviewForceFeedbackEffect::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FPreviewForceFeedbackEffect, STATGROUP_Tickables);
}

void FPreviewForceFeedbackEffect::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(ForceFeedbackEffect);
}

#undef LOCTEXT_NAMESPACE
