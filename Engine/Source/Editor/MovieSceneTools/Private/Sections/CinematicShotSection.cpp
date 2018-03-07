// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/CinematicShotSection.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Rendering/DrawElements.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "MovieSceneTrack.h"
#include "MovieScene.h"
#include "TrackEditors/CinematicShotTrackEditor.h"
#include "SequencerSectionPainter.h"
#include "EditorStyleSet.h"
#include "MovieSceneToolHelpers.h"


#define LOCTEXT_NAMESPACE "FCinematicShotSection"


/* FCinematicShotSection structors
 *****************************************************************************/

FCinematicShotSection::FCinematicSectionCache::FCinematicSectionCache(UMovieSceneCinematicShotSection* Section)
	: ActualStartTime(0.f), TimeScale(0.f)
{
	if (Section)
	{
		ActualStartTime = Section->GetStartTime() - Section->Parameters.StartOffset;
		TimeScale = Section->Parameters.TimeScale;
	}
}


FCinematicShotSection::FCinematicShotSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection, TSharedPtr<FCinematicShotTrackEditor> InCinematicShotTrackEditor)
	: FViewportThumbnailSection(InSequencer, InThumbnailPool, InSection)
	, SectionObject(*CastChecked<UMovieSceneCinematicShotSection>(&InSection))
	, Sequencer(InSequencer)
	, CinematicShotTrackEditor(InCinematicShotTrackEditor)
	, InitialStartOffsetDuringResize(0.f)
	, InitialStartTimeDuringResize(0.f)
	, ThumbnailCacheData(&SectionObject)
{
	AdditionalDrawEffect = ESlateDrawEffect::NoGamma;
}


FCinematicShotSection::~FCinematicShotSection()
{
}

float FCinematicShotSection::GetSectionHeight() const
{
	return FViewportThumbnailSection::GetSectionHeight() + 2*9.f;
}

FMargin FCinematicShotSection::GetContentPadding() const
{
	return FMargin(8.f, 15.f);
}

void FCinematicShotSection::SetSingleTime(float GlobalTime)
{
	SectionObject.SetThumbnailReferenceOffset(GlobalTime - SectionObject.GetStartTime());
}

void FCinematicShotSection::BeginResizeSection()
{
	InitialStartOffsetDuringResize = SectionObject.Parameters.StartOffset;
	InitialStartTimeDuringResize = SectionObject.GetStartTime();
}

void FCinematicShotSection::ResizeSection(ESequencerSectionResizeMode ResizeMode, float ResizeTime)
{
	// Adjust the start offset when resizing from the beginning
	if (ResizeMode == SSRM_LeadingEdge)
	{
		float StartOffset = (ResizeTime - InitialStartTimeDuringResize) / SectionObject.Parameters.TimeScale;
		StartOffset += InitialStartOffsetDuringResize;

		// Ensure start offset is not less than 0
		StartOffset = FMath::Max(StartOffset, 0.f);

		SectionObject.Parameters.StartOffset = StartOffset;
	}

	FViewportThumbnailSection::ResizeSection(ResizeMode, ResizeTime);
}

void FCinematicShotSection::BeginSlipSection()
{
	BeginResizeSection();
}

void FCinematicShotSection::SlipSection(float SlipTime)
{
	float StartOffset = (SlipTime - InitialStartTimeDuringResize) / SectionObject.Parameters.TimeScale;
	StartOffset += InitialStartOffsetDuringResize;

	// Ensure start offset is not less than 0
	StartOffset = FMath::Max(StartOffset, 0.f);

	SectionObject.Parameters.StartOffset = StartOffset;

	FViewportThumbnailSection::SlipSection(SlipTime);
}

void FCinematicShotSection::Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Set cached data
	FCinematicSectionCache NewCacheData(&SectionObject);
	if (NewCacheData != ThumbnailCacheData)
	{
		ThumbnailCache.ForceRedraw();
	}
	ThumbnailCacheData = NewCacheData;

	// Update single reference frame settings
	if (GetDefault<UMovieSceneUserThumbnailSettings>()->bDrawSingleThumbnails)
	{
		ThumbnailCache.SetSingleReferenceFrame(SectionObject.GetStartTime() + SectionObject.GetThumbnailReferenceOffset());
	}
	else
	{
		ThumbnailCache.SetSingleReferenceFrame(TOptional<float>());
	}

	FViewportThumbnailSection::Tick(AllottedGeometry, ClippedGeometry, InCurrentTime, InDeltaTime);
}

