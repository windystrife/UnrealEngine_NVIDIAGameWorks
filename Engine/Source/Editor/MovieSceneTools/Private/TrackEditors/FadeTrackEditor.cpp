// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/FadeTrackEditor.h"
#include "Rendering/DrawElements.h"
#include "SequencerSectionPainter.h"
#include "EditorStyleSet.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "Sections/MovieSceneFadeSection.h"
#include "Sections/FloatPropertySection.h"
#include "CommonMovieSceneTools.h"

#define LOCTEXT_NAMESPACE "FFadeTrackEditor"

/**
 * Class for fade sections handles drawing of fade gradient
 */
class FFadeSection
	: public FFloatPropertySection
{
public:

	/** Constructor. */
	FFadeSection(UMovieSceneSection& InSectionObject, const FText& SectionName) : FFloatPropertySection(InSectionObject, SectionName) {}

	/** Virtual destructor. */
	virtual ~FFadeSection() {}

public:

	virtual int32 OnPaintSection( FSequencerSectionPainter& Painter ) const override
	{
		int32 LayerId = Painter.PaintSectionBackground();

		const ESlateDrawEffect DrawEffects = Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		const UMovieSceneFadeSection* FadeSection = Cast<const UMovieSceneFadeSection>( &SectionObject );

		const FTimeToPixel& TimeConverter = Painter.GetTimeConverter();

		const float StartTime = TimeConverter.PixelToTime(0.f);
		const float EndTime = TimeConverter.PixelToTime(Painter.SectionGeometry.GetLocalSize().X);
		const float SectionDuration = EndTime - StartTime;

		if ( !FMath::IsNearlyZero( SectionDuration ) )
		{
			FVector2D GradientSize = FVector2D( Painter.SectionGeometry.Size.X - 2.f, Painter.SectionGeometry.Size.Y - 3.0f );

			FPaintGeometry PaintGeometry = Painter.SectionGeometry.ToPaintGeometry( FVector2D( 1.f, 3.f ), GradientSize );

			TArray<FSlateGradientStop> GradientStops;

			TArray< TKeyValuePair<float, FLinearColor> > FadeKeys;

			TArray<float> TimesWithKeys;
			for ( auto It( FadeSection->GetFloatCurve().GetKeyIterator() ); It; ++It )
			{
				float Time = It->Time;
				TimesWithKeys.Add(Time);
			}

			// Enforce at least one key for the default value
			if (TimesWithKeys.Num() == 0)
			{
				TimesWithKeys.Add(0);
			}

			for (auto Time : TimesWithKeys)
			{
				float Value = FadeSection->Eval(Time, 0.f);
			
				FLinearColor Color = FadeSection->FadeColor;
				Color.A = Value*255.f;

				float TimeFraction = (Time - StartTime) / SectionDuration;

				if (TimeFraction > 0)
				{
					GradientStops.Add( FSlateGradientStop( FVector2D( TimeFraction * Painter.SectionGeometry.Size.X, 0 ),
						Color ) );
				}
			}

			if ( GradientStops.Num() > 0 )
			{
				FSlateDrawElement::MakeGradient(
					Painter.DrawElements,
					Painter.LayerId + 1,
					PaintGeometry,
					GradientStops,
					Orient_Vertical,
					DrawEffects
					);
			}
		}

		return LayerId + 1;
	}
};


/* FFadeTrackEditor static functions
 *****************************************************************************/

TSharedRef<ISequencerTrackEditor> FFadeTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FFadeTrackEditor(InSequencer));
}


/* FFadeTrackEditor structors
 *****************************************************************************/

FFadeTrackEditor::FFadeTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FFloatPropertyTrackEditor(InSequencer)
{ }

/* ISequencerTrackEditor interface
 *****************************************************************************/

TSharedRef<ISequencerSection> FFadeTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	return MakeShareable(new FFadeSection(SectionObject, Track.GetDisplayName()));
}

void FFadeTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddFadeTrack", "Fade Track"),
		LOCTEXT("AddFadeTrackTooltip", "Adds a new track that controls the fade of the sequence."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Tracks.Fade"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FFadeTrackEditor::HandleAddFadeTrackMenuEntryExecute)
		)
	);
}

bool FFadeTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return (InSequence != nullptr) && (InSequence->GetClass()->GetName() == TEXT("LevelSequence"));
}

bool FFadeTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneFadeTrack::StaticClass());
}

const FSlateBrush* FFadeTrackEditor::GetIconBrush() const
{
	return FEditorStyle::GetBrush("Sequencer.Tracks.Fade");
}


/* FFadeTrackEditor callbacks
 *****************************************************************************/

void FFadeTrackEditor::HandleAddFadeTrackMenuEntryExecute()
{
	UMovieScene* MovieScene = GetFocusedMovieScene();

	if (MovieScene == nullptr)
	{
		return;
	}

	UMovieSceneTrack* FadeTrack = MovieScene->FindMasterTrack<UMovieSceneFadeTrack>();

	if (FadeTrack != nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddFadeTrack_Transaction", "Add Fade Track"));

	MovieScene->Modify();

	FadeTrack = FindOrCreateMasterTrack<UMovieSceneFadeTrack>().Track;
	check(FadeTrack);

	UMovieSceneSection* NewSection = FadeTrack->CreateNewSection();
	check(NewSection);

	FadeTrack->AddSection(*NewSection);

	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}


#undef LOCTEXT_NAMESPACE
