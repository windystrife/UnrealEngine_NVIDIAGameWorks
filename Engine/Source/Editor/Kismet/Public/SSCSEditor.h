// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Templates/SubclassOf.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Styling/SlateColor.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "Engine/SCS_Node.h"
#include "BlueprintEditor.h"
#include "Widgets/SToolTip.h"
#include "SComponentClassCombo.h"

class FMenuBuilder;
class FSCSEditorTreeNode;
class SSCSEditor;
class UPrimitiveComponent;
struct EventData;

// SCS tree node pointer type
typedef TSharedPtr<class FSCSEditorTreeNode> FSCSEditorTreeNodePtrType;

/**
 * FSCSEditorTreeNode
 *
 * Wrapper class for component template nodes displayed in the SCS editor tree widget.
 */
class KISMET_API FSCSEditorTreeNode : public TSharedFromThis<FSCSEditorTreeNode>
{
public:
	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE(FOnRenameRequested);

	enum ENodeType
	{
		ComponentNode,
		RootActorNode,
		SeparatorNode,
	};

	/**
	 * Constructs an empty tree node.
	 */
	FSCSEditorTreeNode(FSCSEditorTreeNode::ENodeType InNodeType);

	/**
	* @return The name to identify this node.
	*/
	virtual FName GetNodeID() const;

	/**
	 * @return The name of the variable represented by this node.
	 */
	virtual FName GetVariableName() const;
	/**
	 * @return The string to be used in the tree display.
	 */
	virtual FString GetDisplayString() const;
	/**
	* @return The name of this node in text.
	*/
	virtual FText GetDisplayName() const;
	/**
	 * @return The SCS node that is represented by this object, or NULL if there is no SCS node associated with the component template.
	 */
	virtual class USCS_Node* GetSCSNode() const;
	/**
	 * @param ActualEditedBlueprint currently edited blueprint
	 *
	 * @return The component template that can be editable for actual class.
	 */
	virtual UActorComponent* GetEditableComponentTemplate(UBlueprint* ActualEditedBlueprint);
	/**
	 * Finds the component instance represented by this node contained within a given Actor instance.
	 *
	 * @param InActor The Actor instance to use as the container object for finding the component instance.
	 * @return The component instance represented by this node and contained within the given Actor instance, or NULL if not found.
	 */
	virtual UActorComponent* FindComponentInstanceInActor(const AActor* InActor) const;
	/**
	 * @return This object's parent node (or an invalid reference if no parent is assigned).
	 */
	FSCSEditorTreeNodePtrType GetParent() const { return ParentNodePtr; }
	/**
	 * @return The set of nodes which are parented to this node (read-only).
	 */
	const TArray<FSCSEditorTreeNodePtrType>& GetChildren() const { return Children; }
	/**
	 * @return Type of node
	 */
	ENodeType GetNodeType() const;
	/**
	 * @param	bEvenIfPendingKill	If false, nullptr will be returned if the cached component template is pending kill.
	 *								If true, it will be returned regardless (this is used for recaching the component template if the objects
	 *								have been reinstanced following construction script execution).
	 *
	 * @note	Deliberately non-virtual, for performance reasons.
	 * @return	The component template or instance represented by this node, if it's a component node.
	 */
	UActorComponent* GetComponentTemplate(bool bEvenIfPendingKill = false) const;
	/**
	 * Set the component template represented by this node, if it's a component node.
	 */
	void SetComponentTemplate(UActorComponent* Component);
	/**
	 * @return Whether or not this node is a direct child of the given node.
	 */
	bool IsDirectlyAttachedTo(FSCSEditorTreeNodePtrType InNodePtr) const { return ParentNodePtr == InNodePtr; }
	/**
	 * @return Whether or not this node is a child (direct or indirect) of the given node.
	 */
	bool IsAttachedTo(FSCSEditorTreeNodePtrType InNodePtr) const;	

	/**
	 * Finds the closest ancestor node in the given node set.
	 *
	 * @param InNodes The given node set.
	 * @return One of the nodes from the set, or an invalid node reference if the set does not contain any ancestor nodes.
	 */
	FSCSEditorTreeNodePtrType FindClosestParent(TArray<FSCSEditorTreeNodePtrType> InNodes);

	/**
	 * Adds the given node as a child node.
	 *
	 * @param InChildNodePtr The node to add as a child node.
	 */
	void AddChild(FSCSEditorTreeNodePtrType InChildNodePtr);

	/**
	 * Adds a child node for the given SCS node.
	 *
	 * @param InSCSNode The SCS node to for which to create a child node.
	 * @param bIsInheritedSCS Whether or not the given SCS node is inherited from a parent.
	 * @return A reference to the new child node or the existing child node if a match is found.
	 */
	FSCSEditorTreeNodePtrType AddChild(USCS_Node* InSCSNode, bool bIsInheritedSCS);

	/**
	 * Adds a child node for the given component template.
	 *
	 * @param InComponentTemplate The component template for which to create a child node.
	 * @return A reference to the new child node or the existing child node if a match is found.
	 */
	FSCSEditorTreeNodePtrType AddChildFromComponent(UActorComponent* InComponentTemplate);

	/**
	 * Attempts to find a reference to the child node that matches the given SCS node.
	 *
	 * @param InSCSNode The SCS node to match.
	 * @param bRecursiveSearch Whether or not to recursively search child nodes (default == false).
	 * @param OutDepth If non-NULL, the depth of the child node will be returned in this parameter on success (default == NULL).
	 * @return The child node that matches the given SCS node, or an invalid node reference if no match was found.
	 */
	FSCSEditorTreeNodePtrType FindChild(const USCS_Node* InSCSNode, bool bRecursiveSearch = false, uint32* OutDepth = NULL) const;

