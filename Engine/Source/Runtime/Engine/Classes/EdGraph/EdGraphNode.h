// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "UObject/WeakObjectPtr.h"
#include "EdGraphNode.generated.h"

class INameValidatorInterface;
class SGraphNode;
class SWidget;
class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UEdGraphSchema;
struct FEdGraphPinType;
struct FPropertyChangedEvent;
struct FSlateIcon;
struct FDiffResults;
struct FDiffSingleResult;

/**
  * Struct used to define information for terminal types, e.g. types that can be contained
  * by a container. Currently can represent strong/weak references to a type (only UObjects), 
  * a structure, or a primitive. Support for "Container of Containers" is done by wrapping 
  * a structure, rather than implicitly defining names for containers.
  */
USTRUCT()
struct ENGINE_API FEdGraphTerminalType
{
	GENERATED_USTRUCT_BODY()

	FEdGraphTerminalType()
		: TerminalCategory()
		, TerminalSubCategory()
		, TerminalSubCategoryObject(nullptr)
		, bTerminalIsConst(false)
		, bTerminalIsWeakPointer(false)
	{
	}

	/** Category */
	UPROPERTY()
	FString TerminalCategory;

	/** Sub-category */
	UPROPERTY()
	FString TerminalSubCategory;

	/** Sub-category object */
	UPROPERTY()
	TWeakObjectPtr<UObject> TerminalSubCategoryObject;

	/** Whether or not this pin is a immutable const value */
	UPROPERTY()
	bool bTerminalIsConst;

	/** Whether or not this is a weak reference */
	UPROPERTY()
	bool bTerminalIsWeakPointer;

	/** Creates a TerminalType from the primary portion of the PinType */
	static FEdGraphTerminalType FromPinType(const FEdGraphPinType& PinType);

	friend FArchive& operator<<(FArchive& Ar, FEdGraphTerminalType& P);
};

/** Enum used to define which way data flows into or out of this pin. */
UENUM()
enum EEdGraphPinDirection
{
	EGPD_Input,
	EGPD_Output,
	EGPD_MAX,
};

/** Enum used to define what container type a pin represents. */
UENUM()
enum class EPinContainerType : uint8
{
	None,
	Array,
	Set,
	Map
};

/** Enum to indicate what sort of title we want. */
UENUM()
namespace ENodeTitleType
{
	enum Type
	{
		/** The full title, may be multiple lines. */
		FullTitle,
		/** More concise, single line title. */
		ListView,
		/** Returns the editable title (which might not be a title at all). */
		EditableTitle,
		/** Menu Title for context menus to be displayed in context menus referencing the node. */
		MenuTitle,

		MAX_TitleTypes,
	};
}

/** Enum to indicate if a node has advanced-display-pins, and whether they are shown. */
UENUM()
namespace ENodeAdvancedPins
{
	enum Type
	{
		/** No advanced pins. */
		NoPins,
		/** There are some advanced pins, and they are shown. */
		Shown,
		/** There are some advanced pins, and they are hidden. */
		Hidden
	};
}

/** Enum to indicate a node's enabled state. */
UENUM()
enum class ENodeEnabledState : uint8
{
	/** Node is enabled. */
	Enabled,
	/** Node is disabled. */
	Disabled,
	/** Node is enabled for development only. */
	DevelopmentOnly
};

/** Enum that defines what kind of orphaned pins should be retained. */
enum class ESaveOrphanPinMode : uint8
{
	SaveNone,
	SaveAll,
	SaveAllButExec
};

/** Holds metadata keys, so as to discourage text duplication throughout the engine. */
struct ENGINE_API FNodeMetadata
{
	/** Identifies nodes that are added to populate new graphs by default (helps determine if a graph has any user-placed nodes). */
	static const FName DefaultGraphNode;
private: 
	FNodeMetadata() {}
};