int32 FCinematicShotSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	static const FSlateBrush* FilmBorder = FEditorStyle::GetBrush("Sequencer.Section.FilmBorder");

	InPainter.LayerId = InPainter.PaintSectionBackground();

	FVector2D LocalSectionSize = InPainter.SectionGeometry.GetLocalSize();

	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(LocalSectionSize.X-2.f, 7.f), FSlateLayoutTransform(FVector2D(1.f, 4.f))),
		FilmBorder,
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);

	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(LocalSectionSize.X-2.f, 7.f), FSlateLayoutTransform(FVector2D(1.f, LocalSectionSize.Y - 11.f))),
		FilmBorder,
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);

	const float SectionSize = SectionObject.GetTimeSize();

	if (SectionSize <= 0.0f)
	{
		return InPainter.LayerId;
	}

	FViewportThumbnailSection::OnPaintSection(InPainter);

	const float DrawScale = InPainter.SectionGeometry.Size.X / SectionSize;
		
	UMovieScene* MovieScene = nullptr;
	FFloatRange PlaybackRange;
	if(SectionObject.GetSequence() != nullptr)
	{
		MovieScene = SectionObject.GetSequence()->GetMovieScene();
		PlaybackRange = MovieScene->GetPlaybackRange();
	}
	else
	{
		UMovieSceneTrack* MovieSceneTrack = CastChecked<UMovieSceneTrack>(SectionObject.GetOuter());
		MovieScene = CastChecked<UMovieScene>(MovieSceneTrack->GetOuter());
		PlaybackRange = MovieScene->GetPlaybackRange();
	}

	// add box for the working size
	const float StartOffset = 1.0f/SectionObject.Parameters.TimeScale * SectionObject.Parameters.StartOffset;
	const float WorkingStart = -1.0f/SectionObject.Parameters.TimeScale * PlaybackRange.GetLowerBoundValue() - StartOffset;
	const float WorkingSize = 1.0f/SectionObject.Parameters.TimeScale * (MovieScene != nullptr ? MovieScene->GetEditorData().WorkingRange.Size<float>() : 1.0f);

	// add dark tint for left out-of-bounds & working range
	if (StartOffset < 0.0f)
	{
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(
				FVector2D(0.0f, 0.f),
				FVector2D(-StartOffset * DrawScale, InPainter.SectionGeometry.Size.Y)
			),
			FEditorStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FLinearColor::Black.CopyWithNewOpacity(0.5f)
		);
	}

	// add green line for playback start
	if (StartOffset < 0)
	{
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(
				FVector2D(-StartOffset * DrawScale, 0.f),
				FVector2D(1.0f, InPainter.SectionGeometry.Size.Y)
			),
			FEditorStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FColor(32, 128, 32)	// 120, 75, 50 (HSV)
		);
	}

	// add dark tint for right out-of-bounds & working range
	const float PlaybackEnd = 1.0f/SectionObject.Parameters.TimeScale * PlaybackRange.Size<float>() - StartOffset;

	if (PlaybackEnd < SectionSize)
	{
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(
				FVector2D(PlaybackEnd * DrawScale, 0.f),
				FVector2D((SectionSize - PlaybackEnd) * DrawScale, InPainter.SectionGeometry.Size.Y)
			),
			FEditorStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FLinearColor::Black.CopyWithNewOpacity(0.5f)
		);
	}

	// add red line for playback end
	if (PlaybackEnd <= SectionSize)
	{
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(
				FVector2D(PlaybackEnd * DrawScale, 0.f),
				FVector2D(1.0f, InPainter.SectionGeometry.Size.Y)
			),
			FEditorStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FColor(128, 32, 32)	// 0, 75, 50 (HSV)
		);
	}

	return InPainter.LayerId;
}

void FCinematicShotSection::BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding)
{
	FViewportThumbnailSection::BuildSectionContextMenu(MenuBuilder, ObjectBinding);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ShotMenuText", "Shot"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("TakesMenu", "Takes"),
			LOCTEXT("TakesMenuTooltip", "Shot takes"),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& InMenuBuilder){ AddTakesMenu(InMenuBuilder); }));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("NewTake", "New Take"),
			FText::Format(LOCTEXT("NewTakeTooltip", "Create a new take for {0}"), SectionObject.GetShotDisplayName()),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::NewTake, &SectionObject))
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("InsertNewShot", "Insert Shot"),
			LOCTEXT("InsertNewShotTooltip", "Insert a new shot at the current time"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::InsertShot))
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DuplicateShot", "Duplicate Shot"),
			FText::Format(LOCTEXT("DuplicateShotTooltip", "Duplicate {0} to create a new shot"), SectionObject.GetShotDisplayName()),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::DuplicateShot, &SectionObject))
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("RenderShot", "Render Shot"),
			FText::Format(LOCTEXT("RenderShotTooltip", "Render shot movie"), SectionObject.GetShotDisplayName()),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::RenderShot, &SectionObject))
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("RenameShot", "Rename Shot"),
			FText::Format(LOCTEXT("RenameShotTooltip", "Rename {0}"), SectionObject.GetShotDisplayName()),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FCinematicShotSection::EnterRename))
		);
	}
	MenuBuilder.EndSection();
}

void FCinematicShotSection::AddTakesMenu(FMenuBuilder& MenuBuilder)
{
	TArray<uint32> TakeNumbers;
	uint32 CurrentTakeNumber = INDEX_NONE;
	MovieSceneToolHelpers::GatherTakes(&SectionObject, TakeNumbers, CurrentTakeNumber);

	for (auto TakeNumber : TakeNumbers)
	{
		MenuBuilder.AddMenuEntry(
			FText::Format(LOCTEXT("TakeNumber", "Take {0}"), FText::AsNumber(TakeNumber)),
			FText::Format(LOCTEXT("TakeNumberTooltip", "Switch to take {0}"), FText::AsNumber(TakeNumber)),
			TakeNumber == CurrentTakeNumber ? FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Star") : FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Empty"),
			FUIAction(FExecuteAction::CreateSP(CinematicShotTrackEditor.Pin().ToSharedRef(), &FCinematicShotTrackEditor::SwitchTake, &SectionObject, TakeNumber))
		);
	}
}

/* FCinematicShotSection callbacks
 *****************************************************************************/

FText FCinematicShotSection::HandleThumbnailTextBlockText() const
{
	return SectionObject.GetShotDisplayName();
}


void FCinematicShotSection::HandleThumbnailTextBlockTextCommitted(const FText& NewShotName, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter && !HandleThumbnailTextBlockText().EqualTo(NewShotName))
	{
		SectionObject.Modify();

		const FScopedTransaction Transaction(LOCTEXT("SetShotName", "Set Shot Name"));
	
		SectionObject.SetShotDisplayName(NewShotName);
	}
}

FReply FCinematicShotSection::OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent)
{
	if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		if (SectionObject.GetSequence())
		{
			Sequencer.Pin()->FocusSequenceInstance(SectionObject);
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
