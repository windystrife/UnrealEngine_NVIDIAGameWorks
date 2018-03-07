// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Widgets/SWidget.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "PropertyPath.h"

class FSequencerTrackNode;
class FMenuBuilder;
struct FSlateBrush;

/** Enumeration specifying what kind of object binding this is */
enum class EObjectBindingType
{
	Possessable, Spawnable, Unknown
};

/**
 * A node for displaying an object binding
 */
class FSequencerObjectBindingNode
	: public FSequencerDisplayNode
{
public:

	/**
	 * Create and initialize a new instance.
	 * 
	 * @param InNodeName The name identifier of then node.
	 * @param InObjectName The name of the object we're binding to.
	 * @param InObjectBinding Object binding guid for associating with live objects.
	 * @param InParentNode The parent of this node, or nullptr if this is a root node.
	 * @param InParentTree The tree this node is in.
	 */
	FSequencerObjectBindingNode(FName NodeName, const FText& InDisplayName, const FGuid& InObjectBinding, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree);

public:

	/** @return The object binding on this node */
	const FGuid& GetObjectBinding() const
	{
		return ObjectBinding;
	}

	/** Access the cached object binding type for this display node */
	EObjectBindingType GetBindingType() const
	{
		return BindingType;
	}

	/**
	 * Adds a new externally created node to this display node
	 *
	 * @param NewChild		The child node to add
	 */
	void AddTrackNode( TSharedRef<FSequencerTrackNode> NewChild );

public:

	// FSequencerDisplayNode interface

	virtual void BuildContextMenu(FMenuBuilder& MenuBuilder) override;
	virtual bool CanRenameNode() const override;
	virtual TSharedRef<SWidget> GetCustomOutlinerContent() override;
	virtual FText GetDisplayName() const override;
	virtual FLinearColor GetDisplayNameColor() const override;
	virtual FText GetDisplayNameToolTipText() const override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual const FSlateBrush* GetIconOverlayBrush() const override;
	virtual FText GetIconToolTipText() const override;
	virtual float GetNodeHeight() const override;
	virtual FNodePadding GetNodePadding() const override;
	virtual ESequencerNode::Type GetType() const override;
	virtual void SetDisplayName(const FText& NewDisplayName) override;
	virtual bool CanDrag() const override;

protected:

	void AddPropertyMenuItems(FMenuBuilder& AddTrackMenuBuilder, TArray<FPropertyPath> KeyableProperties, int32 PropertyNameIndexStart = 0, int32 PropertyNameIndexEnd = -1);

	void AddSpawnOwnershipMenu(FMenuBuilder& MenuBuilder);

	/** Get class for object binding */
	const UClass* GetClassForObjectBinding() const;

private:
	
	TSharedRef<SWidget> HandleAddTrackComboButtonGetMenuContent();
	
	void HandleAddTrackSubMenuNew(FMenuBuilder& AddTrackMenuBuilder, TArray<FPropertyPath> KeyablePropertyPath, int32 PropertyNameIndexStart = 0);

	void HandleLabelsSubMenuCreate(FMenuBuilder& MenuBuilder);
	
	void HandlePropertyMenuItemExecute(FPropertyPath PropertyPath);

private:

	/** The binding to live objects */
	FGuid ObjectBinding;

	/** The default display name of the object which is used if the binding manager doesn't provide one. */
	FText DefaultDisplayName;

	/** Enumeration specifying what kind of object binding this is */
	EObjectBindingType BindingType;
};
