// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/AttachTrackEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "SequencerSectionPainter.h"
#include "GameFramework/WorldSettings.h"
#include "Tracks/MovieScene3DAttachTrack.h"
#include "Sections/MovieScene3DAttachSection.h"
#include "ActorEditorUtils.h"


#define LOCTEXT_NAMESPACE "F3DAttachTrackEditor"

/**
 * Class that draws an attach section in the sequencer
 */
class F3DAttachSection
	: public ISequencerSection
{
public:

	F3DAttachSection( UMovieSceneSection& InSection, F3DAttachTrackEditor* InAttachTrackEditor )
		: Section( InSection )
		, AttachTrackEditor(InAttachTrackEditor)
	{ }

	/** ISequencerSection interface */
	virtual UMovieSceneSection* GetSectionObject() override
	{ 
		return &Section;
	}

	virtual FText GetSectionTitle() const override 
	{ 
		UMovieScene3DAttachSection* AttachSection = Cast<UMovieScene3DAttachSection>(&Section);
		if (AttachSection)
		{
			TSharedPtr<ISequencer> Sequencer = AttachTrackEditor->GetSequencer();
			if (Sequencer.IsValid())
			{
				TArrayView<TWeakObjectPtr<UObject>> RuntimeObjects = Sequencer->FindBoundObjects(AttachSection->GetConstraintId(), Sequencer->GetFocusedTemplateID());
				if (RuntimeObjects.Num() == 1 && RuntimeObjects[0].IsValid())
				{
					if (AActor* Actor = Cast<AActor>(RuntimeObjects[0].Get()))
					{
						if (AttachSection->AttachSocketName.IsNone())
						{
							return FText::FromString(Actor->GetActorLabel());
						}
						else
						{
							return FText::Format(LOCTEXT("SectionTitleFormat", "{0} ({1})"), FText::FromString(Actor->GetActorLabel()), FText::FromName(AttachSection->AttachSocketName));
						}
					}
				}
			}
		}

		return FText::GetEmpty(); 
	}

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override {}

	virtual int32 OnPaintSection( FSequencerSectionPainter& InPainter ) const override 
	{
		return InPainter.PaintSectionBackground();
	}
	
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding) override
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("SetAttach", "Attach"), LOCTEXT("SetAttachTooltip", "Set attach"),
			FNewMenuDelegate::CreateRaw(AttachTrackEditor, &FActorPickerTrackEditor::ShowActorSubMenu, ObjectBinding, &Section));
	}

private:

	/** The section we are visualizing */
	UMovieSceneSection& Section;

	/** The attach track editor */
	F3DAttachTrackEditor* AttachTrackEditor;
};


F3DAttachTrackEditor::F3DAttachTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FActorPickerTrackEditor( InSequencer ) 
{
}


F3DAttachTrackEditor::~F3DAttachTrackEditor()
{
}


TSharedRef<ISequencerTrackEditor> F3DAttachTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F3DAttachTrackEditor( InSequencer ) );
}


bool F3DAttachTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	// We support animatable transforms
	return Type == UMovieScene3DAttachTrack::StaticClass();
}


TSharedRef<ISequencerSection> F3DAttachTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );

	return MakeShareable( new F3DAttachSection( SectionObject, this ) );
}


void F3DAttachTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		UMovieSceneSection* DummySection = nullptr;

		MenuBuilder.AddSubMenu(
			LOCTEXT("AddAttach", "Attach"), LOCTEXT("AddAttachTooltip", "Adds an attach track."),
			FNewMenuDelegate::CreateRaw(this, &FActorPickerTrackEditor::ShowActorSubMenu, ObjectBinding, DummySection));
	}
}


