// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTrackEditor.h"
#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "ISequencer.h"
#include "Framework/Commands/UICommandList.h"
#include "ScopedTransaction.h"
#include "MovieSceneTrack.h"
#include "ISequencerTrackEditor.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "Sequencer.h"
#include "MultiBoxBuilder.h"
#include "SequencerUtilities.h"

FMovieSceneTrackEditor::FMovieSceneTrackEditor(TSharedRef<ISequencer> InSequencer)
	: Sequencer(InSequencer)
{ 
}

FMovieSceneTrackEditor::~FMovieSceneTrackEditor() 
{ 
}

UMovieSceneSequence* FMovieSceneTrackEditor::GetMovieSceneSequence() const
{
	return Sequencer.Pin()->GetFocusedMovieSceneSequence();
}

float FMovieSceneTrackEditor::GetTimeForKey()
{ 
	TSharedPtr<ISequencer> SequencerPin = Sequencer.Pin();
	return SequencerPin.IsValid() ? SequencerPin->GetLocalTime() : 0.f;
}

void FMovieSceneTrackEditor::UpdatePlaybackRange()
{
	TSharedPtr<ISequencer> SequencerPin = Sequencer.Pin();
	if( SequencerPin.IsValid()  )
	{
		SequencerPin->UpdatePlaybackRange();
	}
}

void FMovieSceneTrackEditor::AnimatablePropertyChanged( FOnKeyProperty OnKeyProperty )
{
	check(OnKeyProperty.IsBound());

	// Get the movie scene we want to autokey
	UMovieSceneSequence* MovieSceneSequence = GetMovieSceneSequence();
	if (MovieSceneSequence)
	{
		float KeyTime = GetTimeForKey();

		if( !Sequencer.Pin()->IsRecordingLive() )
		{
			// @todo Sequencer - The sequencer probably should have taken care of this
			MovieSceneSequence->SetFlags(RF_Transactional);
		
			// Create a transaction record because we are about to add keys
			const bool bShouldActuallyTransact = !GIsTransacting && !Sequencer.Pin()->IsRecordingLive();		// Don't transact if we're recording in a PIE world.  That type of keyframe capture cannot be undone.
			FScopedTransaction AutoKeyTransaction( NSLOCTEXT("AnimatablePropertyTool", "PropertyChanged", "Animatable Property Changed"), bShouldActuallyTransact );

			FKeyPropertyResult KeyPropertyResult = OnKeyProperty.Execute( KeyTime );

			if (KeyPropertyResult.bTrackCreated)
			{
				// If a track is created evaluate immediately so that the pre-animated state can be stored
				Sequencer.Pin()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::RefreshAllImmediately );
			}
			else if (KeyPropertyResult.bTrackModified)
			{
				Sequencer.Pin()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
			}

			UpdatePlaybackRange();

			TSharedPtr<FSequencer> SequencerToUpdate = StaticCastSharedPtr<FSequencer>(Sequencer.Pin());
			if (SequencerToUpdate.IsValid())
			{
				SequencerToUpdate->SynchronizeSequencerSelectionWithExternalSelection();
			}
		}
	}
}


FMovieSceneTrackEditor::FFindOrCreateHandleResult FMovieSceneTrackEditor::FindOrCreateHandleToObject( UObject* Object, bool bCreateHandleIfMissing )
{
	FFindOrCreateHandleResult Result;
	bool bHandleWasValid = GetSequencer()->GetHandleToObject( Object, false ).IsValid();
	Result.Handle = GetSequencer()->GetHandleToObject( Object, bCreateHandleIfMissing );
	Result.bWasCreated = bHandleWasValid == false && Result.Handle.IsValid();
	return Result;
}


FMovieSceneTrackEditor::FFindOrCreateTrackResult FMovieSceneTrackEditor::FindOrCreateTrackForObject( const FGuid& ObjectHandle, TSubclassOf<UMovieSceneTrack> TrackClass, FName PropertyName, bool bCreateTrackIfMissing )
{
	FFindOrCreateTrackResult Result;
	bool bTrackExisted;

	UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
	Result.Track = MovieScene->FindTrack( TrackClass, ObjectHandle, PropertyName );
	bTrackExisted = Result.Track != nullptr;

	if (!Result.Track && bCreateTrackIfMissing)
	{
		Result.Track = AddTrack(MovieScene, ObjectHandle, TrackClass, PropertyName);
	}

	Result.bWasCreated = bTrackExisted == false && Result.Track != nullptr;

	return Result;
}


const TSharedPtr<ISequencer> FMovieSceneTrackEditor::GetSequencer() const
{
	return Sequencer.Pin();
}

void FMovieSceneTrackEditor::AddKey( const FGuid& ObjectGuid )
{
}

UMovieSceneTrack* FMovieSceneTrackEditor::AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName)
{
	return FocusedMovieScene->AddTrack(TrackClass, ObjectHandle);
}

void FMovieSceneTrackEditor::BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings) 
{ 
}

void FMovieSceneTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder) 
{ 
}

void FMovieSceneTrackEditor::BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass) 
{ 
}

void FMovieSceneTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) 
{ 
}

TSharedPtr<SWidget> FMovieSceneTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) 
{
	if (Track->GetSupportedBlendTypes().Num() > 0)
	{
		TWeakPtr<ISequencer> WeakSequencer = GetSequencer();

		const int32 RowIndex = Params.TrackInsertRowIndex;
		auto SubMenuCallback = [=]() -> TSharedRef<SWidget>
		{
			FMenuBuilder MenuBuilder(true, nullptr);

			FSequencerUtilities::PopulateMenu_CreateNewSection(MenuBuilder, RowIndex, Track, WeakSequencer);

			return MenuBuilder.MakeWidget();
		};

		return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			FSequencerUtilities::MakeAddButton(NSLOCTEXT("MovieSceneTrackEditor", "AddSection", "Section"), FOnGetContent::CreateLambda(SubMenuCallback), Params.NodeIsHovered)
		];
	}
	else
	{
		return TSharedPtr<SWidget>(); 
	}
}

void FMovieSceneTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) 
{ 
}

bool FMovieSceneTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) 
{ 
	return false; 
}

bool FMovieSceneTrackEditor::OnAllowDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track)
{
	return false;
}

FReply FMovieSceneTrackEditor::OnDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track)
{
	return FReply::Unhandled();
}

void FMovieSceneTrackEditor::OnInitialize() 
{ 
}

void FMovieSceneTrackEditor::OnRelease() 
{ 
}

int32 FMovieSceneTrackEditor::PaintTrackArea(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle)
{
	return LayerId;
}

void FMovieSceneTrackEditor::Tick(float DeltaTime) 
{ 
}

UMovieScene* FMovieSceneTrackEditor::GetFocusedMovieScene() const
{
	UMovieSceneSequence* FocusedSequence = GetSequencer()->GetFocusedMovieSceneSequence();
	return FocusedSequence->GetMovieScene();
}
