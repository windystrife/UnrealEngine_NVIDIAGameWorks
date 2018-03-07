// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Widgets/SWidget.h"
#include "SequencerNodeTree.h"

class FGroupedKeyArea;
class FMenuBuilder;
class FSequencer;
class FSequencerDisplayNodeDragDropOp;
class FSequencerObjectBindingNode;
class IKeyArea;
class ISequencerTrackEditor;
class SSequencerTreeViewRow;
class UMovieSceneTrack;
class UMovieSceneSection;
struct FSlateBrush;
enum class EItemDropZone;

/**
 * Structure used to define padding for a particular node.
 */
struct FNodePadding
{
	FNodePadding(float InUniform) : Top(InUniform), Bottom(InUniform) { }
	FNodePadding(float InTop, float InBottom) : Top(InTop), Bottom(InBottom) { }

	/** @return The sum total of the separate padding values */
	float Combined() const
	{
		return Top + Bottom;
	}

	/** Padding to be applied to the top of the node */
	float Top;

	/** Padding to be applied to the bottom of the node */
	float Bottom;
};

namespace ESequencerNode
{
	enum Type
	{
		/* Top level object binding node */
		Object,
		/* Area for tracks */
		Track,
		/* Area for keys inside of a section */
		KeyArea,
		/* Displays a category */
		Category,
		/* Benign spacer node */
		Spacer,
		/* Folder node */
		Folder
	};
}


/**
 * Base Sequencer layout node.
 */
