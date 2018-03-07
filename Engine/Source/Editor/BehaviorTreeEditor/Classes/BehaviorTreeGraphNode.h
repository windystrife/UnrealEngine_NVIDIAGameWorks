// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AIGraphNode.h"
#include "BehaviorTreeGraphNode.generated.h"

class UEdGraph;
class UEdGraphSchema;

UCLASS()
class UBehaviorTreeGraphNode : public UAIGraphNode
{
	GENERATED_UCLASS_BODY()

	/** only some of behavior tree nodes support decorators */
	UPROPERTY()
	TArray<UBehaviorTreeGraphNode*> Decorators;

	/** only some of behavior tree nodes support services */
	UPROPERTY()
	TArray<UBehaviorTreeGraphNode*> Services;

	//~ Begin UEdGraphNode Interface
	virtual class UBehaviorTreeGraph* GetBehaviorTreeGraph();
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const override;
	virtual void FindDiffs(class UEdGraphNode* OtherNode, struct FDiffResults& Results) override;
	//~ End UEdGraphNode Interface

#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	virtual FText GetDescription() const override;
	virtual bool HasErrors() const override;
	virtual void InitializeInstance() override;
	virtual void OnSubNodeAdded(UAIGraphNode* SubNode) override;
	virtual void OnSubNodeRemoved(UAIGraphNode* SubNode) override;
	virtual void RemoveAllSubNodes() override;
	virtual int32 FindSubNodeDropIndex(UAIGraphNode* SubNode) const override;
	virtual void InsertSubNodeAt(UAIGraphNode* SubNode, int32 DropIndex) override;

	/** check if node can accept breakpoints */
	virtual bool CanPlaceBreakpoints() const { return false; }

	void ClearDebuggerState();

	/** gets icon resource name for title bar */
	virtual FName GetNameIcon() const;

	/** if set, this node was injected from subtree and shouldn't be edited */
	UPROPERTY()
	uint32 bInjectedNode : 1;

	/** if set, this node is root of tree or sub node of it */
	uint32 bRootLevel : 1;

	/** if set, observer setting is invalid (injected nodes only) */
	uint32 bHasObserverError : 1;

	/** highlighting nodes in abort range for more clarity when setting up decorators */
	uint32 bHighlightInAbortRange0 : 1;

	/** highlighting nodes in abort range for more clarity when setting up decorators */
	uint32 bHighlightInAbortRange1 : 1;

	/** highlighting connections in search range for more clarity when setting up decorators */
	uint32 bHighlightInSearchRange0 : 1;

	/** highlighting connections in search range for more clarity when setting up decorators */
	uint32 bHighlightInSearchRange1 : 1;

	/** highlighting nodes during quick find */
	uint32 bHighlightInSearchTree : 1;

	/** highlight other child node indexes when hovering over a child */
	uint32 bHighlightChildNodeIndices : 1;

	/** debugger flag: breakpoint exists */
	uint32 bHasBreakpoint : 1;

	/** debugger flag: breakpoint is enabled */
	uint32 bIsBreakpointEnabled : 1;

	/** debugger flag: mark node as active (current state) */
	uint32 bDebuggerMarkCurrentlyActive : 1;

	/** debugger flag: mark node as active (browsing previous states) */
	uint32 bDebuggerMarkPreviouslyActive : 1;

	/** debugger flag: briefly flash active node */
	uint32 bDebuggerMarkFlashActive : 1;

	/** debugger flag: mark as succeeded search path */
	uint32 bDebuggerMarkSearchSucceeded : 1;

	/** debugger flag: mark as failed on search path */
	uint32 bDebuggerMarkSearchFailed : 1;

	/** debugger flag: mark as trigger of search path */
	uint32 bDebuggerMarkSearchTrigger : 1;

	/** debugger flag: mark as trigger of discarded search path */
	uint32 bDebuggerMarkSearchFailedTrigger : 1;

	/** debugger flag: mark as going to parent */
	uint32 bDebuggerMarkSearchReverseConnection : 1;

	/** debugger flag: mark stopped on this breakpoint */
	uint32 bDebuggerMarkBreakpointTrigger : 1;

	/** debugger variable: index on search path */
	int32 DebuggerSearchPathIndex;

	/** debugger variable: number of nodes on search path */
	int32 DebuggerSearchPathSize;

	/** debugger variable: incremented on change of debugger flags for render updates */
	int32 DebuggerUpdateCounter;

	/** used to show node's runtime description rather than static one */
	FString DebuggerRuntimeDescription;

protected:

	/** creates add decorator... submenu */
	void CreateAddDecoratorSubMenu(class FMenuBuilder& MenuBuilder, UEdGraph* Graph) const;

	/** creates add service... submenu */
	void CreateAddServiceSubMenu(class FMenuBuilder& MenuBuilder, UEdGraph* Graph) const;

	/** add right click menu to create subnodes: Decorators */
	void AddContextMenuActionsDecorators(const FGraphNodeContextMenuBuilder& Context) const;

	/** add right click menu to create subnodes: Services */
	void AddContextMenuActionsServices(const FGraphNodeContextMenuBuilder& Context) const;
};