	/**
	 * Attempts to find a reference to the child node that matches the given component template.
	 *
	 * @param InComponentTemplate The component template instance to match.
	 * @param bRecursiveSearch Whether or not to recursively search child nodes (default == false).
	 * @param OutDepth If non-NULL, the depth of the child node will be returned in this parameter on success (default == NULL).
	 * @return The child node with a component template that matches the given component template instance, or an invalid node reference if no match was found.
	 */
	FSCSEditorTreeNodePtrType FindChild(const UActorComponent* InComponentTemplate, bool bRecursiveSearch = false, uint32* OutDepth = NULL) const;

	/**
	 * Attempts to find a reference to the child node that matches the given component variable or instance name.
	 *
	 * @param InVariableOrInstanceName The component variable or instance name to match.
	 * @param bRecursiveSearch Whether or not to recursively search child nodes (default == false).
	 * @param OutDepth If non-NULL, the depth of the child node will be returned in this parameter on success (default == NULL).
	 * @return The child node with a component variable or instance name that matches the given name, or an invalid node reference if no match was found.
	 */
	FSCSEditorTreeNodePtrType FindChild(const FName& InVariableOrInstanceName, bool bRecursiveSearch = false, uint32* OutDepth = NULL) const;

	/**
	 * Removes the given node from the list of child nodes.
	 *
	 * @param InChildNodePtr The child node to remove.
	 */
	void RemoveChild(FSCSEditorTreeNodePtrType InChildNodePtr);

	bool IsSceneComponent() const
	{
		return Cast<USceneComponent>(GetComponentTemplate()) != nullptr;
	}

	// Tries to find a SCS node that was likely responsible for creating the specified instance component.  Note: This is not always possible to do!
	static USCS_Node* FindSCSNodeForInstance(UActorComponent* InstanceComponent, UClass* ClassToSearch);

	/**
	 * Creates the correct type of node based on the component (instanced or not, etc...)
	 */
	static FSCSEditorTreeNodePtrType FactoryNodeFromComponent(UActorComponent* InComponent);

	// Destructor
	virtual ~FSCSEditorTreeNode() {}
protected:
	// Called when this node is being removed via a RemoveChild call
	virtual void RemoveMeAsChild() {}


public:



	/**
	* @return The Blueprint to which this node belongs.
	*/
	UBlueprint* GetBlueprint() const;

	/**
	 * @return Whether or not this object represents a "native" component template (i.e. one that is not found in the SCS tree).
	 */
	virtual bool IsNative() const { return false; }

	/**
	 * @return Whether or not this object represents a root component.
	 */
	virtual bool IsRootComponent() const { return false; }

	/**
	 * @return Whether or not this object represents an inherited SCS node (one from a SCS node in a parent Blueprint).
	 */
	virtual bool IsInheritedSCS() const { return false; }

	/**
	 * @return Whether or not this object was declared in the current class (or instance).  Anything inherited cannot be reorganized (renamed, reparented, etc...).
	 */
	virtual bool IsInherited() const
	{
		return IsNative() || IsInheritedSCS();
	}

	/**
	 * @return Whether or not this object represents a component instance rather than a template.
	 */
	virtual bool IsInstanced() const { return false; }

	/**
	 * @return Whether or not this object represents a component instance that was created by the user and not by a native or Blueprint-generated class.
	 */
	virtual bool IsUserInstanced() const { return false; }

	/**
	* @return Whether or not this object represents the default SCS scene root component.
	*/
	virtual bool IsDefaultSceneRoot() const { return false; }

	/**
	 * @return Whether or not this object represents a node that can be deleted from the SCS tree.
	 */
	virtual bool CanDelete() const { return false; }

	/**
	 * @return Whether or not this object represents a node that can be reparented to other nodes based on its context.
	 */
	virtual bool CanReparent() const { return false; }

	/**
	 * @return Whether or not we can edit default properties for the component template represented by this object.
	 */
	virtual bool CanEditDefaults() const { return false; }

	/**
	 * @return Whether or not this object represents a node that can be renamed from the components tree.
	 */
	virtual bool CanRename() const { return false; }

	/** Requests a rename on the component */
	void OnRequestRename(bool bTransactional);

	/** Renames the component */
	virtual void OnCompleteRename(const FText& InNewName);

	/** Sets up the delegate for renaming a component */
	void SetRenameRequestedDelegate(FOnRenameRequested InRenameRequested) { RenameRequestedDelegate = InRenameRequested; }

	/** Query that determines if this item should be filtered out or not */
	bool IsFlaggedForFiltration() const 
	{
		return ensureMsgf(FilterFlags != EFilteredState::Unknown, TEXT("Querying a bad filtration state.")) ? 
			(FilterFlags & EFilteredState::FilteredInMask) == 0 : false; 
	}

	/** Refreshes this item's filtration state. Use bUpdateParent to make sure the parent's EFilteredState::ChildMatches flag is properly updated based off the new state */
	void UpdateCachedFilterState(bool bMatchesFilter, bool bUpdateParent);

protected:
	/** Updates the EFilteredState::ChildMatches flag, based off of children's current state */
	void RefreshCachedChildFilterState(bool bUpdateParent);
	/** Used to update the EFilteredState::ChildMatches flag for parent nodes, when this item's filtration state has changed */
	void ApplyFilteredStateToParent();
	
	bool GetAndClearNonTransactionalRenameFlag()
	{
		const bool bResult = bNonTransactionalRename;
		bNonTransactionalRename = false;
		return bResult;
	}

	// Component template represented by this node, if it's a component node, otherwise invalid
	TWeakObjectPtr<UActorComponent> ComponentTemplatePtr;

private:
	// The type of component tree node
	ENodeType NodeType;

	// Actual tree structure
	FSCSEditorTreeNodePtrType ParentNodePtr;
	TArray<FSCSEditorTreeNodePtrType> Children;

	/** Handles rename requests */
	bool bNonTransactionalRename;
	FOnRenameRequested RenameRequestedDelegate;

	enum EFilteredState
	{
		FilteredOut    = 0x00,
		MatchesFilter  = (1 << 0),
		ChildMatches   = (1 << 1),

		FilteredInMask = (MatchesFilter | ChildMatches),
		Unknown = 0xFC // ~FilteredInMask
	};
	uint8 FilterFlags;
};

