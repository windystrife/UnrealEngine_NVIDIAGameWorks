// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaPlayerEditorViewport.h"

#include "MediaPlayer.h"
#include "Misc/Paths.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"

#include "MediaPlayerEditorSettings.h"
#include "SMediaPlayerEditorOutput.h"
#include "SMediaPlayerEditorOverlay.h"


#define LOCTEXT_NAMESPACE "SMediaPlayerEditorViewport"


/* SMediaPlayerEditorPlayer structors
 *****************************************************************************/

SMediaPlayerEditorViewport::SMediaPlayerEditorViewport()
	: MediaPlayer(nullptr)
{ }


/* SMediaPlayerEditorPlayer interface
 *****************************************************************************/

void SMediaPlayerEditorViewport::Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer, const TSharedRef<ISlateStyle>& InStyle)
{
	MediaPlayer = &InMediaPlayer;
	Style = InStyle;

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
			[
				// movie viewport
				SNew(SScaleBox)
					.Stretch_Lambda([]() -> EStretch::Type {
						auto Settings = GetDefault<UMediaPlayerEditorSettings>();
						if (Settings->ViewportScale == EMediaPlayerEditorScale::Fill)
						{
							return EStretch::Fill;
						}
						if (Settings->ViewportScale == EMediaPlayerEditorScale::Fit)
						{
							return EStretch::ScaleToFit;
						}
						return EStretch::None;
					})
					[
						// movie texture
						SNew(SMediaPlayerEditorOutput, InMediaPlayer)
					]
			]

		+ SOverlay::Slot()
			[
				// subtitle & caption overlays
				SNew(SMediaPlayerEditorOverlay, InMediaPlayer)
					.Visibility_Lambda([]{ return GetDefault<UMediaPlayerEditorSettings>()->ShowTextOverlays ? EVisibility::Visible : EVisibility::Collapsed; })
			]

		+ SOverlay::Slot()
			.Padding(FMargin(12.0f, 8.0f))
			[
				// top info overlays
				SNew(SHorizontalBox)
					.Visibility_Lambda([this]() -> EVisibility {
						return (IsHovered() || HasMouseCapture()) ? EVisibility::Visible : EVisibility::Hidden;
					})

				+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					[
						// media source name
						SNew(STextBlock)
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18))
							.ShadowOffset(FVector2D(1.f, 1.f))
							.Text(this, &SMediaPlayerEditorViewport::HandleMediaSourceNameText)
							.ToolTipText(LOCTEXT("OverlaySourceNameTooltip", "Name of the currently opened media source"))
					]

				+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Top)
					[
						// player name
						SNew(STextBlock)
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18))
							.ShadowOffset(FVector2D(1.f, 1.f))
							.Text(this, &SMediaPlayerEditorViewport::HandleMediaPlayerNameText)
							.ToolTipText(LOCTEXT("OverlayPlayerNameTooltip", "Name of the currently used media player plug-in"))
					]
			]

		+ SOverlay::Slot()
			.Padding(FMargin(12.0f, 8.0f))
			.VAlign(VAlign_Bottom)
			[
				// bottom info overlays
				SNew(SHorizontalBox)
					.Visibility_Lambda([this]() -> EVisibility {
						return (IsHovered() || HasMouseCapture()) ? EVisibility::Visible : EVisibility::Hidden;
					})

				+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Bottom)
					[
						// playback state
						SNew(STextBlock)
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18))
							.ShadowOffset(FVector2D(1.f, 1.f))
							.Text(this, &SMediaPlayerEditorViewport::HandleMediaPlayerStateText)
							.ToolTipText(LOCTEXT("OverlayPlayerStateTooltip", "The media player's current state"))
					]

				+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					[
						// view settings
						SNew(STextBlock)
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18))
							.ShadowOffset(FVector2D(1.f, 1.f))
							.Text(this, &SMediaPlayerEditorViewport::HandleViewSettingsText)
							.ToolTipText(LOCTEXT("OverlayViewSettingsTooltip", "The current view settings"))
					]
			]
	];
}


/* SWidget interface
 *****************************************************************************/

FReply SMediaPlayerEditorViewport::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	SetCursor(EMouseCursor::CardinalCross);

	return FReply::Handled().CaptureMouse(SharedThis(this));
}


