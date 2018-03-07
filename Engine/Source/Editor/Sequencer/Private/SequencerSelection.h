// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "UObject/WeakObjectPtr.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "SequencerSelectedKey.h"

class UMovieSceneSection;

/**
 * Manages the selection of keys, sections, and outliner nodes for the sequencer.
 */
class FSequencerSelection
{
public:

	DECLARE_MULTICAST_DELEGATE(FOnSelectionChanged)

	DECLARE_MULTICAST_DELEGATE(FOnSelectionChangedObjectGuids)

public:

	FSequencerSelection();

	/** Gets a set of the selected keys. */
	const TSet<FSequencerSelectedKey>& GetSelectedKeys() const;

	/** Gets a set of the selected sections */
	const TSet<TWeakObjectPtr<UMovieSceneSection>>& GetSelectedSections() const;

	/** Gets a set of the selected outliner nodes. */
	const TSet<TSharedRef<FSequencerDisplayNode>>& GetSelectedOutlinerNodes() const;

	/** Gets a set of the outliner nodes that have selected keys or sections */
	const TSet<TSharedRef<FSequencerDisplayNode>>& GetNodesWithSelectedKeysOrSections() const;

	/** Get the currently selected tracks as UMovieSceneTracks */
	TArray<UMovieSceneTrack*> GetSelectedTracks() const;

	/** Adds a key to the selection */
	void AddToSelection(FSequencerSelectedKey Key);

	/** Adds a section to the selection */
	void AddToSelection(UMovieSceneSection* Section);

	/** Adds an outliner node to the selection */
	void AddToSelection(TSharedRef<FSequencerDisplayNode> OutlinerNode);

	/** Adds an array of outliner nodes to the selection */
	void AddToSelection(const TArray<TSharedRef<FSequencerDisplayNode>>& OutlinerNodes);

	/** Adds an outliner node that has selected keys or sections */
	void AddToNodesWithSelectedKeysOrSections(TSharedRef<FSequencerDisplayNode> OutlinerNode);

	/** Removes a key from the selection */
	void RemoveFromSelection(FSequencerSelectedKey Key);

	/** Removes a section from the selection */
	void RemoveFromSelection(UMovieSceneSection* Section);

	/** Removes an outliner node from the selection */
	void RemoveFromSelection(TSharedRef<FSequencerDisplayNode> OutlinerNode);

	/** Removes an outliner node that has selected keys or sections */
	void RemoveFromNodesWithSelectedKeysOrSections(TSharedRef<FSequencerDisplayNode> OutlinerNode);

	/** Removes any outliner nodes from the selection that do not relate to the given section */
	void EmptySelectedOutlinerNodesWithoutSection(UMovieSceneSection* Section);

	/** Returns whether or not the key is selected. */
	bool IsSelected(FSequencerSelectedKey Key) const;

	/** Returns whether or not the section is selected. */
	bool IsSelected(UMovieSceneSection* Section) const;

	/** Returns whether or not the outliner node is selected. */
	bool IsSelected(TSharedRef<FSequencerDisplayNode> OutlinerNode) const;

	/** Returns whether or not the outliner node has keys or sections selected. */
	bool NodeHasSelectedKeysOrSections(TSharedRef<FSequencerDisplayNode> OutlinerNode) const;

	/** Empties all selections. */
	void Empty();

	/** Empties the key selection. */
	void EmptySelectedKeys();

	/** Empties the section selection. */
	void EmptySelectedSections();

	/** Empties the outliner node selection. */
	void EmptySelectedOutlinerNodes();

	/** Empties the outliner nodes with selected keys or sections. */
	void EmptyNodesWithSelectedKeysOrSections();

	/** Gets a multicast delegate which is called when the key selection changes. */
	FOnSelectionChanged& GetOnKeySelectionChanged();

	/** Gets a multicast delegate which is called when the section selection changes. */
	FOnSelectionChanged& GetOnSectionSelectionChanged();

	/** Gets a multicast delegate which is called when the outliner node selection changes. */
	FOnSelectionChanged& GetOnOutlinerNodeSelectionChanged();

	/** Gets a multicast delegate which is called when the outliner node with selected keys or sections changes. */
	FOnSelectionChanged& GetOnNodesWithSelectedKeysOrSectionsChanged();

	/** Gets a multicast delegate with an array of FGuid of bound objects which is called when the outliner node selection changes. */
	FOnSelectionChangedObjectGuids& GetOnOutlinerNodeSelectionChangedObjectGuids();

	/** Helper function to get an array of FGuid of bound objects on return */
	TArray<FGuid> GetBoundObjectsGuids();

	/** Suspend the broadcast of selection change notifications.  */
	void SuspendBroadcast();

	/** Resume the broadcast of selection change notifications.  */
	void ResumeBroadcast();

	/** Requests that the outliner node selection changed delegate be broadcast on the next update. */
	void RequestOutlinerNodeSelectionChangedBroadcast();

	/** Updates the selection once per frame.  This is required for deferred selection broadcasts. */
	void Tick();

private:
	/** When true, selection change notifications should be broadcasted. */
	bool IsBroadcasting();
	
private:

	TSet<FSequencerSelectedKey> SelectedKeys;
	TSet<TWeakObjectPtr<UMovieSceneSection>> SelectedSections;
	TSet<TSharedRef<FSequencerDisplayNode>> SelectedOutlinerNodes;
	TSet<TSharedRef<FSequencerDisplayNode>> NodesWithSelectedKeysOrSections;

	FOnSelectionChanged OnKeySelectionChanged;
	FOnSelectionChanged OnSectionSelectionChanged;
	FOnSelectionChanged OnOutlinerNodeSelectionChanged;
	FOnSelectionChanged OnNodesWithSelectedKeysOrSectionsChanged;

	FOnSelectionChangedObjectGuids OnOutlinerNodeSelectionChangedObjectGuids;

	/** The number of times the broadcasting of selection change notifications has been suspended. */
	int32 SuspendBroadcastCount;

	/** When true there is a pending outliner node selection change which will be broadcast next tick. */
	bool bOutlinerNodeSelectionChangedBroadcastPending;
};