//////////////////////////////////////////////////////////////////////////
//

class KISMET_API FSCSEditorTreeNodeComponentBase : public FSCSEditorTreeNode
{
protected:
	FSCSEditorTreeNodeComponentBase()
		: FSCSEditorTreeNode(FSCSEditorTreeNode::ComponentNode)
	{
	}

public:
	// FSCSEditorTreeNode interface
	virtual FName GetVariableName() const override;
	virtual FString GetDisplayString() const override;
	virtual bool CanRename() const override { return !IsInherited() && !IsDefaultSceneRoot(); }
	virtual bool CanDelete() const override { return !IsInherited() && !IsDefaultSceneRoot(); }
	virtual bool CanReparent() const override { return !IsInherited() && !IsDefaultSceneRoot() && IsSceneComponent(); }
	// End of FSCSEditorTreeNode interface
};


//////////////////////////////////////////////////////////////////////////
// FSCSEditorTreeNodeInstancedInheritedComponent - A inherited component in the instanced case (either an inherited SCS node or an inherited native component)

class KISMET_API FSCSEditorTreeNodeInstancedInheritedComponent : public FSCSEditorTreeNodeComponentBase
{
public:
	/**
	 * Constructs a wrapper around a component template not contained within an SCS tree node (e.g. "native" components).
	 *
	 * @param InComponentTemplate The component template represented by this object.
	 */
	FSCSEditorTreeNodeInstancedInheritedComponent(AActor* Owner, FName InComponentName);

	// FSCSEditorTreeNode public interface
	virtual bool IsNative() const override;
	virtual bool IsRootComponent() const override;
	virtual bool IsInheritedSCS() const override;
	virtual bool IsInstanced() const override { return true; }
	virtual bool IsInherited() const override { return true; }
	virtual bool IsUserInstanced() const override { return false; }
	virtual bool IsDefaultSceneRoot() const override;
	virtual bool CanEditDefaults() const override;
	//virtual FName GetVariableName() const override;
	//virtual FString GetDisplayString() const override;
	virtual FText GetDisplayName() const override;
	virtual UActorComponent* GetEditableComponentTemplate(UBlueprint* ActualEditedBlueprint) override;
	// End of FSCSEditorTreeNode public interface

private:
	FName InstancedComponentName;
	TWeakObjectPtr<AActor> InstancedComponentOwnerPtr;
};

//////////////////////////////////////////////////////////////////////////
// FSCSEditorTreeNodeInstanceAddedComponent - A unique-to-this instance component

class KISMET_API FSCSEditorTreeNodeInstanceAddedComponent : public FSCSEditorTreeNodeComponentBase
{
public:
	/**
	* Constructs a wrapper around a component template not contained within an SCS tree node (e.g. "native" components).
	*
	* @param InComponentTemplate The component template represented by this object.
	*/
	FSCSEditorTreeNodeInstanceAddedComponent(AActor* Owner, FName InComponentName);

	// FSCSEditorTreeNode public interface
	virtual bool IsNative() const override { return false; }
	virtual bool IsRootComponent() const override;
	virtual bool IsInheritedSCS() const override { return false; }
	virtual bool IsInstanced() const override { return true; }
	virtual bool IsUserInstanced() const override { return true; }
	virtual bool IsDefaultSceneRoot() const override;
	virtual bool CanEditDefaults() const override { return true; }
	virtual FName GetVariableName() const override { return NAME_None; }
	virtual FString GetDisplayString() const override;
	virtual FText GetDisplayName() const override;
	virtual UActorComponent* GetEditableComponentTemplate(UBlueprint* ActualEditedBlueprint) override;
	virtual void OnCompleteRename(const FText& InNewName) override;
	// End of FSCSEditorTreeNode public interface

protected:
	// FSCSEditorTreeNode protected interface
	virtual void RemoveMeAsChild() override;
	// End of FSCSEditorTreeNode protected interface

private:
	FName InstancedComponentName;
	TWeakObjectPtr<AActor> InstancedComponentOwnerPtr;
};

//////////////////////////////////////////////////////////////////////////
// FSCSEditorTreeNodeComponent - A generic component in the non-instanced case (either a SCS node or an inherited native component)

class KISMET_API FSCSEditorTreeNodeComponent : public FSCSEditorTreeNodeComponentBase
{
public:
	/**
	 * Constructs a wrapper around a component template contained within an SCS tree node.
	 *
	 * @param InSCSNode The SCS tree node represented by this object.
	 * @param bInIsInherited Whether or not the SCS tree node is inherited from a parent Blueprint class.
	 */
	FSCSEditorTreeNodeComponent(class USCS_Node* InSCSNode, bool bInIsInherited = false);

	/**
	 * Constructs a wrapper around a component template not contained within an SCS tree node (e.g. "native" components).
	 *
	 * @param InComponentTemplate The component template represented by this object.
	 */
	FSCSEditorTreeNodeComponent(UActorComponent* InComponentTemplate);


	// FSCSEditorTreeNode public interface
	virtual bool IsNative() const override;
	virtual bool IsRootComponent() const override;
	virtual bool IsInheritedSCS() const override;
	virtual bool IsInstanced() const override { return false; }
	virtual bool IsUserInstanced() const override { return false; }
	virtual bool IsDefaultSceneRoot() const override;
	virtual bool CanEditDefaults() const override;
	//virtual FName GetVariableName() const override;
	//virtual FString GetDisplayString() const override;
	virtual FText GetDisplayName() const override;
	virtual class USCS_Node* GetSCSNode() const override;
	virtual UActorComponent* GetEditableComponentTemplate(UBlueprint* ActualEditedBlueprint) override;
	virtual void OnCompleteRename(const FText& InNewName) override;
	// End of FSCSEditorTreeNode public interface

protected:
	// FSCSEditorTreeNode protected interface
	virtual void RemoveMeAsChild() override;
	// End of FSCSEditorTreeNode protected interface

