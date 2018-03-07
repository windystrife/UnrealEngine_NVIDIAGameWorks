// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/SlomoTrackEditor.h"
#include "EditorStyleSet.h"
#include "Tracks/MovieSceneSlomoTrack.h"


#define LOCTEXT_NAMESPACE "FSlomoTrackEditor"


/* FSlomoTrackEditor static functions
 *****************************************************************************/

TSharedRef<ISequencerTrackEditor> FSlomoTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FSlomoTrackEditor(InSequencer));
}


/* FSlomoTrackEditor structors
 *****************************************************************************/

FSlomoTrackEditor::FSlomoTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FFloatPropertyTrackEditor(InSequencer)
{ }


/* ISequencerTrackEditor interface
 *****************************************************************************/

void FSlomoTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddPlayRateTrack", "Play Rate Track"),
		LOCTEXT("AddPlayRateTrackTooltip", "Adds a new track that controls the playback rate of the sequence."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Tracks.Slomo"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FSlomoTrackEditor::HandleAddSlomoTrackMenuEntryExecute)
		)
	);
}

bool FSlomoTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return (InSequence != nullptr) && (InSequence->GetClass()->GetName() == TEXT("LevelSequence"));
}

bool FSlomoTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneSlomoTrack::StaticClass());
}

const FSlateBrush* FSlomoTrackEditor::GetIconBrush() const
{
	return FEditorStyle::GetBrush("Sequencer.Tracks.Slomo");
}


/* FSlomoTrackEditor callbacks
 *****************************************************************************/

void FSlomoTrackEditor::HandleAddSlomoTrackMenuEntryExecute()
{
	UMovieScene* MovieScene = GetFocusedMovieScene();

	if (MovieScene == nullptr)
	{
		return;
	}

	UMovieSceneTrack* SlomoTrack = MovieScene->FindMasterTrack<UMovieSceneSlomoTrack>();

	if (SlomoTrack != nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddSlomoTrack_Transaction", "Add Play Rate Track"));

	MovieScene->Modify();

	SlomoTrack = FindOrCreateMasterTrack<UMovieSceneSlomoTrack>().Track;
	check(SlomoTrack);

	UMovieSceneSection* NewSection = SlomoTrack->CreateNewSection();
	check(NewSection);

	SlomoTrack->AddSection(*NewSection);

	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}


#undef LOCTEXT_NAMESPACE
