// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PathTrackEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "SequencerSectionPainter.h"
#include "GameFramework/WorldSettings.h"
#include "Tracks/MovieScene3DPathTrack.h"
#include "Sections/MovieScene3DPathSection.h"
#include "ISectionLayoutBuilder.h"
#include "ActorEditorUtils.h"
#include "Components/SplineComponent.h"
#include "FloatCurveKeyArea.h"


#define LOCTEXT_NAMESPACE "FPathTrackEditor"

/**
 * Class that draws a path section in the sequencer
 */
class F3DPathSection
	: public ISequencerSection
{
public:
	F3DPathSection( UMovieSceneSection& InSection, F3DPathTrackEditor* InPathTrackEditor )
		: Section( InSection )
		, PathTrackEditor(InPathTrackEditor)
	{ }

	/** ISequencerSection interface */
	virtual UMovieSceneSection* GetSectionObject() override
	{ 
		return &Section;
	}
	
	virtual FText GetSectionTitle() const override 
	{ 
		UMovieScene3DPathSection* PathSection = Cast<UMovieScene3DPathSection>(&Section);
		if (PathSection)
		{
			TSharedPtr<ISequencer> Sequencer = PathTrackEditor->GetSequencer();
			if (Sequencer.IsValid())
			{
				TArrayView<TWeakObjectPtr<UObject>> RuntimeObjects = Sequencer->FindBoundObjects(PathSection->GetConstraintId(), Sequencer->GetFocusedTemplateID());
				if (RuntimeObjects.Num() == 1 && RuntimeObjects[0].IsValid())
				{
					if (AActor* Actor = Cast<AActor>(RuntimeObjects[0].Get()))
					{
						return FText::FromString(Actor->GetActorLabel());
					}
				}
			}
		}

		return FText::GetEmpty(); 
	}

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override
	{
		UMovieScene3DPathSection* PathSection = Cast<UMovieScene3DPathSection>( &Section );

		LayoutBuilder.AddKeyArea("Timing", LOCTEXT("TimingArea", "Timing"), MakeShareable( new FFloatCurveKeyArea ( &PathSection->GetTimingCurve( ), PathSection ) ) );
	}

	virtual int32 OnPaintSection( FSequencerSectionPainter& InPainter ) const override 
	{
		return InPainter.PaintSectionBackground();
	}
	
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding) override
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("SetPath", "Path"), LOCTEXT("SetPathTooltip", "Set path"),
			FNewMenuDelegate::CreateRaw(PathTrackEditor, &FActorPickerTrackEditor::ShowActorSubMenu, ObjectBinding, &Section));
	}

private:

	/** The section we are visualizing */
	UMovieSceneSection& Section;

	/** The path track editor */
	F3DPathTrackEditor* PathTrackEditor;
};


F3DPathTrackEditor::F3DPathTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FActorPickerTrackEditor( InSequencer ) 
{ 
}


TSharedRef<ISequencerTrackEditor> F3DPathTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F3DPathTrackEditor( InSequencer ) );
}


bool F3DPathTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	// We support animatable transforms
	return Type == UMovieScene3DPathTrack::StaticClass();
}


TSharedRef<ISequencerSection> F3DPathTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );

	return MakeShareable( new F3DPathSection( SectionObject, this ) );
}


void F3DPathTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		UMovieSceneSection* DummySection = nullptr;

		MenuBuilder.AddSubMenu(
			LOCTEXT("AddPath", "Path"), LOCTEXT("AddPathTooltip", "Adds a path track."),
			FNewMenuDelegate::CreateRaw(this, &FActorPickerTrackEditor::ShowActorSubMenu, ObjectBinding, DummySection));
	}
}

