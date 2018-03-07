// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

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

class FMenuBuilder;
class FPaintArgs;
class FSlateWindowElementList;
class SHorizontalBox;

/**
 * Result of keying
 */
struct FKeyPropertyResult
{
	FKeyPropertyResult()
		: bTrackModified(false)
		, bHandleCreated(false)
		, bTrackCreated(false) {}

	/* Was the track modified in any way? */
	bool bTrackModified;

	/* Was a handle/binding created? */
	bool bHandleCreated;

	/* Was a track created? */
	bool bTrackCreated;
};

/** Delegate for adding keys for a property
 * float - The time at which to add the key.
 * return - KeyPropertyResult - 
 */
DECLARE_DELEGATE_RetVal_OneParam(FKeyPropertyResult, FOnKeyProperty, float)


/** Delegate for whether a property can be keyed
 * float - The time at which to add the key.
 * return - True if the property can be keyed, otherwise false.
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FCanKeyProperty, float)


/**
 * Base class for handling key and section drawing and manipulation
 * of a UMovieSceneTrack class.
 *
 * @todo Sequencer Interface needs cleanup
 */
class SEQUENCER_API FMovieSceneTrackEditor
	: public TSharedFromThis<FMovieSceneTrackEditor>
	, public ISequencerTrackEditor
{
public:

	/** Constructor */
	FMovieSceneTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Destructor */
	virtual ~FMovieSceneTrackEditor();

public:

	/** @return The current movie scene */
	UMovieSceneSequence* GetMovieSceneSequence() const;

	/**
	 * @return The current local time at which we should add a key
	 */
	float GetTimeForKey();

	void UpdatePlaybackRange();

	void AnimatablePropertyChanged( FOnKeyProperty OnKeyProperty );

	struct FFindOrCreateHandleResult
	{
		FGuid Handle;
		bool bWasCreated;
	};
	
	FFindOrCreateHandleResult FindOrCreateHandleToObject( UObject* Object, bool bCreateHandleIfMissing = true );

	struct FFindOrCreateTrackResult
	{
		UMovieSceneTrack* Track;
		bool bWasCreated;
	};

	FFindOrCreateTrackResult FindOrCreateTrackForObject( const FGuid& ObjectHandle, TSubclassOf<UMovieSceneTrack> TrackClass, FName PropertyName = NAME_None, bool bCreateTrackIfMissing = true );

	template<typename TrackClass>
	struct FFindOrCreateMasterTrackResult
	{
		TrackClass* Track;
		bool bWasCreated;
	};

	/**
	 * Find or add a master track of the specified type in the focused movie scene.
	 *
	 * @param TrackClass The class of the track to find or add.
	 * @return The track results.
	 */
	template<typename TrackClass>
	FFindOrCreateMasterTrackResult<TrackClass> FindOrCreateMasterTrack()
	{
		FFindOrCreateMasterTrackResult<TrackClass> Result;
		bool bTrackExisted;

		UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();
		Result.Track = MovieScene->FindMasterTrack<TrackClass>();
		bTrackExisted = Result.Track != nullptr;

		if (Result.Track == nullptr)
		{
			Result.Track = MovieScene->AddMasterTrack<TrackClass>();
		}

		Result.bWasCreated = bTrackExisted == false && Result.Track != nullptr;
		return Result;
	}


	/** @return The sequencer bound to this handler */
	const TSharedPtr<ISequencer> GetSequencer() const;

public:

	// ISequencerTrackEditor interface

	virtual void AddKey( const FGuid& ObjectGuid ) override;

	virtual UMovieSceneTrack* AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName) override;

	virtual void BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings) override;
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual void BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual void BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) override;
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
	virtual bool OnAllowDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track) override;
	virtual FReply OnDrop(const FDragDropEvent& DragDropEvent, UMovieSceneTrack* Track) override;

	virtual void OnInitialize() override;
	virtual void OnRelease() override;

	virtual int32 PaintTrackArea(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle);

	virtual bool SupportsType( TSubclassOf<class UMovieSceneTrack> TrackClass ) const = 0;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const { return true; }
	virtual void Tick(float DeltaTime) override;

protected:

	/**
	 * Gets the currently focused movie scene, if any.
	 *
	 * @return Focused movie scene, or nullptr if no movie scene is focused.
	 */
	UMovieScene* GetFocusedMovieScene() const;

private:

	/** The sequencer bound to this handler.  Used to access movie scene and time info during auto-key */
	TWeakPtr<ISequencer> Sequencer;
};
