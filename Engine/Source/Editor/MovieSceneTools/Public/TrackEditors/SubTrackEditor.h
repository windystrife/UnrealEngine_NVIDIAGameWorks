// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Widgets/SWidget.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneTrackEditor.h"

class AActor;
struct FAssetData;
class FMenuBuilder;

/**
 * Tools for animatable property types such as floats ands vectors
 */
class MOVIESCENETOOLS_API FSubTrackEditor
	: public FMovieSceneTrackEditor
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FSubTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FSubTrackEditor() { }

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:

	// ISequencerTrackEditor interface

	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual bool OnAllowDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track) override;
	virtual FReply OnDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track) override;
	
protected:

	/**
	 * Check whether the given sequence can be added as a sub-sequence.
	 *
	 * The purpose of this method is to disallow circular references
	 * between sub-sequences in the focused movie scene.
	 *
	 * @param Sequence The sequence to check.
	 * @return true if the sequence can be added as a sub-sequence, false otherwise.
	 */
	bool CanAddSubSequence(const UMovieSceneSequence& Sequence) const;

private:

	/** Callback for executing the "Add Event Track" menu entry. */
	void HandleAddSubTrackMenuEntryExecute();

	/** Callback for generating the menu of the "Add Sequence" combo button. */
	TSharedRef<SWidget> HandleAddSubSequenceComboButtonGetMenuContent(UMovieSceneTrack* InTrack);

	/** Callback for executing a menu entry in the "Add Sequence" combo button. */
	void HandleAddSubSequenceComboButtonMenuEntryExecute(const FAssetData& AssetData, UMovieSceneTrack* InTrack);

	/** Delegate for AnimatablePropertyChanged in AddKey */
	FKeyPropertyResult AddKeyInternal(float KeyTime, UMovieSceneSequence* InMovieSceneSequence, UMovieSceneTrack* InTrack);

	/** Callback for AnimatablePropertyChanged in HandleAssetAdded. */
	FKeyPropertyResult HandleSequenceAdded(float KeyTime, UMovieSceneSequence* Sequence);

	/** Check if we can record a new sequence (deny it if one is already primed) */
	bool CanRecordNewSequence() const;

	/** Handle recording new sequence into a sub track */
	void HandleRecordNewSequence(AActor* InActorToRecord, UMovieSceneTrack* InTrack);

	/** Actually handles the adding of the section */
	FKeyPropertyResult HandleRecordNewSequenceInternal(float KeyTime, AActor* InActorToRecord, UMovieSceneTrack* InTrack);
};