class FSequencerDisplayNode
	: public TSharedFromThis<FSequencerDisplayNode>
{
public:

	/**
	 * Create and initialize a new instance.
	 * 
	 * @param InNodeName	The name identifier of then node
	 * @param InParentNode	The parent of this node or nullptr if this is a root node
	 * @param InParentTree	The tree this node is in
	 */
	FSequencerDisplayNode( FName InNodeName, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree );

	/** Virtual destructor. */
	virtual ~FSequencerDisplayNode(){}

public:

	/**
	* Adds an object binding node to this node.
	*
	* @param ObjectBindingNode The node to add
	*/
	void AddObjectBindingNode( TSharedRef<FSequencerObjectBindingNode> ObjectBindingNode );

	/** 
	 * Finds any parent object binding node above this node in the hierarchy
	 *
	 * @return the parent node, or nullptr if no object binding is found
	 */
	TSharedPtr<FSequencerObjectBindingNode> FindParentObjectBindingNode() const;

	/**
	 * Adds a category to this node
	 * 
	 * @param CategoryName	Name of the category
	 * @param DisplayName	Localized label to display in the animation outliner
	 */
	TSharedRef<class FSequencerSectionCategoryNode> AddCategoryNode( FName CategoryName, const FText& DisplayLabel );

	/**
	 * Adds a key area to this node
	 *
	 * @param KeyAreaName	Name of the key area
	 * @param DisplayLabel	Localized label to display in the animation outliner
	 * @param KeyArea		Key area interface for drawing and interaction with keys
	 */
	void AddKeyAreaNode( FName KeyAreaName, const FText& DisplayLabel, TSharedRef<IKeyArea> KeyArea );

	/**
	 * @return The type of node this is
	 */
	virtual ESequencerNode::Type GetType() const = 0;

	/** @return Whether or not this node can be selected */
	virtual bool IsSelectable() const
	{
		return true;
	}

	/**
	 * @return The desired height of the node when displayed
	 */
	virtual float GetNodeHeight() const = 0;
	
	/**
	 * @return The desired padding of the node when displayed
	 */
	virtual FNodePadding GetNodePadding() const = 0;

	/**
	 * Whether the node can be renamed.
	 *
	 * @return true if this node can be renamed, false otherwise.
	 */
	virtual bool CanRenameNode() const = 0;

	/**
	 * @return The localized display name of this node
	 */
	virtual FText GetDisplayName() const = 0;

	/**
	* @return the color used to draw the display name.
	*/
	virtual FLinearColor GetDisplayNameColor() const;

	/**
	 * @return the text to display for the tool tip for the display name. 
	 */
	virtual FText GetDisplayNameToolTipText() const;

	/**
	 * Set the node's display name.
	 *
	 * @param NewDisplayName the display name to set.
	 */
	virtual void SetDisplayName(const FText& NewDisplayName) = 0;

	/**
	 * @return Whether this node handles resize events
	 */
	virtual bool IsResizable() const
	{
		return false;
	}

	/**
	 * Resize this node
	 */
	virtual void Resize(float NewSize)
	{
		
	}

	/**
	 * Generates a container widget for tree display in the animation outliner portion of the track area
	 * 
	 * @return Generated outliner container widget
	 */
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SSequencerTreeViewRow>& InRow);

	/**
	 * Customizes an outliner widget that is to represent this node
	 * 
	 * @return Content to display on the outliner node
	 */
	virtual TSharedRef<SWidget> GetCustomOutlinerContent();

	/**
	 * Generates a widget for display in the section area portion of the track area
	 * 
	 * @param ViewRange	The range of time in the sequencer that we are displaying
	 * @return Generated outliner widget
	 */
	virtual TSharedRef<SWidget> GenerateWidgetForSectionArea( const TAttribute<TRange<float>>& ViewRange );

	/**
	 * Gets an icon that represents this sequencer display node
	 * 
	 * @return This node's representative icon
	 */
	virtual const FSlateBrush* GetIconBrush() const;

	/**
	 * Get a brush to overlay on top of the icon for this node
	 * 
	 * @return An overlay brush, or nullptr
	 */
	virtual const FSlateBrush* GetIconOverlayBrush() const;

	/**
	 * Gets the color for the icon brush
	 * 
	 * @return This node's representative color
	 */
	virtual FSlateColor GetIconColor() const;

	/**
	 * Get the tooltip text to display for this node's icon
	 * 
	 * @return Text to display on the icon
	 */
	virtual FText GetIconToolTipText() const;

	/**
	 * Get the display node that is ultimately responsible for constructing a section area widget for this node.
	 * Could return this node itself, or a parent node
	 */
	TSharedPtr<FSequencerDisplayNode> GetSectionAreaAuthority() const;

	/**
	 * @return the path to this node starting with the outermost parent
	 */
	FString GetPathName() const;

	/** Summon context menu */
	TSharedPtr<SWidget> OnSummonContextMenu();

	/** What sort of context menu this node summons */
	virtual void BuildContextMenu(FMenuBuilder& MenuBuilder);

	/**
	 * @return The name of the node (for identification purposes)
	 */
	FName GetNodeName() const
	{
		return NodeName;
	}

	/**
	 * @return The number of child nodes belonging to this node
	 */
	uint32 GetNumChildren() const
	{
		return ChildNodes.Num();
	}

	/**
	 * @return A List of all Child nodes belonging to this node
	 */
	const TArray<TSharedRef<FSequencerDisplayNode>>& GetChildNodes() const
	{
		return ChildNodes;
	}

	/**
	 * Iterate this entire node tree, child first.
	 *
	 * @param 	InPredicate			Predicate to call for each node, returning whether to continue iteration or not
	 * @param 	bIncludeThisNode	Whether to include this node in the iteration, or just children
	 * @return  true where the client prematurely exited the iteration, false otherwise
	 */
	bool Traverse_ChildFirst(const TFunctionRef<bool(FSequencerDisplayNode&)>& InPredicate, bool bIncludeThisNode = true);

	/**
	 * Iterate this entire node tree, parent first.
	 *
	 * @param 	InPredicate			Predicate to call for each node, returning whether to continue iteration or not
	 * @param 	bIncludeThisNode	Whether to include this node in the iteration, or just children
	 * @return  true where the client prematurely exited the iteration, false otherwise
	 */
	bool Traverse_ParentFirst(const TFunctionRef<bool(FSequencerDisplayNode&)>& InPredicate, bool bIncludeThisNode = true);

	/**
	 * Iterate any visible portions of this node's sub-tree, child first.
	 *
	 * @param 	InPredicate			Predicate to call for each node, returning whether to continue iteration or not
	 * @param 	bIncludeThisNode	Whether to include this node in the iteration, or just children
	 * @return  true where the client prematurely exited the iteration, false otherwise
	 */
	bool TraverseVisible_ChildFirst(const TFunctionRef<bool(FSequencerDisplayNode&)>& InPredicate, bool bIncludeThisNode = true);

	/**
	 * Iterate any visible portions of this node's sub-tree, parent first.
	 *
	 * @param 	InPredicate			Predicate to call for each node, returning whether to continue iteration or not
	 * @param 	bIncludeThisNode	Whether to include this node in the iteration, or just children
	 * @return  true where the client prematurely exited the iteration, false otherwise
	 */
	bool TraverseVisible_ParentFirst(const TFunctionRef<bool(FSequencerDisplayNode&)>& InPredicate, bool bIncludeThisNode = true);

	/**
	 * Sorts the child nodes with the supplied predicate
	 */
	template <class PREDICATE_CLASS>
	void SortChildNodes(const PREDICATE_CLASS& Predicate)
	{
		ChildNodes.StableSort(Predicate);

		for (TSharedRef<FSequencerDisplayNode> ChildNode : ChildNodes)
		{
			ChildNode->SortChildNodes(Predicate);
		}
	}

	/**
	 * @return The parent of this node
	 */
	TSharedPtr<FSequencerDisplayNode> GetParent() const
	{
		return ParentNode.Pin();
	}

	/**
	 * @return The outermost parent of this node
	 */
	TSharedRef<FSequencerDisplayNode> GetOutermostParent()
	{
		TSharedPtr<FSequencerDisplayNode> Parent = ParentNode.Pin();
		return Parent.IsValid() ? Parent->GetOutermostParent() : AsShared();
	}
	
	/** Gets the sequencer that owns this node */
	FSequencer& GetSequencer() const
	{
		return ParentTree.GetSequencer();
	}
	
	/** Gets the parent tree that this node is in */
	FSequencerNodeTree& GetParentTree() const
	{
		return ParentTree;
	}

	/** Gets all the key area nodes recursively, including this node if applicable */
	virtual void GetChildKeyAreaNodesRecursively(TArray<TSharedRef<class FSequencerSectionKeyAreaNode>>& OutNodes) const;

	/**
	 * Set whether this node is expanded or not
	 */
	void SetExpansionState(bool bInExpanded);

	/**
	 * @return Whether or not this node is expanded
	 */
	bool IsExpanded() const;

	/**
	 * @return Whether this node is explicitly hidden from the view or not
	 */
	bool IsHidden() const;

	/**
	 * Check whether the node's tree view or track area widgets are hovered by the user's mouse.
	 *
	 * @return true if hovered, false otherwise. */
	bool IsHovered() const;

	/** Initialize this node with expansion states and virtual offsets */
	void Initialize(float InVirtualTop, float InVirtualBottom);

	/** @return this node's virtual offset from the top of the tree, irrespective of expansion states */
	float GetVirtualTop() const
	{
		return VirtualTop;
	}
	
	/** @return this node's virtual offset plus its virtual height, irrespective of expansion states */
	float GetVirtualBottom() const
	{
		return VirtualBottom;
	}

	/** Get the key grouping for the specified section index, ensuring it is fully up to date */
	TSharedRef<FGroupedKeyArea> GetKeyGrouping(UMovieSceneSection* InSection);

	/** Get key groupings array */
	const TArray<TSharedRef<FGroupedKeyArea>>& GetKeyGroupings() const { return KeyGroupings; }

	DECLARE_EVENT(FSequencerDisplayNode, FRequestRenameEvent);
	FRequestRenameEvent& OnRenameRequested() { return RenameRequestedEvent; }

	/** 
	 * Returns whether or not this node can be dragged. 
	 */
	virtual bool CanDrag() const { return false; }

	/**
	 * Determines if there is a valid drop zone based on the current drag drop operation and the zone the items were dragged onto.
	 */
	virtual TOptional<EItemDropZone> CanDrop( FSequencerDisplayNodeDragDropOp& DragDropOp, EItemDropZone ItemDropZone ) const { return TOptional<EItemDropZone>(); }

	/**
	 * Handles a drop of items onto this display node.
	 */
	virtual void Drop( const TArray<TSharedRef<FSequencerDisplayNode>>& DraggedNodes, EItemDropZone DropZone ) { }

