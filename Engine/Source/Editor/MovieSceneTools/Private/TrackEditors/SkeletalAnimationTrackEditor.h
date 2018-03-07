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

struct FAssetData;
class FFloatCurveKeyArea;
class FMenuBuilder;
class FSequencerSectionPainter;
class UMovieSceneSkeletalAnimationSection;
class USkeleton;

/**
 * Tools for animation tracks
 */
class FSkeletalAnimationTrackEditor : public FMovieSceneTrackEditor
{
public:
	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FSkeletalAnimationTrackEditor( TSharedRef<ISequencer> InSequencer );

	/** Virtual destructor. */
	virtual ~FSkeletalAnimationTrackEditor() { }

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

public:

	// ISequencerTrackEditor interface

	virtual void AddKey(const FGuid& ObjectGuid) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding ) override;
	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override;
	virtual void BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;

private:

	/** Animation sub menu */
	TSharedRef<SWidget> BuildAnimationSubMenu(FGuid ObjectBinding, USkeleton* Skeleton, UMovieSceneTrack* Track);
	void AddAnimationSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, USkeleton* Skeleton, UMovieSceneTrack* Track);

	/** Animation sub menu filter function */
	bool ShouldFilterAsset(const FAssetData& AssetData);

	/** Animation asset selected */
	void OnAnimationAssetSelected(const FAssetData& AssetData, FGuid ObjectBinding, UMovieSceneTrack* Track);

	/** Delegate for AnimatablePropertyChanged in AddKey */
	FKeyPropertyResult AddKeyInternal(float KeyTime, UObject* Object, class UAnimSequenceBase* AnimSequence, UMovieSceneTrack* Track);

	/** Gets a skeleton from an object guid in the movie scene */
	class USkeleton* AcquireSkeletonFromObjectGuid(const FGuid& Guid);
};


/** Class for animation sections */
class FSkeletalAnimationSection
	: public ISequencerSection
	, public TSharedFromThis<FSkeletalAnimationSection>
{
public:

	/** Constructor. */
	FSkeletalAnimationSection( UMovieSceneSection& InSection );

	/** Virtual destructor. */
	virtual ~FSkeletalAnimationSection() { }

public:

	// ISequencerSection interface

	virtual UMovieSceneSection* GetSectionObject() override;
	virtual FText GetSectionTitle() const override;
	virtual float GetSectionHeight() const override;
	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override;
	virtual int32 OnPaintSection( FSequencerSectionPainter& Painter ) const override;
	virtual void BeginResizeSection() override;
	virtual void ResizeSection(ESequencerSectionResizeMode ResizeMode, float ResizeTime) override;
	virtual void BeginSlipSection() override;
	virtual void SlipSection(float SlipTime) override;

private:

	/** The section we are visualizing */
	UMovieSceneSkeletalAnimationSection& Section;

	/** Weight key areas. */
	mutable TSharedPtr<FFloatCurveKeyArea> WeightArea;

	/** Cached start offset value valid only during resize */
	float InitialStartOffsetDuringResize;
	
	/** Cached start time valid only during resize */
	float InitialStartTimeDuringResize;
};