bool F3DPathTrackEditor::IsActorPickable(const AActor* const ParentActor, FGuid ObjectBinding, UMovieSceneSection* InSection)
{
	// Can't pick the object that this track binds
	if (GetSequencer()->FindObjectsInCurrentSequence(ObjectBinding).Contains(ParentActor))
	{
		return false;
	}

	// Can't pick the object that this track attaches to
	UMovieScene3DPathSection* PathSection = Cast<UMovieScene3DPathSection>(InSection);
	if (PathSection != nullptr)
	{
		FGuid ConstraintId = PathSection->GetConstraintId();

		if (ConstraintId.IsValid())
		{
			if (GetSequencer()->FindObjectsInCurrentSequence(ConstraintId).Contains(ParentActor))
			{
				return false;
			}
		}
	}

	if (ParentActor->IsListedInSceneOutliner() &&
		!FActorEditorUtils::IsABuilderBrush(ParentActor) &&
		!ParentActor->IsA( AWorldSettings::StaticClass() ) &&
		!ParentActor->IsPendingKill())
	{			
		TArray<USplineComponent*> SplineComponents;
		ParentActor->GetComponents(SplineComponents);
		if (SplineComponents.Num())
		{
			return true;
		}
	}
	return false;
}


void F3DPathTrackEditor::ActorSocketPicked(const FName SocketName, USceneComponent* Component, AActor* ParentActor, FGuid ObjectGuid, UMovieSceneSection* Section)
{
	if (Section != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("UndoSetPath", "Set Path"));

		UMovieScene3DPathSection* PathSection = (UMovieScene3DPathSection*)(Section);
		FGuid SplineId = FindOrCreateHandleToObject(ParentActor).Handle;

		if (SplineId.IsValid())
		{
			PathSection->SetConstraintId(SplineId);
		}
	}
	else if (ObjectGuid.IsValid())
	{
		TArray<TWeakObjectPtr<>> OutObjects;
		for (TWeakObjectPtr<> Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
		{
			OutObjects.Add(Object);
		}
		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &F3DPathTrackEditor::AddKeyInternal, OutObjects, ParentActor) );
	}
}

FKeyPropertyResult F3DPathTrackEditor::AddKeyInternal( float KeyTime, const TArray<TWeakObjectPtr<UObject>> Objects, AActor* ParentActor)
{
	FKeyPropertyResult KeyPropertyResult;

	AActor* ActorWithSplineComponent = ParentActor;
	
	FGuid SplineId;

	if (ActorWithSplineComponent != nullptr)
	{
		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject( ActorWithSplineComponent );
		SplineId = HandleResult.Handle;
		KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;
	}

	if (!SplineId.IsValid())
	{
		return KeyPropertyResult;
	}

	for( int32 ObjectIndex = 0; ObjectIndex < Objects.Num(); ++ObjectIndex )
	{
		UObject* Object = Objects[ObjectIndex].Get();

		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject( Object );
		FGuid ObjectHandle = HandleResult.Handle;
		KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;
		if (ObjectHandle.IsValid())
		{
			FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(ObjectHandle, UMovieScene3DPathTrack::StaticClass());
			UMovieSceneTrack* Track = TrackResult.Track;
			KeyPropertyResult.bTrackCreated |= TrackResult.bWasCreated;

			if (ensure(Track))
			{
				// Clamp to next path section's start time or the end of the current sequencer view range
				float PathEndTime = GetSequencer()->GetViewRange().GetUpperBoundValue();
	
				for (int32 PathSectionIndex = 0; PathSectionIndex < Track->GetAllSections().Num(); ++PathSectionIndex)
				{
					float StartTime = Track->GetAllSections()[PathSectionIndex]->GetStartTime();
					float EndTime = Track->GetAllSections()[PathSectionIndex]->GetEndTime();
					if (KeyTime < StartTime)
					{
						if (PathEndTime > StartTime)
						{
							PathEndTime = StartTime;
						}
					}
				}

				Cast<UMovieScene3DPathTrack>(Track)->AddConstraint( KeyTime, PathEndTime, NAME_None, NAME_None, SplineId );
				KeyPropertyResult.bTrackModified = true;
			}
		}
	}

	return KeyPropertyResult;
}


#undef LOCTEXT_NAMESPACE