public:

	/** Temporarily disable dynamic regeneration of key groupings. This prevents overlapping key groups from being amalgamated during drags. Key times will continue to update correctly. */
	static void DisableKeyGoupingRegeneration();

	/** Re-enable dynamic regeneration of key groupings */
	static void EnableKeyGoupingRegeneration();

protected:

	/** Adds a child to this node, and sets it's parent to this node. */
	void AddChildAndSetParent( TSharedRef<FSequencerDisplayNode> InChild );

private:

	/** Callback for executing a "Rename Node" context menu action. */
	void HandleContextMenuRenameNodeExecute();

	/** Callback for determining whether a "Rename Node" context menu action can execute. */
	bool HandleContextMenuRenameNodeCanExecute() const;

protected:

	/** The virtual offset of this item from the top of the tree, irrespective of expansion states. */
	float VirtualTop;

	/** The virtual offset + virtual height of this item, irrespective of expansion states. */
	float VirtualBottom;

protected:

	/** The parent of this node*/
	TWeakPtr<FSequencerDisplayNode > ParentNode;

	/** List of children belonging to this node */
	TArray<TSharedRef<FSequencerDisplayNode>> ChildNodes;

	/** Parent tree that this node is in */
	FSequencerNodeTree& ParentTree;

	/** The name identifier of this node */
	FName NodeName;

	/** Whether or not the node is expanded */
	bool bExpanded;

	/** Transient grouped keys for this node */
	TArray<TSharedRef<FGroupedKeyArea>> KeyGroupings;

	/** Event that is triggered when rename is requested */
	FRequestRenameEvent RenameRequestedEvent;
};