bool F3DAttachTrackEditor::IsActorPickable(const AActor* const ParentActor, FGuid ObjectBinding, UMovieSceneSection* InSection)
{
	// Can't pick the object that this track binds
	TArrayView<TWeakObjectPtr<>> Objects = GetSequencer()->FindObjectsInCurrentSequence(ObjectBinding);
	if (Objects.Contains(ParentActor))
	{
		return false;
	}

	for (auto Object : Objects)
	{
		if (Object.IsValid())
		{
			AActor* ChildActor = Cast<AActor>(Object.Get());
			if (ChildActor)
			{
				USceneComponent* ChildRoot = ChildActor->GetRootComponent();
				USceneComponent* ParentRoot = ParentActor->GetDefaultAttachComponent();

				if (!ChildRoot || !ParentRoot || ParentRoot->IsAttachedTo(ChildRoot))
				{
					return false;
				}
			}
		}
	}

	if (ParentActor->IsListedInSceneOutliner() &&
		!FActorEditorUtils::IsABuilderBrush(ParentActor) &&
		!ParentActor->IsA( AWorldSettings::StaticClass() ) &&
		!ParentActor->IsPendingKill())
	{			
		return true;
	}
	return false;
}


void F3DAttachTrackEditor::ActorSocketPicked(const FName SocketName, USceneComponent* Component, AActor* ParentActor, FGuid ObjectGuid, UMovieSceneSection* Section)
{
	if (Section != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("UndoSetAttach", "Set Attach"));

		UMovieScene3DAttachSection* AttachSection = (UMovieScene3DAttachSection*)(Section);
		FGuid ActorId = FindOrCreateHandleToObject(ParentActor).Handle;

		if (ActorId.IsValid())
		{
			AttachSection->SetConstraintId(ActorId);
			AttachSection->AttachSocketName = SocketName;
			AttachSection->AttachComponentName = Component ? Component->GetFName() : NAME_None;
		}
	}
	else if (ObjectGuid.IsValid())
	{
		TArray<TWeakObjectPtr<>> OutObjects;
		for (TWeakObjectPtr<> Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
		{
			OutObjects.Add(Object);
		}

		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &F3DAttachTrackEditor::AddKeyInternal, OutObjects, SocketName, Component ? Component->GetFName() : NAME_None, ParentActor) );
	}
}

FKeyPropertyResult F3DAttachTrackEditor::AddKeyInternal( float KeyTime, const TArray<TWeakObjectPtr<UObject>> Objects, const FName SocketName, const FName ComponentName, AActor* ParentActor)
{
	FKeyPropertyResult KeyPropertyResult;

	FGuid ParentActorId;

	if (ParentActor != nullptr)
	{
		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(ParentActor);
		ParentActorId = HandleResult.Handle;
		KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;
	}

	if (!ParentActorId.IsValid())
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
			FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject( ObjectHandle, UMovieScene3DAttachTrack::StaticClass());
			UMovieSceneTrack* Track = TrackResult.Track;
			KeyPropertyResult.bTrackCreated |= TrackResult.bWasCreated;

			if (ensure(Track))
			{
				// Clamp to next attach section's start time or the end of the current sequencer view range
				float AttachEndTime = GetSequencer()->GetViewRange().GetUpperBoundValue();

				for (int32 AttachSectionIndex = 0; AttachSectionIndex < Track->GetAllSections().Num(); ++AttachSectionIndex)
				{
					float StartTime = Track->GetAllSections()[AttachSectionIndex]->GetStartTime();
					float EndTime = Track->GetAllSections()[AttachSectionIndex]->GetEndTime();
					if (KeyTime < StartTime)
					{
						if (AttachEndTime > StartTime)
						{
							AttachEndTime = StartTime;
						}
					}
				}

				Cast<UMovieScene3DAttachTrack>(Track)->AddConstraint( KeyTime, AttachEndTime, SocketName, ComponentName, ParentActorId );
				KeyPropertyResult.bTrackModified = true;
			}
		}
	}

	return KeyPropertyResult;
}


#undef LOCTEXT_NAMESPACE
