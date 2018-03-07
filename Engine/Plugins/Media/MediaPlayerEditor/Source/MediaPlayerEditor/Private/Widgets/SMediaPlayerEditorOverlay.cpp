// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SMediaPlayerEditorOverlay.h"

#include "IMediaOverlaySample.h"
#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Text/SRichTextBlock.h"


/* SMediaPlayerEditorOverlay interface
 *****************************************************************************/

void SMediaPlayerEditorOverlay::Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer)
{
	MediaPlayer = &InMediaPlayer;

	ChildSlot
	[
		SAssignNew(Canvas, SConstraintCanvas)
	];
}


/* SWidget interface
 *****************************************************************************/

void SMediaPlayerEditorOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	Canvas->ClearChildren();

	// fetch overlay samples
	TArray<TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe>> OverlaySamples;
	// @todo gmp: MF3.0 implement overlay sample caching
//	MediaPlayer->GetPlayerFacade()->GetOverlaySamples(MediaPlayer->GetTime(), OverlaySamples);

	if (OverlaySamples.Num() == 0)
	{
		return;
	}

	// create widgets for overlays
	for (const auto& Sample : OverlaySamples)
	{
		auto RichTextBlock = SNew(SRichTextBlock)
			.AutoWrapText(true)
			.Justification(ETextJustify::Center)
			.Text(Sample->GetText());

		const TOptional<FVector2D> Position = Sample->GetPosition();

		if (Position.IsSet())
		{
			Canvas->AddSlot()
				.Alignment(FVector2D(0.0f, 0.0f))
				.Anchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f))
				.AutoSize(true)
				.Offset(FMargin(Position->X, Position->Y, 0.0f, 0.0f))
				[
					RichTextBlock
				];
		}
		else
		{
			Canvas->AddSlot()
				.Alignment(FVector2D(0.0f, 1.0f))
				.Anchors(FAnchors(0.1f, 0.8f, 0.9f, 0.9f))
				.AutoSize(true)
				[
					RichTextBlock
				];
		}
	}

	// recalculate layout
	SlatePrepass(AllottedGeometry.GetAccumulatedLayoutTransform().GetScale());
}
