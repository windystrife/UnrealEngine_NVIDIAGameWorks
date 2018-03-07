// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Widgets/SWidget.h"
#include "ISequencerSection.h"
#include "MovieSceneTrack.h"
#include "ISequencer.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneTrackEditor.h"

struct FAssetData;
class FAudioThumbnail;
class FFloatCurveKeyArea;
class FMenuBuilder;
class FSequencerSectionPainter;
class USoundWave;

/**
 * Tools for audio tracks
 */
class MOVIESCENETOOLS_API FAudioTrackEditor
	: public FMovieSceneTrackEditor
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FAudioTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FAudioTrackEditor();

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
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	virtual void BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual bool IsResizable(UMovieSceneTrack* InTrack) const override;
	virtual void Resize(float NewSize, UMovieSceneTrack* InTrack) override;
	
protected:

	/** Delegate for AnimatablePropertyChanged in HandleAssetAdded for master sounds */
	FKeyPropertyResult AddNewMasterSound(float KeyTime, class USoundBase* Sound);

	/** Delegate for AnimatablePropertyChanged in HandleAssetAdded for attached sounds */
	FKeyPropertyResult AddNewAttachedSound(float KeyTime, class USoundBase* Sound, TArray<TWeakObjectPtr<UObject>> ObjectsToAttachTo);

private:

	/** Callback for executing the "Add Audio Track" menu entry. */
	void HandleAddAudioTrackMenuEntryExecute();

	/** Audio sub menu */
	TSharedRef<SWidget> BuildAudioSubMenu(UMovieSceneTrack* Track);

	/** Audio asset selected */
	void OnAudioAssetSelected(const FAssetData& AssetData, UMovieSceneTrack* Track);
};


/**
 * Class for audio sections, handles drawing of all waveform previews.
 */
class FAudioSection
	: public ISequencerSection
	, public TSharedFromThis<FAudioSection>
{
public:

	/** Constructor. */
	FAudioSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FAudioSection();

public:

	// ISequencerSection interface

	virtual UMovieSceneSection* GetSectionObject() override;
	virtual FText GetSectionTitle() const override;
	virtual float GetSectionHeight() const override;
	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder) const override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const FGeometry& ParentGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void BeginSlipSection() override;
	virtual void SlipSection(float SlipTime) override;
	
private:

	/* Re-creates the texture used to preview the waveform. */
	void RegenerateWaveforms(TRange<float> DrawRange, int32 XOffset, int32 XSize, const FColor& ColorTint, float DisplayScale);

private:

	/** The section we are visualizing. */
	UMovieSceneSection& Section;

	/** The waveform thumbnail render object. */
	TSharedPtr<class FAudioThumbnail> WaveformThumbnail;

	/** Sound volume/pitch key areas. */
	mutable TSharedPtr<FFloatCurveKeyArea> SoundVolumeArea;
	mutable TSharedPtr<FFloatCurveKeyArea> PitchMultiplierArea;

	/** Stored data about the waveform to determine when it is invalidated. */
	TRange<float> StoredDrawRange;
	float StoredStartOffset;
	int32 StoredXOffset;
	int32 StoredXSize;
	FColor StoredColor;
	float StoredSectionHeight;

	/** Stored sound wave to determine when it is invalidated. */
	TWeakObjectPtr<USoundWave> StoredSoundWave;

	TWeakPtr<ISequencer> Sequencer;

	/** Cached start offset value valid only during resize */
	float InitialStartOffsetDuringResize;
	
	/** Cached start time valid only during resize */
	float InitialStartTimeDuringResize;
};