	/** Get overridden template component, specialized in given blueprint */
	UActorComponent* INTERNAL_GetOverridenComponentTemplate(UBlueprint* Blueprint, bool bCreateIfNecessary) const;

private:
	// Was this component inherited from a parent class or introduced in this class?
	bool bIsInheritedSCS;

	// Is this the template coming from an SCS node?
	TWeakObjectPtr<class USCS_Node> SCSNodePtr;
};


class KISMET_API FSCSEditorTreeNodeRootActor : public FSCSEditorTreeNode
{
public:
	FSCSEditorTreeNodeRootActor(AActor* InActor, bool bInAllowRename)
		: FSCSEditorTreeNode(FSCSEditorTreeNode::RootActorNode)
		, Actor(InActor)
		, bAllowRename(bInAllowRename)
	{
	}

	// FSCSEditorTreeNode public interface
	virtual FName GetNodeID() const override;
	virtual bool CanRename() const override { return bAllowRename; }
	virtual void OnCompleteRename(const FText& InNewName) override;
	// End of FSCSEditorTreeNode public interface

private:
	AActor* Actor;
	bool bAllowRename;
};

class KISMET_API FSCSEditorTreeNodeSeparator : public FSCSEditorTreeNode
{
public:
	FSCSEditorTreeNodeSeparator()
		: FSCSEditorTreeNode(FSCSEditorTreeNode::SeparatorNode)
	{
	}
};

//////////////////////////////////////////////////////////////////////////
// SSCS_RowWidget

class SSCS_RowWidget : public SMultiColumnTableRow<FSCSEditorTreeNodePtrType>
{
public:
	SLATE_BEGIN_ARGS( SSCS_RowWidget ){}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedPtr<SSCSEditor> InSCSEditor, FSCSEditorTreeNodePtrType InNodePtr, TSharedPtr<STableViewBase> InOwnerTableView  );
	virtual ~SSCS_RowWidget();

	// SMultiColumnTableRow<T> interface
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;
	// End of SMultiColumnTableRow<T>

	// SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	// End of SWidget interface

	/** Get the blueprint we are editing */
	UBlueprint* GetBlueprint() const;

	FText GetNameLabel() const;
	FText GetTooltipText() const;
	FSlateColor GetColorTintForIcon() const;
	FSlateColor GetColorTintForText() const;
	FString GetDocumentationLink() const;
	FString GetDocumentationExcerptName() const;
	
	KISMET_API static FSlateColor GetColorTintForIcon(FSCSEditorTreeNodePtrType InNode);

	FText GetAssetName() const;
	FText GetAssetPath() const;
	EVisibility GetAssetVisibility() const;

	/* Get the node used by the row Widget */
	virtual FSCSEditorTreeNodePtrType GetNode() const { return TreeNodePtr; };

protected:
	virtual ESelectionMode::Type GetSelectionMode() const override;

	static void AddToToolTipInfoBox(const TSharedRef<SVerticalBox>& InfoBox, const FText& Key, TSharedRef<SWidget> ValueIcon, const TAttribute<FText>& Value, bool bImportant);

	/** Commits the new name of the component */
	void OnNameTextCommit(const FText& InNewName, ETextCommit::Type InTextCommit);

private:
	/** Verifies the name of the component when changing it */
	bool OnNameTextVerifyChanged(const FText& InNewText, FText& OutErrorMessage);

	/** Builds a context menu popup for dropping a child node onto the scene root node */
	TSharedPtr<SWidget> BuildSceneRootDropActionMenu(FSCSEditorTreeNodePtrType DroppedNodePtr);

	/** Creates a tooltip for this row */
	TSharedRef<SToolTip> CreateToolTipWidget() const;

	/** Drag-drop handlers */
	void HandleOnDragEnter(const FDragDropEvent& DragDropEvent);
	void HandleOnDragLeave(const FDragDropEvent& DragDropEvent);
	FReply HandleOnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	TOptional<EItemDropZone> HandleOnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, FSCSEditorTreeNodePtrType TargetItem);
	FReply HandleOnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, FSCSEditorTreeNodePtrType TargetItem);

	/** Handler for attaching a single node to this node */
	void OnAttachToDropAction(FSCSEditorTreeNodePtrType DroppedNodePtr)
	{
		TArray<FSCSEditorTreeNodePtrType> DroppedNodePtrs;
		DroppedNodePtrs.Add(DroppedNodePtr);
		OnAttachToDropAction(DroppedNodePtrs);
	}

	/** Handler for attaching one or more nodes to this node */
	void OnAttachToDropAction(const TArray<FSCSEditorTreeNodePtrType>& DroppedNodePtrs);

	/** Handler for detaching one or more nodes from the current parent and reattaching to the existing scene root node */
	void OnDetachFromDropAction(const TArray<FSCSEditorTreeNodePtrType>& DroppedNodePtrs);

	/** Handler for making the given node the new scene root node */
	void OnMakeNewRootDropAction(FSCSEditorTreeNodePtrType DroppedNodePtr);
	
	/** Tasks to perform after handling a drop action */
	void PostDragDropAction(bool bRegenerateTreeNodes);

	/**
	 * Retrieves an image brush signifying the specified component's mobility (could sometimes be NULL).
	 * 
	 * @returns A pointer to the FSlateBrush to use (NULL for Static and Non-SceneComponents)
	 */
	FSlateBrush const* GetMobilityIconImage() const;

	/**
	 * Retrieves tooltip text describing the specified component's mobility.
	 * 
	 * @returns An FText object containing a description of the component's mobility
	 */
	FText GetMobilityToolTipText() const;

	/**
	 * Retrieves tooltip text describing where the component was first introduced (for inherited components).
	 * 
	 * @returns An FText object containing a description of when the component was first introduced
	 */
	FText GetIntroducedInToolTipText() const;

	/**
	 * Retrieves tooltip text describing how the component was introduced
	 * 
	 * @returns An FText object containing a description of when the component was first introduced
	 */
	FText GetComponentAddSourceToolTipText() const;

