// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Curves/KeyHandle.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneTrackEditor.h"

class FMenuBuilder;
class FSequencerSectionPainter;

/**
 * Tools for particle tracks
 */
class FParticleTrackEditor
	: public FMovieSceneTrackEditor
{
public:

	/**
	 * Constructor.
	 *
	 * @param InSequencer The sequencer instance to be used by this tool.
	 */
	FParticleTrackEditor( TSharedRef<ISequencer> InSequencer );

	/** Virtual destructor. */
	virtual ~FParticleTrackEditor() { }

	/**
	 * Creates an instance of this class.  Called by a sequencer.
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool.
	 * @return The new instance of this class.
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

public:

	void AddParticleKey(const FGuid ObjectGuid, bool bTrigger);

public:

	// ISequencerTrackEditor interface

	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding ) override;
	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override;
	virtual void BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) override;
	
	void AddParticleKey(const FGuid ObjectGuid);
private:

	/** Delegate for AnimatablePropertyChanged in AddKey. */
	FKeyPropertyResult AddKeyInternal( float KeyTime, UObject* Object);
};


/**
 * Class for particle sections.
 */
class FParticleSection
	: public ISequencerSection
	, public TSharedFromThis<FParticleSection>
{
public:

	FParticleSection( UMovieSceneSection& InSection, TSharedRef<ISequencer> InOwningSequencer );
	~FParticleSection();

public:

	// ISequencerSection interface
	virtual UMovieSceneSection* GetSectionObject() override;
	virtual float GetSectionHeight() const override;
	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override;
	virtual int32 OnPaintSection( FSequencerSectionPainter& InPainter ) const override;
	virtual const FSlateBrush* GetKeyBrush( FKeyHandle KeyHandle ) const override;
	virtual FVector2D GetKeyBrushOrigin( FKeyHandle KeyHandle ) const override;
	virtual bool SectionIsResizable() const override { return false; }

private:

	/** The section we are visualizing. */
	UMovieSceneSection& Section;

	/** The sequencer that owns this section */
	TWeakPtr<ISequencer> OwningSequencerPtr;

	/** The UEnum for the EParticleKey enum */
	const UEnum* ParticleKeyEnum;

	const FSlateBrush* LeftKeyBrush;
	const FSlateBrush* RightKeyBrush;
};
