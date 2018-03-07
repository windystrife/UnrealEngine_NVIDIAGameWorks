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
#include "TrackEditors/SubTrackEditor.h"
#include "ControlRig.h"

struct FAssetData;
class FFloatCurveKeyArea;
class FMenuBuilder;
class FSequencerSectionPainter;
class UMovieSceneControlRigSection;
class UControlRigSequence;

/**
 * Tools for animation tracks
 */
class FControlRigTrackEditor : public FSubTrackEditor
{
public:
	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FControlRigTrackEditor( TSharedRef<ISequencer> InSequencer );

	/** Virtual destructor. */
	virtual ~FControlRigTrackEditor() { }

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

public:

	// ISequencerTrackEditor interface

	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding ) override;
	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override;
	virtual void BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) override;
	virtual const FSlateBrush* GetIconBrush() const override;

private:

	/** ControlRig sub menu */
	TSharedRef<SWidget> BuildControlRigSubMenu(FGuid ObjectBinding, UMovieSceneTrack* Track);
	void AddControlRigSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, UMovieSceneTrack* Track);

	/** Animation asset selected */
	void OnSequencerAssetSelected(const FAssetData& AssetData, FGuid ObjectBinding, UMovieSceneTrack* Track);

	/** Delegate for AnimatablePropertyChanged in AddKey */
	FKeyPropertyResult AddKeyInternal(float KeyTime, FGuid ObjectBinding, UControlRigSequence* Sequence, UMovieSceneTrack* Track);

	/** Callback for generating the menu of the "Add Sequence" combo button. */
	TSharedRef<SWidget> HandleAddSubSequenceComboButtonGetMenuContent(FGuid ObjectBinding, UMovieSceneTrack* InTrack);
};