public:
	/** Pointer back to owning SCSEditor 2 tool */
	TWeakPtr<SSCSEditor> SCSEditor;
	TSharedPtr<SInlineEditableTextBlock> InlineWidget;
private:
	/** Pointer to node we represent */
	FSCSEditorTreeNodePtrType TreeNodePtr;
};

class SSCS_RowWidget_ActorRoot : public SSCS_RowWidget
{
public:

	// SMultiColumnTableRow<T> interface
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	// End of SMultiColumnTableRow<T>

private:
	/** Creates a tooltip for this row */
	TSharedRef<SToolTip> CreateToolTipWidget() const;

	/** Called to validate the actor name */
	bool OnVerifyActorLabelChanged(const FText& InLabel, FText& OutErrorMessage);

	/** Data accessors */
	const FSlateBrush* GetActorIcon() const;
	FText GetActorDisplayText() const;
	FText GetActorContextText() const;
	FText GetActorClassNameText() const;
	FText GetActorSuperClassNameText() const;
	FText GetActorMobilityText() const;
};

class SSCS_RowWidget_Separator : public SSCS_RowWidget
{
public:

	// SMultiColumnTableRow<T> interface
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	// End of SMultiColumnTableRow<T>

private:

};

//////////////////////////////////////////////////////////////////////////
// SSCSEditorDragDropTree - implements STreeView for our specific node type and adds drag/drop functionality
class SSCSEditorDragDropTree : public STreeView<FSCSEditorTreeNodePtrType>
{
public:
	SLATE_BEGIN_ARGS( SSCSEditorDragDropTree )
		: _SCSEditor( NULL )
		, _OnGenerateRow()
		, _OnGetChildren()
		, _OnSetExpansionRecursive()
		, _TreeItemsSource( static_cast< TArray<FSCSEditorTreeNodePtrType>* >(NULL) ) //@todo Slate Syntax: Initializing from NULL without a cast
		, _ItemHeight(16)
		, _OnContextMenuOpening()
		, _OnMouseButtonDoubleClick()
		, _OnSelectionChanged()
		, _OnExpansionChanged()
		, _SelectionMode(ESelectionMode::Multi)
		, _ClearSelectionOnClick(true)
		, _ExternalScrollbar()
		, _OnTableViewBadState()
		{}

		SLATE_ARGUMENT( SSCSEditor*, SCSEditor )

		SLATE_EVENT( FOnGenerateRow, OnGenerateRow )

		SLATE_EVENT( FOnItemScrolledIntoView, OnItemScrolledIntoView )

		SLATE_EVENT( FOnGetChildren, OnGetChildren )

		SLATE_EVENT( FOnSetExpansionRecursive, OnSetExpansionRecursive )

		SLATE_ARGUMENT( TArray<FSCSEditorTreeNodePtrType>* , TreeItemsSource )

		SLATE_ATTRIBUTE( float, ItemHeight )

		SLATE_EVENT( FOnContextMenuOpening, OnContextMenuOpening )

		SLATE_EVENT( FOnMouseButtonDoubleClick, OnMouseButtonDoubleClick )

		SLATE_EVENT( FOnSelectionChanged, OnSelectionChanged )

		SLATE_EVENT( FOnExpansionChanged, OnExpansionChanged )

		SLATE_ATTRIBUTE( ESelectionMode::Type, SelectionMode )

		SLATE_ARGUMENT( TSharedPtr<SHeaderRow>, HeaderRow )

		SLATE_ARGUMENT ( bool, ClearSelectionOnClick )

		SLATE_ARGUMENT( TSharedPtr<SScrollBar>, ExternalScrollbar )

		SLATE_EVENT( FOnTableViewBadState, OnTableViewBadState )

	SLATE_END_ARGS()
	/** Object construction - mostly defers to the base STreeView */
	void Construct( const FArguments& InArgs );

	// SWidget interface
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	// End SWidget interface

private:
	/** Pointer to the SSCSEditor that owns this widget */
	SSCSEditor* SCSEditor;
};

//////////////////////////////////////////////////////////////////////////
// SSCSEditor

typedef SSCSEditorDragDropTree SSCSTreeType;

/* Component editor mode */
namespace EComponentEditorMode
{
	enum Type
	{
		/* View/edit the SCS in a BGPC */
		BlueprintSCS,
		/* View/edit the Actor instance */
		ActorInstance
	};
};