/** This is the context for a GetContextMenuActions call into a specific node. */
struct FGraphNodeContextMenuBuilder
{
	/** The blueprint associated with this context; may be NULL for non-Kismet related graphs. */
	const UBlueprint* Blueprint;
	/** The graph associated with this context. */
	const UEdGraph* Graph;
	/** The node associated with this context. */
	const UEdGraphNode* Node;
	/** The pin associated with this context; may be NULL when over a node. */
	const UEdGraphPin* Pin;
	/** The menu builder to append actions to. */
	class FMenuBuilder* MenuBuilder;
	/** Whether the graph editor is currently part of a debugging session (any non-debugging commands should be disabled). */
	bool bIsDebugging;

	FGraphNodeContextMenuBuilder(const UEdGraph* InGraph, const UEdGraphNode* InNode, const UEdGraphPin* InPin, class FMenuBuilder* InMenuBuilder, bool bInDebuggingMode);
};

UCLASS()
class ENGINE_API UEdGraphNode : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	TArray<UEdGraphPin*> Pins;

	/** List of connector pins */
	UPROPERTY()
	TArray<class UEdGraphPin_Deprecated*> DeprecatedPins;

	/** X position of node in the editor */
	UPROPERTY()
	int32 NodePosX;

	/** Y position of node in the editor */
	UPROPERTY()
	int32 NodePosY;

	/** Width of node in the editor; only used when the node can be resized */
	UPROPERTY()
	int32 NodeWidth;

	/** Height of node in the editor; only used when the node can be resized */
	UPROPERTY()
	int32 NodeHeight;

	/** Enum to indicate if a node has advanced-display-pins, and if they are shown */
	UPROPERTY()
	TEnumAsByte<ENodeAdvancedPins::Type> AdvancedPinDisplay;

private:
	/** Indicates in what state the node is enabled, which may eliminate it from being compiled */
	UPROPERTY()
	ENodeEnabledState EnabledState;

public:
	/** When reconstructing a node should the orphaned pins be retained and transfered to the new pin list. */
	ESaveOrphanPinMode OrphanedPinSaveMode;

private:
	/** Indicates whether or not the user explicitly set the enabled state */
	UPROPERTY()
	uint8 bUserSetEnabledState:1;

protected:
	/** (DEPRECATED) Value used for AllowSplitPins(). Do not override. */
	uint8 bAllowSplitPins_DEPRECATED:1;

private:
	/** (DEPRECATED) FALSE if the node is a disabled, which eliminates it from being compiled */
	UPROPERTY()
	uint8 bIsNodeEnabled_DEPRECATED:1;

public:

#if WITH_EDITORONLY_DATA
	/** If true, this node can be resized and should be drawn with a resize handle */
	UPROPERTY()
	uint8 bCanResizeNode:1;

#endif // WITH_EDITORONLY_DATA

private:
	/** Whether the node was created as part of an expansion step */
	uint8 bIsIntermediateNode : 1;

public:
	/** Flag to check for compile error/warning */
	UPROPERTY()
	uint8 bHasCompilerMessage:1;

	/** Comment bubble pinned state */
	UPROPERTY()
	uint8 bCommentBubblePinned:1;

	/** Comment bubble visibility */
	UPROPERTY()
	uint8 bCommentBubbleVisible:1;

	/** Make comment bubble visible */
	UPROPERTY(Transient)
	uint8 bCommentBubbleMakeVisible:1;

#if WITH_EDITORONLY_DATA
	/** If true, this node can be renamed in the editor */
	UPROPERTY()
	uint8 bCanRenameNode:1;

	/** Note for a node that lingers until saved */
	UPROPERTY(Transient)
	FText NodeUpgradeMessage;
#endif // WITH_EDITORONLY_DATA

	/** Comment string that is drawn on the node */
	UPROPERTY()
	FString NodeComment;

	/** Flag to store node specific compile error/warning*/
	UPROPERTY()
	int32 ErrorType;
	
	/** Error/Warning description */
	UPROPERTY()
	FString ErrorMsg;
	
	/** GUID to uniquely identify this node, to facilitate diffing versions of this graph */
	UPROPERTY()
	FGuid NodeGuid;

