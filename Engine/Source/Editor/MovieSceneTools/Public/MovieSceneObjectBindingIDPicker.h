// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateIcon.h"
#include "MovieSceneSequenceID.h"

class SWidget;
class UMovieSceneSequence;
class FMenuBuilder;
class STextBlock;
class ISequencer;
struct EVisibility;
struct FSlateBrush;
struct FSequenceBindingTree;
struct FSequenceBindingNode;
struct FMovieSceneObjectBindingID;

/**
 * Helper class that is used to pick object bindings for movie scene data
 */
class MOVIESCENETOOLS_API FMovieSceneObjectBindingIDPicker
{
public:

	/** Default constructor used in contexts external to the sequencer interface. Always generates FMovieSceneObjectBindingIDs from the root of the sequence */
	FMovieSceneObjectBindingIDPicker()
		: bIsCurrentItemSpawnable(false)
	{}

	/**
	 * Constructor used from within the sequencer interface to generate IDs from the currently focused sequence if possible (else from the root sequence).
	 * This ensures that the bindings will resolve correctly in isolation only the the focused sequence is being used, or from the root sequence.
	 */
	FMovieSceneObjectBindingIDPicker(FMovieSceneSequenceID InLocalSequenceID, TWeakPtr<ISequencer> InSequencer)
		: WeakSequencer(InSequencer)
		, LocalSequenceID(InLocalSequenceID)
		, bIsCurrentItemSpawnable(false)
	{}

protected:
	virtual ~FMovieSceneObjectBindingIDPicker() { }

	/** Get the sequence to look up object bindings within. Only used when no sequencer is available. */
	virtual UMovieSceneSequence* GetSequence() const = 0;

	/** Set the current binding ID */
	virtual void SetCurrentValue(const FMovieSceneObjectBindingID& InBindingId) = 0;

	/** Get the current binding ID */
	virtual FMovieSceneObjectBindingID GetCurrentValue() const = 0;

protected:

	/** Initialize this class - rebuilds sequence hierarchy data and available IDs from the source sequence */
	void Initialize();

	/** Access the text that relates to the currently selected binding ID */
	FText GetCurrentText() const;

	/** Access the tooltip text that relates to the currently selected binding ID */
	FText GetToolTipText() const;

	/** Get the icon that represents the currently assigned binding */
	FSlateIcon GetCurrentIcon() const;
	const FSlateBrush* GetCurrentIconBrush() const;

	/** Get the visibility for the spawnable icon overlap */ 
	EVisibility GetSpawnableIconOverlayVisibility() const;

	/** Assign a new binding ID in response to user-input */
	void SetBindingId(FMovieSceneObjectBindingID InBindingId);

	/** Build menu content that allows the user to choose a binding from inside the source sequence */
	TSharedRef<SWidget> GetPickerMenu();

	/** Get a widget that represents the currently chosen item */
	TSharedRef<SWidget> GetCurrentItemWidget(TSharedRef<STextBlock> TextContent);

	/** Optional sequencer ptr */
	TWeakPtr<ISequencer> WeakSequencer;

	/** The ID of the sequence to generate IDs relative to */
	FMovieSceneSequenceID LocalSequenceID;

private:

	/** Get the currently set binding ID, remapped to the root sequence if necessary */
	FMovieSceneObjectBindingID GetRemappedCurrentValue() const;

	/** Set the binding ID, remapped to the local sequence if possible */
	void SetRemappedCurrentValue(FMovieSceneObjectBindingID InValue);

	/** UPdate the cached text, tooltip and icon */
	void UpdateCachedData();

	/** Called when the combo box has been clicked to populate its menu content */
	void OnGetMenuContent(FMenuBuilder& MenuBuilder, TSharedPtr<FSequenceBindingNode> Node);

	/** Cached current text and tooltips */
	FText CurrentText, ToolTipText;

	/** Cached current icon */
	FSlateIcon CurrentIcon;

	/** Cached value indicating whether the current item is a spawnable */
	bool bIsCurrentItemSpawnable;

	/** Data tree that stores all the available bindings for the current sequence, and their identifiers */
	TSharedPtr<FSequenceBindingTree> DataTree;
};