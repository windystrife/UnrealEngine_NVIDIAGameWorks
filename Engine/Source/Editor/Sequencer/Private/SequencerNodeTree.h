// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

class FSequencer;
class FSequencerDisplayNode;
class FSequencerObjectBindingNode;
class FSequencerTrackNode;
class ISequencerTrackEditor;
class UMovieSceneFolder;
class UMovieSceneTrack;
struct FMovieSceneBinding;

/**
 * Represents a tree of sequencer display nodes, used to populate the Sequencer UI with MovieScene data
 */
class FSequencerNodeTree : public TSharedFromThis<FSequencerNodeTree>
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnUpdated);

public:
	FSequencerNodeTree( class FSequencer& InSequencer )
		: Sequencer( InSequencer )
	{}
	
	/**
	 * Empties the entire tree
	 */
	void Empty();

	/**
	 * Updates the tree with sections from a MovieScene
	 */
	void Update();

	/**
	 * @return The root nodes of the tree
	 */
	const TArray< TSharedRef<FSequencerDisplayNode> >& GetRootNodes() const;

	/** @return Whether or not there is an active filter */
	bool HasActiveFilter() const { return !FilterString.IsEmpty(); }

	/**
	 * Returns whether or not a node is filtered
	 *
	 * @param Node	The node to check if it is filtered
	 */
	bool IsNodeFiltered( const TSharedRef<const FSequencerDisplayNode> Node ) const;
	
	/**
	 * Filters the nodes based on the passed in filter terms
	 *
	 * @param InFilter	The filter terms
	 */
	void FilterNodes( const FString& InFilter );

	/**
	 * @return All nodes in a flat array
	 */
	TArray< TSharedRef<FSequencerDisplayNode> > GetAllNodes() const;

	/** Gets the parent sequencer of this tree */
	FSequencer& GetSequencer() {return Sequencer;}

	/**
	 * Saves the expansion state of a display node
	 *
	 * @param Node		The node whose expansion state should be saved
	 * @param bExpanded	The new expansion state of the node
	 */
	void SaveExpansionState( const FSequencerDisplayNode& Node, bool bExpanded );

	/**
	 * Gets the saved expansion state of a display node
	 *
	 * @param Node	The node whose expansion state may have been saved
	 * @return true if the node should be expanded, false otherwise	
	 */
	bool GetSavedExpansionState( const FSequencerDisplayNode& Node ) const;

	/**
	 * Get the default expansion state for the specified node, where its state has not yet been saved
	 *
	 * @return true if the node is to be expanded, false otherwise
	 */
	bool GetDefaultExpansionState( const FSequencerDisplayNode& Node ) const;

	/**
	 * Set the single hovered node in the tree
	 */
	void SetHoveredNode(const TSharedPtr<FSequencerDisplayNode>& InHoveredNode);

	/**
	 * Get the single hovered node in the tree, possibly nullptr
	 */
	const TSharedPtr<FSequencerDisplayNode>& GetHoveredNode() const;

	/*
	 * Get the object binding map from guid to object binding nodes
	 */
	const TMap< FGuid, TSharedPtr<FSequencerObjectBindingNode> >& GetObjectBindingMap() const { return ObjectBindingMap; }

	/*
	 * Gets a multicast delegate which is called whenever the node tree has been updated.
	 */
	FOnUpdated& OnUpdated() { return OnUpdatedDelegate; }

private:
	/**
	 * Finds or adds a type editor for the track
	 *
	 * @param Track	The type to find an editor for
	 * @rerturn The editor for the type
	 */
	TSharedRef<ISequencerTrackEditor> FindOrAddTypeEditor( UMovieSceneTrack& Track );

	/* 
	 * Makes sub-track nodes and section interfaces for a track node.
	 * @param The track node to create sub-tracks and section interfaces for.
	 * @return A new parent for the supplied track, or the track itself
	 */
	TSharedRef<FSequencerTrackNode> MakeSubTracksAndSectionInterfaces(TSharedRef<FSequencerTrackNode> TrackNode, const FGuid& ObjectBinding = FGuid());

	/**
	 * Makes section interfaces for all sections in a track
	 *
	 * @param SectionAreaNode	The section area which section interfaces belong to
	 * @param ObjectBinding Object binding ID that the track operates on
	 */
	void MakeSectionInterfaces( TSharedRef<FSequencerTrackNode> TrackNode, const FGuid& ObjectBinding );

	/**
	 * Creates a new object binding node and any parent binding nodes.
	 */
	TSharedRef<FSequencerObjectBindingNode> AddObjectBinding( const FString& ObjectName, const FGuid& ObjectBinding, TMap<FGuid, const FMovieSceneBinding*>& GuidToBindingMap, TArray< TSharedRef<FSequencerObjectBindingNode> >& OutNodeList );

	/**
	 * Creates the tree of folder nodes and populates it with object and track nodes. 
	 */
	void CreateAndPopulateFolderNodes( TArray<TSharedRef<FSequencerTrackNode>>& MasterTrackNodes, TArray<TSharedRef<FSequencerObjectBindingNode>>& ObjectNodes,
		TArray<UMovieSceneFolder*>& MovieSceneFolders, TArray<TSharedRef<FSequencerDisplayNode>>& FolderAndObjectNodes, TArray<TSharedRef<FSequencerDisplayNode>>& MasterTrackNodesNotInFolders );

private:
	/** Tools for building movie scene section layouts.  One tool for each track */
	TMap< UMovieSceneTrack*, TSharedPtr<ISequencerTrackEditor> > EditorMap;
	/** Root nodes */
	TArray< TSharedRef<FSequencerDisplayNode> > RootNodes;
	/** Mapping of object binding guids to their node (for fast lookup) */
	TMap< FGuid, TSharedPtr<FSequencerObjectBindingNode> > ObjectBindingMap;
	/** Set of all filtered nodes */
	TSet< TSharedRef<const FSequencerDisplayNode> > FilteredNodes;
	/** Cardinal hovered node */
	TSharedPtr<FSequencerDisplayNode> HoveredNode;
	/** Active filter string if any */
	FString FilterString;
	/** Sequencer interface */
	FSequencer& Sequencer;
	/** A multicast delegate which is called whenever the node tree has been updated. */
	FOnUpdated OnUpdatedDelegate;
};