public:
	/** Determines whether or not the node is enabled. */
	bool IsNodeEnabled() const
	{
		return (EnabledState == ENodeEnabledState::Enabled) || ((EnabledState == ENodeEnabledState::DevelopmentOnly) && IsInDevelopmentMode());
	}

	/** Returns the specific sort of enable state this node wants */
	ENodeEnabledState GetDesiredEnabledState() const
	{
		return EnabledState;
	}

	/** Set the enabled state of the node to a new value */
	void SetEnabledState(ENodeEnabledState NewState, bool bUserAction = true)
	{
		EnabledState = NewState;
		bUserSetEnabledState = bUserAction;
	}

	/** Has the user set the enabled state or is it still using the automatic settings? */
	bool HasUserSetTheEnabledState() const
	{
		return bUserSetEnabledState;
	}

	/** Determines whether or not the node will compile in development mode. */
	virtual bool IsInDevelopmentMode() const;

	/** Returns true if this is a disabled automatically placed ghost node (see the DefaultEventNodes ini section) */
	bool IsAutomaticallyPlacedGhostNode() const;

	/** Marks this node as an automatically placed ghost node (see the DefaultEventNodes ini section) */
	void MakeAutomaticallyPlacedGhostNode();

#if WITH_EDITOR

private:
	static TArray<UEdGraphPin*> PooledPins;