class KISMET_API SSCSEditor : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(class USCS_Node*, FOnAddNewComponent, class UClass*);
	DECLARE_DELEGATE_RetVal_OneParam(class USCS_Node*, FOnAddExistingComponent, class UActorComponent*);
	DECLARE_DELEGATE_OneParam(FOnSelectionUpdated, const TArray<FSCSEditorTreeNodePtrType>&);
	DECLARE_DELEGATE_OneParam(FOnItemDoubleClicked, const FSCSEditorTreeNodePtrType);
	DECLARE_DELEGATE_OneParam(FOnHighlightPropertyInDetailsView, const class FPropertyPath&);

	SLATE_BEGIN_ARGS( SSCSEditor )
		:_EditorMode(EComponentEditorMode::BlueprintSCS)
		,_IsDiffing(false)
		,_ActorContext(nullptr)
		,_PreviewActor(nullptr)
		,_AllowEditing(true)
		,_HideComponentClassCombo(false)
		,_OnSelectionUpdated()
		,_OnHighlightPropertyInDetailsView()
		{}

		SLATE_ARGUMENT(EComponentEditorMode::Type, EditorMode)
		SLATE_ARGUMENT(bool, IsDiffing)
		SLATE_ATTRIBUTE(class AActor*, ActorContext)
		SLATE_ATTRIBUTE(class AActor*, PreviewActor)
		SLATE_ATTRIBUTE(bool, AllowEditing)
		SLATE_ATTRIBUTE(bool, HideComponentClassCombo)
		SLATE_EVENT(FOnSelectionUpdated, OnSelectionUpdated)
		SLATE_EVENT(FOnItemDoubleClicked, OnItemDoubleClicked)
		SLATE_EVENT(FOnHighlightPropertyInDetailsView, OnHighlightPropertyInDetailsView)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** SWidget interface */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent );

	/** Used by tree control - make a widget for a table row from a node */
	TSharedRef<ITableRow> MakeTableRowWidget(FSCSEditorTreeNodePtrType InNodePtr, const TSharedRef<STableViewBase>& OwnerTable);

	/** Used by tree control - get children for a specified node */
	void OnGetChildrenForTree(FSCSEditorTreeNodePtrType InNodePtr, TArray<FSCSEditorTreeNodePtrType>& OutChildren);

	/** Returns true if editing is allowed */
	bool IsEditingAllowed() const;

	/** Adds a component to the SCS Table
	   @param NewComponentClass				(In) The class to add
	   @param Asset       					(In) Optional asset to assign to the component
	   @param bSkipMarkBlueprintModified 	(In) Optionally skip marking this blueprint as modified (e.g. if we're handling that externally)
	   @return The reference of the newly created ActorComponent */
	UActorComponent* AddNewComponent(UClass* NewComponentClass, UObject* Asset, const bool bSkipMarkBlueprintModified = false, bool bSetFocusToNewItem = true);

	/** Adds a new SCS Node to the component Table
	   @param NewNode	(In) The SCS node to add
	   @param Asset		(In) Optional asset to assign to the component
	   @param bMarkBlueprintModified (In) Whether or not to mark the Blueprint as structurally modified
	   @param bSetFocusToNewItem (In) Select the new item and activate the inline rename widget (default is true)
	   @return The reference of the newly created ActorComponent */
	UActorComponent* AddNewNode(USCS_Node* NewNode, UObject* Asset, bool bMarkBlueprintModified, bool bSetFocusToNewItem = true);

	/** Adds a new component instance node to the component Table
		@param NewInstanceComponent	(In) The component being added to the actor instance
		@param Asset (In) Optional asset to assign to the component
		@param bSetFocusToNewItem (In) Select the new item and activate the inline rename widget (default is true)
		@return The reference of the newly created ActorComponent */
	UActorComponent* AddNewNodeForInstancedComponent(UActorComponent* NewInstanceComponent, UObject* Asset, bool bSetFocusToNewItem = true);
	
	/** Returns true if the specified component is currently selected */
	bool IsComponentSelected(const UPrimitiveComponent* PrimComponent) const;

	/** Assigns a selection override delegate to the specified component */
	void SetSelectionOverride(UPrimitiveComponent* PrimComponent) const;

	/** Cut selected node(s) */
	void CutSelectedNodes();
	bool CanCutNodes() const;

	/** Copy selected node(s) */
	void CopySelectedNodes();
	bool CanCopyNodes() const;

	/** Pastes previously copied node(s) */
	void PasteNodes();
	bool CanPasteNodes() const;

	/** Callbacks to duplicate the selected component */
	bool CanDuplicateComponent() const;
	void OnDuplicateComponent();

	/** Removes existing selected component nodes from the SCS */
	void OnDeleteNodes();
	bool CanDeleteNodes() const;

	/** Callbacks to find references of the selected component */
	void OnFindReferences();

	/** Removes an existing component node from the tree */
	void RemoveComponentNode(FSCSEditorTreeNodePtrType InNodePtr);

	/** Called when selection in the tree changes */
	void OnTreeSelectionChanged(FSCSEditorTreeNodePtrType InSelectedNodePtr, ESelectInfo::Type SelectInfo);

	/** Called when the Actor is selected. */
	void OnActorSelected(const ECheckBoxState NewCheckedState);

	/** Called to determine if actor is selected. */
	ECheckBoxState OnIsActorSelected() const;

	/** Update any associated selection (e.g. details view) from the passed in nodes */
	void UpdateSelectionFromNodes(const TArray<FSCSEditorTreeNodePtrType> &SelectedNodes );

	/** Refresh the tree control to reflect changes in the SCS */
	void UpdateTree(bool bRegenerateTreeNodes = true);

	/** Dumps out the tree view contents to the log (used to assist with debugging widget hierarchy issues) */
	void DumpTree();

	/** Forces the details panel to refresh on the same objects */
	void RefreshSelectionDetails();

	/** Clears the current selection */
	void ClearSelection();

	/** Get the currently selected tree nodes */
	TArray<FSCSEditorTreeNodePtrType> GetSelectedNodes() const;

	/**
	 * Fills out an events section in ui.
	 * @param Menu								the menu to add the events section into
	 * @param Blueprint							the active blueprint context being edited
	 * @param SelectedClass						the common component class to build the events list from
	 * @param CanExecuteActionDelegate			the delegate to query whether or not to execute the UI action
	 * @param GetSelectedObjectsDelegate		the delegate to fill the currently select variables / components
	 */
	static void BuildMenuEventsSection( FMenuBuilder& Menu, UBlueprint* Blueprint, UClass* SelectedClass, FCanExecuteAction CanExecuteActionDelegate, FGetSelectedObjectsDelegate GetSelectedObjectsDelegate );

	/**
	 * Given an actor component, attempts to find an associated tree node.
	 *
	 * @param ActorComponent The component associated with the node.
	 * @param bIncludeAttachedComponents Whether or not to include components attached to each node in the search (default is true).
	 * @return A shared pointer to a tree node. The pointer will be invalid if no match could be found.
	 */
	FSCSEditorTreeNodePtrType GetNodeFromActorComponent(const UActorComponent* ActorComponent, bool bIncludeAttachedComponents = true) const;

	/** Select the root of the tree */
	void SelectRoot();

	/** Select the given tree node */
	void SelectNode(FSCSEditorTreeNodePtrType InNodeToSelect, bool IsCntrlDown);

	/**
	 * Set the expansion state of a node
	 *
	 * @param InNodeToChange	The node to be expanded/collapsed
	 * @param bIsExpanded		True to expand the node, false to collapse it
	 */
	void SetNodeExpansionState(FSCSEditorTreeNodePtrType InNodeToChange, const bool bIsExpanded);

	/**
	 * Highlight a tree node and, optionally, a property with in it
	 *
	 * @param TreeNodeName		Name of the treenode to be highlighted
	 * @param Property	The name of the property to be highlighted in the details view
	 * @return True if the node was found in this Editor, otherwise false
	 */
	void HighlightTreeNode( FName TreeNodeName, const class FPropertyPath& Property );
	/**
	 * Highlight a tree node and, optionally, a property with in it
	 *
	 * @param Node		 A Reference to the Node SCS_Node to be highlighted
	 * @param Property	The name of the property to be highlighted in the details view
	 */
	void HighlightTreeNode( const USCS_Node* Node, FName Property );

	/**
	 * Function to save current state of SimpleConstructionScript and nodes associated with it.
	 *
	 * @param: SimpleContructionScript object reference.
	 */
	static void SaveSCSCurrentState( USimpleConstructionScript* SCSObj );

	/**
	 * Function to save the current state of SCS_Node and its children
	 *
	 * @param: Reference of the SCS_Node to be saved
	 */
	static void SaveSCSNode(USCS_Node* Node);

	/** Is this node still used by the Simple Construction Script */
	bool IsNodeInSimpleConstructionScript(USCS_Node* Node) const;

	/**
	 * Fills the supplied array with the currently selected objects
	 * @param OutSelectedItems The array to fill.
	 */
	void GetSelectedItemsForContextMenu(TArray<FComponentEventConstructionData>& OutSelectedItems) const;

	/** Provides access to the Blueprint context that's being edited */
	class UBlueprint* GetBlueprint() const;

	/** Returns the set of root nodes */
	const TArray<FSCSEditorTreeNodePtrType>& GetRootComponentNodes();

	/** @return The current editor mode (editing live actors or editing blueprints) */
	EComponentEditorMode::Type GetEditorMode() const { return EditorMode; }

	/** Try to handle a drag-drop operation */
	FReply TryHandleAssetDragDropOperation(const FDragDropEvent& DragDropEvent);

	/** Handler for recursively expanding/collapsing items */
	void SetItemExpansionRecursive(FSCSEditorTreeNodePtrType Model, bool bInExpansionState);

	/** Callback for the action trees to get the filter text */
	FText GetFilterText() const;

