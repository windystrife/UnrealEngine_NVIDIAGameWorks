// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaThumbnailSection.h"

#include "EditorStyleSet.h"
#include "ISequencer.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "MovieSceneMediaSection.h"
#include "Rendering/DrawElements.h"
#include "SequencerSectionPainter.h"
#include "TrackEditorThumbnail/TrackEditorThumbnailPool.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "FMediaThumbnailSection"


/* FMediaSection structors
 *****************************************************************************/

FMediaThumbnailSection::FMediaThumbnailSection(UMovieSceneMediaSection& InSection, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, TSharedPtr<ISequencer> InSequencer)
	: FThumbnailSection(InSequencer, InThumbnailPool, this, InSection)
{
	MediaPlayer = nullptr;
	MediaTexture = nullptr;
	TimeSpace = ETimeSpace::Local;
}


FMediaThumbnailSection::~FMediaThumbnailSection()
{
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->Close();
	}
}


/* FGCObject interface
 *****************************************************************************/

void FMediaThumbnailSection::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MediaPlayer);
	Collector.AddReferencedObject(MediaTexture);
}


/* FThumbnailSection interface
 *****************************************************************************/

FText FMediaThumbnailSection::GetSectionTitle() const
{
	if (UMediaSource* MediaSource = Section ? CastChecked<UMovieSceneMediaSection>(Section)->GetMediaSource() : nullptr)
	{
		return FText::FromString(MediaSource->GetName());
	}

	return FText::GetEmpty();
}


void FMediaThumbnailSection::SetSingleTime(float GlobalTime)
{
	UMovieSceneMediaSection* MediaSection = CastChecked<UMovieSceneMediaSection>(Section);
	
	if (MediaSection)
	{
		MediaSection->SetThumbnailReferenceOffset(GlobalTime - MediaSection->GetStartTime());
	}
}


TSharedRef<SWidget> FMediaThumbnailSection::GenerateSectionWidget()
{
	return SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(GetContentPadding())
		[
			SNew(STextBlock)
				.Text(this, &FMediaThumbnailSection::GetSectionTitle)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
		];
}


float FMediaThumbnailSection::GetSectionHeight() const
{
	return FThumbnailSection::GetSectionHeight() + 2 * 9.0f; // make space for the film border
}


FMargin FMediaThumbnailSection::GetContentPadding() const
{
	return FMargin(8.0f, 15.0f);
}


void FMediaThumbnailSection::Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);

	if (MediaSection != nullptr)
	{
		if (GetDefault<UMovieSceneUserThumbnailSettings>()->bDrawSingleThumbnails)
		{
			ThumbnailCache.SetSingleReferenceFrame(MediaSection->GetThumbnailReferenceOffset());
		}
		else
		{
			ThumbnailCache.SetSingleReferenceFrame(TOptional<float>());
		}
	}

	FThumbnailSection::Tick(AllottedGeometry, ClippedGeometry, InCurrentTime, InDeltaTime);
}


int32 FMediaThumbnailSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	static const FSlateBrush* FilmBorder = FEditorStyle::GetBrush("Sequencer.Section.FilmBorder");

	InPainter.LayerId = InPainter.PaintSectionBackground();

	FVector2D LocalSectionSize = InPainter.SectionGeometry.GetLocalSize();

	FSlateClippingZone ClippingZone(InPainter.SectionClippingRect.InsetBy(FMargin(1.0f)));
	InPainter.DrawElements.PushClip(ClippingZone);

	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(LocalSectionSize.X - 2.0f, 7.0f), FSlateLayoutTransform(FVector2D(1.0f, 4.0f))),
		FilmBorder,
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);

	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(LocalSectionSize.X - 2.0f, 7.0f), FSlateLayoutTransform(FVector2D(1.0f, LocalSectionSize.Y - 11.0f))),
		FilmBorder,
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);

	InPainter.DrawElements.PopClip();

	int32 LayerId = FThumbnailSection::OnPaintSection(InPainter);

	// @todo gmp: draw media cache state

	++LayerId;

	return LayerId;
}


/* ICustomThumbnailClient interface
 *****************************************************************************/

void FMediaThumbnailSection::Draw(FTrackEditorThumbnail& TrackEditorThumbnail)
{
	if ((MediaTexture == nullptr) || (MediaTexture->Resource == nullptr) || !MediaTexture->Resource->TextureRHI.IsValid())
	{
		return;
	}

	// get target texture resource
	FTexture2DRHIRef Texture2DRHI = MediaTexture->Resource->TextureRHI->GetTexture2D();

	if (!Texture2DRHI.IsValid())
	{
		return;
	}

	// seek media player to thumbnail position
	float VideoTime = FMath::Max(0.0f, TrackEditorThumbnail.GetEvalPosition());

	if (!MediaPlayer->Seek(FTimespan(int64(ETimespan::TicksPerSecond * VideoTime))))
	{
		return;
	}

	// resolve media player texture to track editor thumbnail
	TrackEditorThumbnail.CopyTextureIn(Texture2DRHI);

	TSharedPtr<ISequencer> Sequencer = SequencerPtr.Pin();

	if (Sequencer.IsValid())
	{
		TrackEditorThumbnail.SetupFade(Sequencer->GetSequencerWidget());
	}
}


void FMediaThumbnailSection::Setup()
{
	UMovieSceneMediaSection* MediaSection = CastChecked<UMovieSceneMediaSection>(Section);
	UMediaSource* MediaSource = MediaSection->GetMediaSource();

	if (!MediaSource)
	{
		return;
	}

	// create internal player
	if (MediaPlayer == nullptr)
	{
		MediaPlayer = NewObject<UMediaPlayer>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UMediaPlayer::StaticClass()));
	}

	// create target texture
	if (MediaTexture == nullptr)
	{
		MediaTexture = NewObject<UMediaTexture>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UMediaTexture::StaticClass()));
		MediaTexture->MediaPlayer = MediaPlayer;
		MediaTexture->UpdateResource();
	}

	// open latest media source
	if (MediaPlayer->GetUrl() != MediaSource->GetUrl())
	{
		if (MediaPlayer->CanPlaySource(MediaSource))
		{
			MediaPlayer->OpenSource(MediaSource);
		}
	}

	MediaPlayer->Pause();
}


#undef LOCTEXT_NAMESPACE
