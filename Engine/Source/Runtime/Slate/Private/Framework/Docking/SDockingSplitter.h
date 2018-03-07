// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/Docking/TabManager.h"
#include "SDockingNode.h"

class SDockingTabStack;

/** Dynamic N-way splitter that provides the resizing functionality in the docking framework. */
class SLATE_API SDockingSplitter : public SDockingNode
{
public:
	SLATE_BEGIN_ARGS(SDockingSplitter){}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef<FTabManager::FSplitter>& PersistentNode );

	virtual Type GetNodeType() const override
	{
		return SDockingNode::DockSplitter;
	}

	/**
	 * Add a new child dock node at the desired location.
	 * Assumes this DockNode is a splitter.
	 *
	 * @param InChild      The DockNode child to add.
	 * @param InLocation   Index at which to add; INDEX_NONE (default) adds to the end of the list
	 */
	void AddChildNode( const TSharedRef<SDockingNode>& InChild, int32 InLocation = INDEX_NONE);

	/**
	 * Replace the child InChildToReplace with Replacement
	 *
	 * @param InChildToReplace     The child of this node to replace
	 * @param Replacement          What to replace with
	 */
	void ReplaceChild( const TSharedRef<SDockingNode>& InChildToReplace, const TSharedRef<SDockingNode>& Replacement );

	/**
	 * Remove a child node from the splitter
	 * 
	 * @param ChildToRemove    The child node to remove from this splitter
	 */
	void RemoveChild( const TSharedRef<SDockingNode>& ChildToRemove );
	
	/**
	 * Remove a child at this index
	 *
	 * @param IndexOfChildToRemove    Remove a child at this index
	 */
	void RemoveChildAt( int32 IndexOfChildToRemove );

	/**
	 * Inserts NodeToPlace next to RelativeToMe.
	 * Next to means above, below, left or right of RelativeToMe.
	 *
	 * @param NodeToPlace        The node to insert.
	 * @param Direction          Where to insert relative to the other node
	 * @param RelativeToMove     Insert relative to this node.
	 */
	void PlaceNode( const TSharedRef<SDockingNode>& NodeToPlace, SDockingNode::RelativeDirection Direction, const TSharedRef<SDockingNode>& RelativeToMe );

	/** Sets the orientation of this splitter */
	void SetOrientation(EOrientation NewOrientation);

	/** Gets an array of all child dock nodes */
	const TArray< TSharedRef<SDockingNode> >& GetChildNodes() const;

	/** Gets an array of all child dock nodes and all of their child dock nodes and so on */
	TArray< TSharedRef<SDockingNode> > GetChildNodesRecursively() const;
	
	/** Recursively searches through all children looking for child tabs */
	virtual TArray< TSharedRef<SDockTab> > GetAllChildTabs() const override;

	/** Gets the size coefficient of a given child dock node */
	float GetSizeCoefficientForSlot(int32 Index) const;

	/** Gets the orientation of this splitter */
	EOrientation GetOrientation() const;

	virtual TSharedPtr<FTabManager::FLayoutNode> GatherPersistentLayout() const override;

	/**
	 * The tab stack which would be appropriate to use for showing the minimize/maximize/close buttons.
	 * On Windows it is the upper right most. On Mac the upper left most.
	 */
	TSharedRef<SDockingTabStack> FindTabStackToHouseWindowControls() const;

	/** Which tab stack is appropriate for showing the app icon. */
	TSharedRef<SDockingTabStack> FindTabStackToHouseWindowIcon() const;

protected:

	/** Is the docking direction (left, right, above, below) match the orientation (horizontal vs. vertical) */
	static bool DoesDirectionMatchOrientation( SDockingNode::RelativeDirection InDirection, EOrientation InOrientation );

	static SDockingNode::ECleanupRetVal MostResponsibility( SDockingNode::ECleanupRetVal A, SDockingNode::ECleanupRetVal B );

	virtual SDockingNode::ECleanupRetVal CleanUpNodes() override;

	float ComputeChildCoefficientTotal() const;

	enum class ETabStackToFind
	{
		UpperLeft,
		UpperRight
	};

	/** Helper: Finds the upper left or the upper right tab stack in the hierarchy */
	TSharedRef<SDockingNode> FindTabStack(ETabStackToFind FindMe) const;

	/** The SSplitter widget that SDockingSplitter wraps. */
	TSharedPtr<SSplitter> Splitter;
	
	/** The DockNode children. All these children are kept in sync with the SSplitter's children via the use of the public interface for adding, removing and replacing children. */
	TArray< TSharedRef<class SDockingNode> > Children;
};


