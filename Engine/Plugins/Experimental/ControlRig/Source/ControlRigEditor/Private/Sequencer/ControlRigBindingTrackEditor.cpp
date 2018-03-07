// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigBindingTrackEditor.h"
#include "ControlRigBindingTrack.h"
#include "ControlRigEditMode.h"
#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "ControlRigBindingTrackEditor"


TSharedRef<ISequencerTrackEditor> FControlRigBindingTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FControlRigBindingTrackEditor(InSequencer));
}


FControlRigBindingTrackEditor::FControlRigBindingTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FSpawnTrackEditor(InSequencer)
{ 
}


void FControlRigBindingTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	UMovieSceneSequence* MovieSequence = GetSequencer()->GetFocusedMovieSceneSequence();

	if (!MovieSequence || MovieSequence->GetClass()->GetName() != TEXT("LevelSequence") || !MovieSequence->GetMovieScene()->FindSpawnable(ObjectBinding))
	{
		return;
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddBindingTrack", "Binding Track"),
		LOCTEXT("AddBindingTrackTooltip", "Adds a new track that controls the lifetime and binding of the animation controller."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FControlRigBindingTrackEditor::HandleAddBindingTrackMenuEntryExecute, ObjectBinding),
			FCanExecuteAction::CreateSP(this, &FControlRigBindingTrackEditor::CanAddBindingTrack, ObjectBinding)
		)
	);
}


bool FControlRigBindingTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UControlRigBindingTrack::StaticClass());
}

void FControlRigBindingTrackEditor::HandleAddBindingTrackMenuEntryExecute(FGuid ObjectBinding)
{
	FScopedTransaction AddSpawnTrackTransaction(LOCTEXT("AddBindingTrack_Transaction", "Add Binding Track"));
	AddTrack(GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene(), ObjectBinding, UControlRigBindingTrack::StaticClass(), NAME_None);
	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );

	if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
	{
		ControlRigEditMode->ReBindToActor();
	}
}

bool FControlRigBindingTrackEditor::CanAddBindingTrack(FGuid ObjectBinding) const
{
	return !GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrack<UControlRigBindingTrack>(ObjectBinding);
}

#undef LOCTEXT_NAMESPACE