FReply SMediaPlayerEditorViewport::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	if (MediaPlayer->IsPlaying())
	{
		MediaPlayer->Pause();
	}
	else
	{
		MediaPlayer->Play();
	}

	return FReply::Handled();
}


FReply SMediaPlayerEditorViewport::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	SetCursor(EMouseCursor::Default);

	return FReply::Handled().ReleaseMouseCapture();
}


FReply SMediaPlayerEditorViewport::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!HasMouseCapture())
	{
		return FReply::Unhandled();
	}

	const FVector2D CursorDelta = MouseEvent.GetCursorDelta();
	MediaPlayer->SetViewRotation(FRotator::MakeFromEuler(FVector(CursorDelta.Y, CursorDelta.X, 0.0f)), false);

	return FReply::Handled();
}


FReply SMediaPlayerEditorViewport::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	MediaPlayer->SetViewField(MouseEvent.GetWheelDelta(), 0.0f, false);

	return FReply::Handled();
}


/* SMediaPlayerEditorViewport callbacks
 *****************************************************************************/

FText SMediaPlayerEditorViewport::HandleMediaPlayerNameText() const
{
	FName PlayerName = MediaPlayer->GetPlayerName();

	if ((PlayerName == NAME_None) || MediaPlayer->GetUrl().IsEmpty())
	{
		const FName DesiredPlayerName = MediaPlayer->GetDesiredPlayerName();

		if (DesiredPlayerName == NAME_None)
		{
			return LOCTEXT("AutoPlayerName", "Auto");
		}

		return FText::FromName(DesiredPlayerName);
	}

	return FText::FromName(PlayerName);
}


FText SMediaPlayerEditorViewport::HandleMediaPlayerStateText() const
{
	if (MediaPlayer->HasError())
	{
		return LOCTEXT("StateOverlayError", "Error");
	}

	if (MediaPlayer->IsBuffering())
	{
		return LOCTEXT("StateOverlayBuffering", "Buffering");
	}

	if (MediaPlayer->IsPreparing())
	{
		return LOCTEXT("StateOverlayPreparing", "Preparing");
	}

	if (!MediaPlayer->IsReady())
	{
		return LOCTEXT("StateOverlayStopped", "Not Ready");
	}

	if (MediaPlayer->IsPaused())
	{
		return LOCTEXT("StateOverlayPaused", "Paused");
	}

	if (MediaPlayer->IsPlaying())
	{
		float Rate = MediaPlayer->GetRate();

		if (Rate == 1.0f)
		{
			return LOCTEXT("StateOverlayPlaying", "Playing");
		}

		if (Rate < 0.0f)
		{
			return FText::Format(LOCTEXT("StateOverlayReverseFormat", "Reverse ({0}x)"), FText::AsNumber(-Rate));
		}

		return FText::Format(LOCTEXT("StateOverlayForwardFormat", "Forward ({0}x)"), FText::AsNumber(Rate));
	}

	return LOCTEXT("StateOverlayReady", "Ready");
}


FText SMediaPlayerEditorViewport::HandleMediaSourceNameText() const
{
	const FText MediaName = MediaPlayer->GetMediaName();

	if (MediaName.IsEmpty())
	{
		return LOCTEXT("StateOverlayNoMedia", "No Media");
	}

	return MediaName;
}


FText SMediaPlayerEditorViewport::HandleViewSettingsText() const
{
	FVector Euler = MediaPlayer->GetViewRotation().Euler();

	FNumberFormattingOptions NumberFormat;
	{
		NumberFormat.MaximumFractionalDigits = 0;
		NumberFormat.MinimumIntegralDigits = 3;
	}

	FFormatNamedArguments Args;
	{
		Args.Add(TEXT("P"), FText::AsNumber(Euler.Y, &NumberFormat));
		Args.Add(TEXT("Y"), FText::AsNumber(Euler.Z, &NumberFormat));
		Args.Add(TEXT("R"), FText::AsNumber(Euler.X, &NumberFormat));
		Args.Add(TEXT("H"), FText::AsNumber(MediaPlayer->GetHorizontalFieldOfView(), &NumberFormat));
		Args.Add(TEXT("V"), FText::AsNumber(MediaPlayer->GetVerticalFieldOfView(), &NumberFormat));
	}

	return FText::Format(LOCTEXT("ViewSettingsFormat", "R {P} {Y} {R} | FOV {H} {V}"), Args);
}


#undef LOCTEXT_NAMESPACE