public:
	// UObject interface
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void Serialize(FArchive& Ar) override;
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;
	virtual void BeginDestroy() override;
	// End of UObject interface

	/** widget representing this node if it exists; Note: This is not safe to use in general and will be removed in the future, as there is no guarantee that only one graph editor/panel is viewing a given graph */
	TWeakPtr<SGraphNode> DEPRECATED_NodeWidget;

	/** Get all pins this node owns */
	TArray<UEdGraphPin*> GetAllPins() { return Pins; }

	/** Create a new pin on this node using the supplied info, and return the new pin */
	UEdGraphPin* CreatePin(
		EEdGraphPinDirection Dir, 
		const FString& PinCategory, 
		const FString& PinSubCategory, 
		UObject* PinSubCategoryObject, 
		bool bIsArray, 
		bool bIsReference, 
		const FString& PinName, 
		bool bIsConst = false, 
		int32 Index = INDEX_NONE, 
		bool bIsSet = false, 
		bool bIsMap = false,
		const FEdGraphTerminalType& ValueTerminalType = FEdGraphTerminalType());

	/** Create a new pin on this node using the supplied info, and return the new pin */
	UEdGraphPin* CreatePin(
		EEdGraphPinDirection Dir, 
		const FString& PinCategory, 
		const FString& PinSubCategory, 
		UObject* PinSubCategoryObject, 
		const FString& PinName, 
		EPinContainerType PinContainerType = EPinContainerType::None,
		bool bIsReference = false, 
		bool bIsConst = false, 
		int32 Index = INDEX_NONE, 
		const FEdGraphTerminalType& ValueTerminalType = FEdGraphTerminalType());

	/** Create a new pin on this node using the supplied pin type, and return the new pin */
	UEdGraphPin* CreatePin(EEdGraphPinDirection Dir, const FEdGraphPinType& InPinType, const FString& PinName, int32 Index = INDEX_NONE);

	/** Destroys the specified pin, does not modify its owning pin's Pins list */
	static void DestroyPin(UEdGraphPin* Pin);

	/** Find a pin on this node with the supplied name and optional direction */
	UEdGraphPin* FindPin(const FString& PinName, const EEdGraphPinDirection Direction = EGPD_MAX) const;

	/** Find a pin on this node with the supplied name and optional direction and assert if it is not present */
	UEdGraphPin* FindPinChecked(const FString& PinName, const EEdGraphPinDirection Direction = EGPD_MAX) const;
	
	/** Find the pin on this node with the supplied guid */
	UEdGraphPin* FindPinById(const FGuid PinId) const;

	/** Find the pin on this node with the supplied guid and assert if it is not present */
	UEdGraphPin* FindPinByIdChecked(const FGuid PinId) const;

	/** Find a pin on this node with the supplied name and remove it, returns TRUE if successful */
	bool RemovePin(UEdGraphPin* Pin);

	/** Returns whether the node was created by UEdGraph::CreateIntermediateNode. */
	bool IsIntermediateNode() const { return bIsIntermediateNode; }

	/** Whether or not this node should be given the chance to override pin names.  If this returns true, then GetPinNameOverride() will be called for each pin, each frame */
	virtual bool ShouldOverridePinNames() const { return false; }

	/** Whether or not struct pins belonging to this node should be allowed to be split or not. */
	DEPRECATED(4.14, "Please call CanSplitPin and provide the specific Pin to split.")
	virtual bool AllowSplitPins() const { return bAllowSplitPins_DEPRECATED; }

	/** Whether or not struct pins belonging to this node should be allowed to be split or not. */
	virtual bool CanSplitPin(const UEdGraphPin* Pin) const 
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return AllowSplitPins();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/** Gets the overridden name for the specified pin, if any */
	virtual FText GetPinNameOverride(const UEdGraphPin& Pin) const { return FText::GetEmpty(); }

	/** Gets the display name for a pin */
	virtual FText GetPinDisplayName(const UEdGraphPin* Pin) const;

	/**
	 * Fetch the hover text for a pin when the graph is being edited.
	 * 
	 * @param   Pin				The pin to fetch hover text for (should belong to this node)
	 * @param   HoverTextOut	This will get filled out with the requested text
	 */
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const;

	/** Gets the index for a pin */
	int32 GetPinIndex(UEdGraphPin* Pin) const;

	/** Gets the pin at a given index 
	* @param Index The zero-based index of the pin to access.
	* @return The pin found at this location or nullptr if invalid index.
	*/
	UEdGraphPin* GetPinAt(int32 Index) const;

	/** Break all links on this node */
	void BreakAllNodeLinks();

	/** Snap this node to a specified grid size */
	void SnapToGrid(float GridSnapSize);

	/** Clear error flag */
	void ClearCompilerMessage()
	{
		bHasCompilerMessage = false;
	}

	/** Generate a unique pin name, trying to stick close to a passed in name */
	virtual FString CreateUniquePinName(FString SourcePinName) const
	{
		FString PinName(SourcePinName);
		
		int32 Index = 1;
		while (FindPin(PinName) != NULL)
		{
			++Index;
			PinName = SourcePinName + FString::FromInt(Index);
		}

		return PinName;
	}

	/** Returns the graph that contains this node */
	class UEdGraph* GetGraph() const;

	/**
	 * Allocate default pins for a given node, based only the NodeType, which should already be filled in.
	 *
	 * @return	true if the pin creation succeeds, false if there was a problem (such a failure to find a function when the node is a function call).
	 */
	virtual void AllocateDefaultPins() {}

	/** Destroy the specified node */
	virtual void DestroyNode();

	/**
	 * Refresh the connectors on a node, preserving as many connections as it can.
	 */
	virtual void ReconstructNode() {}

	/**
	 * Removes the specified pin from the node, preserving remaining pin ordering.
	 */
	virtual void RemovePinAt(const int32 PinIndex, const EEdGraphPinDirection PinDirection);

	/**
	 * Perform any steps necessary prior to copying a node into the paste buffer
	 */
	virtual void PrepareForCopying() {}

	/**
	 * Determine if this node can live in the specified graph
	 */
	virtual bool CanPasteHere(const UEdGraph* TargetGraph) const { return IsCompatibleWithGraph(TargetGraph); }

	/**
	 * Determine if this node can be created under the specified schema
     */
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const { return true; }
	
	/**
	 * Determine if a node of this type can be created for the specified graph.
     */
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const;

	/**
	 * Perform any fixups (deep copies of associated data, etc...) necessary after a node has been pasted in the editor
	 */
	virtual void PostPasteNode() {}

	/** Gets the name of this node, shown in title bar */
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const;

	/** Gets the search string to find references to this node */
	virtual FString GetFindReferenceSearchString() const;

	/** 
	 * Gets the draw color of a node's title bar
	 */
	virtual FLinearColor GetNodeTitleColor() const;

	/**
	 * Get the draw color for a node's comment popup
	 */
	virtual FLinearColor GetNodeCommentColor() const;

	/**
	 * Gets the tooltip to display when over the node
	 */
	virtual FText GetTooltipText() const;

	/**
	 * Returns the keywords that should be used when searching for this node
	 *
	 * @TODO: Should search keywords be localized? Probably.
	 */
	virtual FText GetKeywords() const;
	 
	/**
	 * Returns the link used for external documentation for the graph node
	 */
	virtual FString GetDocumentationLink() const { return FString(); }

	/**
	 * Returns the name of the excerpt to display from the specified external documentation link for the graph node
	 * Default behavior is to return the class name (including prefix)
	 */
	virtual FString GetDocumentationExcerptName() const;

	/** @return Icon to use in menu or on node */
	DEPRECATED(4.13, "Please override 'virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const;' instead.")
	virtual FName GetPaletteIcon(FLinearColor& OutColor) const { return NAME_None; }

	/** @return Icon to use in menu or on node */
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const;

	/** Should we show the Palette Icon for this node on the node title */
	virtual bool ShowPaletteIconOnNode() const { return false; }

	/**
	 * Autowire a newly created node.
	 *
	 * @param	FromPin	The source pin that caused the new node to be created (typically a drag-release context menu creation).
	 */
	virtual void AutowireNewNode(UEdGraphPin* FromPin) {}

	// A chance to initialize a new node; called just once when a new node is created, before AutowireNewNode or AllocateDefaultPins is called.
	// This method is not called when a node is reconstructed, etc...
	virtual void PostPlacedNewNode() {}

	/** Called when the DefaultValue of one of the pins of this node is changed in the editor */
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) {}

	/** Called when the connection list of one of the pins of this node is changed in the editor */
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) {}

	/** Called when one of the pins of this node has had its' pin type changed from an external source (like the SPinTypeSelector in the case of kismet) */
	virtual void PinTypeChanged(UEdGraphPin* Pin) {}

	/**
	 * Called when something external to this node has changed the connection list of any of the pins in the node
	 *   - Different from PinConnectionListChanged as this is called outside of any loops iterating over our pins allowing
	 *     us to do things like reconstruct the node safely without trashing pins we are already iterating on
	 *   - Typically called after a user induced action like making a pin connection or a pin break
	 */
	virtual void NodeConnectionListChanged() {}

	/** Shorthand way to access the schema of the graph that owns this node */
	const UEdGraphSchema* GetSchema() const;

	/** Whether or not this node can be safely duplicated (via copy/paste, etc...) in the graph */
	virtual bool CanDuplicateNode() const;

	/** Whether or not this node can be deleted by user action */
	virtual bool CanUserDeleteNode() const;

	/** Whether or not this node allows users to edit the advanced view flag of pins (actually edit the property, not the same as show/hide advanced pins). */
	virtual bool CanUserEditPinAdvancedViewFlag() const { return false; }

	/** Tries to come up with a descriptive name for the compiled output */
	virtual FString GetDescriptiveCompiledName() const;

	/** Update node size to new value */
	virtual void ResizeNode(const FVector2D& NewSize) {}

	// Returns true if this node is deprecated
	virtual bool IsDeprecated() const;

	// Returns true if this node should produce a compiler warning on deprecation
	virtual bool ShouldWarnOnDeprecation() const { return true; }

	// Returns the string to use when reporting the deprecation
	virtual FString GetDeprecationMessage() const;

	// Returns the object that should be focused when double-clicking on this node
	// (the object can be an actor, which selects it in the world, or a node/graph/pin)
	virtual UObject* GetJumpTargetForDoubleClick() const;

	// Returns true if it is possible to jump to the definition of this node (e.g., if it's a variable get or a function call)
	virtual bool CanJumpToDefinition() const;

	// Jump to the definition of this node (should only be called if CanJumpToDefinition() return true)
	virtual void JumpToDefinition() const;

	/** Create a new unique Guid for this node */
	void CreateNewGuid();

	/** Gets a list of actions that can be done to this particular node */
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const {}

	// Gives each visual node a chance to do final validation before it's node is harvested for use at runtime
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const {}

	/** Gives the node the option to customize how diffs are discovered within it.  */
	virtual void FindDiffs(class UEdGraphNode* OtherNode, FDiffResults& Results);

	// This function gets menu items that can be created using this node given the specified context
	virtual void GetMenuEntries(struct FGraphContextMenuBuilder& ContextMenuBuilder) const {}

	// create a name validator for this node
	virtual TSharedPtr<class INameValidatorInterface> MakeNameValidator() const { return NULL; }

	// called when this node is being renamed after a successful name validation
	virtual void OnRenameNode(const FString& NewName) {}

	// called to replace this nodes comment text
	virtual void OnUpdateCommentText( const FString& NewComment );

	// called when the node's comment bubble is toggled
	virtual void OnCommentBubbleToggled( bool bInCommentBubbleVisible ) {}

	// called when a pin is removed
	virtual void OnPinRemoved( UEdGraphPin* InRemovedPin ) {}

	/** 
	* Returns whether to draw this node as a control point only (knot/reroute node). Note that this means that the node should only have on input and output pin.
	* @param OutInputPinIndex The index in the pins array associated with the control point input pin.
	* @param OutOutputPinIndex The index in the pins array associated with the control point output pin.
	* @return Whether or not to draw this node as a control point.
	*/
	virtual bool ShouldDrawNodeAsControlPointOnly(int32& OutInputPinIndex, int32& OutOutputPinIndex) const;

	/**
	 * Add's node data to the search metadata, override to collect more data that may be desirable to search for
	 *
	 * @param OutTaggedMetaData		Built array of tagged meta data for the node
	 */
	virtual void AddSearchMetaDataInfo(TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const;

	/** Return the requested metadata for the pin if there is any */
	virtual FString GetPinMetaData(FString InPinName, FName InKey) { return FString(); }

	/** Return false if the node and any expansion will isolate itself during compile */
	virtual bool IsCompilerRelevant() const { return true; }

	/** Return the matching "pass-through" pin for the given pin (if supported by this node) */
	virtual UEdGraphPin* GetPassThroughPin(const UEdGraphPin* FromPin) const { return nullptr; }

	/** If the node has a subgraph, should they be merged into the main graph? */
	virtual bool ShouldMergeChildGraphs() const { return true; }

	/** Create a visual widget to represent this node in a graph editor or graph panel.  If not implemented, the default node factory will be used. */
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() { return TSharedPtr<SGraphNode>(); }

	/** Create the background image for the widget representing this node */
	virtual TSharedPtr<SWidget> CreateNodeImage() const { return TSharedPtr<SWidget>(); }

	/** Adds an upgrade note to this node */
	void AddNodeUpgradeNote(FText InUpgradeNote);

	/** If the comment bubble needs to be made visible immediately */
	bool ShouldMakeCommentBubbleVisible() const;

	/** Sets a flag if the comment bubble needs to be made visible immediately */
	void SetMakeCommentBubbleVisible(bool MakeVisible);

protected:
	/**
	 * Finds the difference in properties of node instance
	 *
	 * @param StructA The struct of the class we are looking at LHS
	 * @param StructB The struct of the class we are looking at RHS
	 * @param DataA The raw data for the UObject we are comparing LHS
	 * @param DataB The raw data for the UObject we are comparing RHS
	 * @param Results The Results where differences are stored
	 * @param Diff The single result with default parameters setup
	 */
	virtual void DiffProperties(UClass* StructA, UClass* StructB, UObject* DataA, UObject* DataB, FDiffResults& Results, FDiffSingleResult& Diff) const;

	// Returns a human-friendly description of the property in the form "PropertyName: Value"
	virtual FString GetPropertyNameAndValueForDiff(const UProperty* Prop, const uint8* PropertyAddr) const;

#endif // WITH_EDITOR

	friend struct FSetAsIntermediateNode;
};

struct FSetAsIntermediateNode
{
	friend UEdGraph;

private:
	FSetAsIntermediateNode(UEdGraphNode* GraphNode)
	{
		GraphNode->bIsIntermediateNode = true;
	}
};