protected:
	FString GetSelectedClassText() const;

	/** Add a component from the selection in the combo box */
	UActorComponent* PerformComboAddClass(TSubclassOf<UActorComponent> ComponentClass, EComponentCreateAction::Type ComponentCreateAction, UObject* AssetOverride);

	/** Called to display context menu when right clicking on the widget */
	TSharedPtr< SWidget > CreateContextMenu();

	/** Called when the level editor requests a component to be renamed. */
	void OnLevelComponentRequestRename(const UActorComponent* InComponent);

	/** Checks to see if renaming is allowed on the selected component */
	bool CanRenameComponent() const;
	/**
	 * Requests a rename on the selected component
	 * @param bTransactional Whether or not the rename should be transactional (i.e. undoable)
	 */
	void OnRenameComponent(bool bTransactional);

	/** Called when component objects are replaced following construction script execution */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	/** Update component pointers held by tree nodes if components have been replaced following construction script execution */
	void ReplaceComponentReferencesInTree(const TArray<FSCSEditorTreeNodePtrType>& Nodes, const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	/**
	 * Function to create events for the current selection
	 * @param Blueprint						the active blueprint context
	 * @param EventName						the event to add
	 * @param GetSelectedObjectsDelegate	the delegate to gather information about current selection
	 * @param NodeIndex						an index to a specified node to add event for or < 0 for all selected nodes.
	 */
	static void CreateEventsForSelection(UBlueprint* Blueprint, FName EventName, FGetSelectedObjectsDelegate GetSelectedObjectsDelegate);

	/**
	 * Function to construct an event for a node
	 * @param Blueprint						the nodes blueprint
	 * @param EventName						the event to add
	 * @param EventData						the event data structure describing the node
	 */
	static void ConstructEvent(UBlueprint* Blueprint, const FName EventName, const FComponentEventConstructionData EventData);

	/**
	 * Function to view an event for a node
	 * @param Blueprint						the nodes blueprint
	 * @param EventName						the event to view
	 * @param EventData						the event data structure describing the node
	 */
	static void ViewEvent(UBlueprint* Blueprint, const FName EventName, const FComponentEventConstructionData EventData);

	/** Helper method to add a tree node for the given SCS node */
	FSCSEditorTreeNodePtrType AddTreeNode(USCS_Node* InSCSNode, FSCSEditorTreeNodePtrType InParentNodePtr, const bool bIsInheritedSCS);

	/** Helper method to add a tree node for the given scene component */
	FSCSEditorTreeNodePtrType AddTreeNodeFromComponent(USceneComponent* InSceneComponent);

	/** Helper method to recursively find a tree node for the given SCS node starting at the given tree node */
	FSCSEditorTreeNodePtrType FindTreeNode(const USCS_Node* InSCSNode, FSCSEditorTreeNodePtrType InStartNodePtr = FSCSEditorTreeNodePtrType()) const;

	/** Helper method to recursively find a tree node for the given scene component starting at the given tree node */
	FSCSEditorTreeNodePtrType FindTreeNode(const UActorComponent* InComponent, FSCSEditorTreeNodePtrType InStartNodePtr = FSCSEditorTreeNodePtrType()) const;

	/** Helper method to recursively find a tree node for the given variable or instance name starting at the given tree node */
	FSCSEditorTreeNodePtrType FindTreeNode(const FName& InVariableOrInstanceName, FSCSEditorTreeNodePtrType InStartNodePtr = FSCSEditorTreeNodePtrType()) const;

	/** Callback when a component item is scrolled into view */
	void OnItemScrolledIntoView( FSCSEditorTreeNodePtrType InItem, const TSharedPtr<ITableRow>& InWidget);

	/** Callback when a component item is double clicked. */
	void HandleItemDoubleClicked(FSCSEditorTreeNodePtrType InItem);

	/** Returns the set of expandable nodes that are currently collapsed in the UI */
	void GetCollapsedNodes(const FSCSEditorTreeNodePtrType& InNodePtr, TSet<FSCSEditorTreeNodePtrType>& OutCollapsedNodes) const;

	/** @return The visibility of the promote to blueprint button (only visible with an actor instance that is not created from a blueprint)*/
	EVisibility GetPromoteToBlueprintButtonVisibility() const;

	/** @return The visibility of the Edit Blueprint button (only visible with an actor instance that is created from a blueprint)*/
	EVisibility GetEditBlueprintButtonVisibility() const;

	/** @return the tooltip describing how many properties will be applied to the blueprint */
	FText OnGetApplyChangesToBlueprintTooltip() const;

	/** @return the tooltip describing how many properties will be reset to the blueprint default*/
	FText OnGetResetToBlueprintDefaultsTooltip() const;

	/** Opens the blueprint editor for the blueprint being viewed by the scseditor */
	void OnOpenBlueprintEditor(bool bForceCodeEditing) const;

	/** Propagates instance changes to the blueprint */
	void OnApplyChangesToBlueprint() const;

	/** Resets instance changes to the blueprint default */
	void OnResetToBlueprintDefaults() const;

	/** Converts the current actor instance to a blueprint */
	void PromoteToBlueprint() const;

	/** Called when the promote to blueprint button is clicked */
	FReply OnPromoteToBlueprintClicked();

	/** gets a root nodes of the tree */
	const TArray<FSCSEditorTreeNodePtrType>& GetRootNodes() const;

	/** Adds a root component tree node */
	TSharedPtr<FSCSEditorTreeNode> AddRootComponentTreeNode(UActorComponent* ActorComp);

	/**
	 * Creates a new C++ component from the specified class type
	 * The user will be prompted to pick a new subclass name and code will be recompiled
	 *
	 * @return The new class that was created
	 */
	UClass* CreateNewCPPComponent(TSubclassOf<UActorComponent> ComponentClass);
	
	/**
	 * Creates a new Blueprint component from the specified class type
	 * The user will be prompted to pick a new subclass name and a blueprint asset will be created
	 *
	 * @return The new class that was created
	 */
	UClass* CreateNewBPComponent(TSubclassOf<UActorComponent> ComponentClass);

	/** Recursively updates the filtered state for each component item */
	void OnFilterTextChanged(const FText& InFilterText);

	/** 
	 * Compares the filter bar's text with the item's component name. Use 
	 * bRecursive to refresh the state of child nodes as well. Returns true if 
	 * the node is set to be filtered out 
	 */
	bool RefreshFilteredState(FSCSEditorTreeNodePtrType TreeNode, bool bRecursive);

	/** 
	 * Iterates the RootNodes list, and uses the cached filtered state to 
	 * determine what items should be listed in the tree view. 
	 */
	void RebuildFilteredRootList();

public:
	/** Tree widget */
	TSharedPtr<SSCSTreeType> SCSTreeWidget;

	/** The node that represents the root component in the scene hierarchy */
	FSCSEditorTreeNodePtrType SceneRootNodePtr;

	/** Command list for handling actions in the SSCSEditor */
	TSharedPtr< FUICommandList > CommandList;

	/** Name of a node that has been requested to be renamed */
	FName DeferredRenameRequest;

	/** Whether or not the deferred rename request was flagged as transactional */
	bool bIsDeferredRenameRequestTransactional;

	/** Attribute that provides access to the Actor context for which we are viewing/editing the SCS. */
	TAttribute<class AActor*> ActorContext;

	/** Attribute that provides access to a "preview" Actor context (may not be same as the Actor context that's being edited. */
	TAttribute<class AActor*> PreviewActor;

	/** Attribute to indicate whether or not editing is allowed. */
	TAttribute<bool> AllowEditing;

	/** Delegate to invoke on selection update. */
	FOnSelectionUpdated OnSelectionUpdated;

	/** Delegate to invoke when an item in the tree is double clicked. */
	FOnItemDoubleClicked OnItemDoubleClicked;

	/** Delegate to invoke when the given property should be highlighted in the details view (e.g. diff). */
	FOnHighlightPropertyInDetailsView OnHighlightPropertyInDetailsView;

	/** Returns the Actor context for which we are viewing/editing the SCS.  Can return null.  Should not be cached as it may change from frame to frame. */
	class AActor* GetActorContext() const;
private:
	/** Indicates which editor mode we're in. */
	EComponentEditorMode::Type EditorMode;

	/** Root set of tree */
	TArray<FSCSEditorTreeNodePtrType> RootNodes;

	/** Root set of components (contains the root scene component and any non-scene component nodes) */
	TArray<FSCSEditorTreeNodePtrType> RootComponentNodes;

	/** The list of nodes used for the UI (a filtered version of RootNodes) */
	TArray<FSCSEditorTreeNodePtrType> FilteredRootNodes;

	/* Root Tree Node (for scene components) */
	TSharedPtr<FSCSEditorTreeNode> RootTreeNode;

	/* Root Tree Node*/
	TSharedPtr<FExtender> ActorMenuExtender;

	/** Flag to enable/disable component editing */
	bool bEnableComponentEditing;

	/** Gate to prevent changing the selection while selection change is being broadcast. */
	bool bUpdatingSelection;

	/** true if we've added the separator between the scene and behavior components to the root nodes */
	bool bHasAddedSceneAndBehaviorComponentSeparator;

	/** Controls whether or not to allow calls to UpdateTree() */
	bool bAllowTreeUpdates;

	/** TRUE if this SCSEditor is currently the target of a diff */
	bool bIsDiffing;

	/** The filter box that handles filtering for the tree. */
	TSharedPtr< SSearchBox > FilterBox;
};
