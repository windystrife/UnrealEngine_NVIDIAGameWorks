
#pragma once

#include "ISequencerEditorObjectBinding.h"

class ISequencer;
class AActor;

class FLevelSequenceEditorActorBinding : public ISequencerEditorObjectBinding
{
public:

	FLevelSequenceEditorActorBinding(TSharedRef<ISequencer> InSequencer);

	// ISequencerEditorObjectBinding interface
	virtual void BuildSequencerAddMenu(FMenuBuilder& MenuBuilder) override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;

private:

	/** Menu extension callback for the add menu */
	void AddPossessActorMenuExtensions(FMenuBuilder& MenuBuilder);
	
	/** Add the specified actors to the sequencer */
	void AddActorsToSequencer(AActor*const* InActors, int32 NumActors);

private:
	TWeakPtr<ISequencer> Sequencer;
};