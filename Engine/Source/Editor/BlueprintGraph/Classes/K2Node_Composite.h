// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_Tunnel.h"
#include "K2Node_Composite.generated.h"

class INameValidatorInterface;

UCLASS()
class BLUEPRINTGRAPH_API UK2Node_Composite : public UK2Node_Tunnel
{
	GENERATED_UCLASS_BODY()

	// The graph that this composite node is representing
	UPROPERTY()
	class UEdGraph* BoundGraph;

	//~ Begin UObject Interface
	virtual void PostEditUndo() override;
	//~ End UObject Interface

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual void DestroyNode() override;
	virtual void PostPasteNode() override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanUserDeleteNode() const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual void PostPlacedNewNode() override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual TSharedPtr<class INameValidatorInterface> MakeNameValidator() const override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	virtual bool DrawNodeAsExit() const override { return false; }
	virtual bool DrawNodeAsEntry() const override { return false; }
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	//~ End UK2Node Interface

	// Get the entry/exit nodes inside this collapsed graph
	UK2Node_Tunnel* GetEntryNode() const;
	UK2Node_Tunnel* GetExitNode() const;

protected:
	/** Fixes up the input and output sink when needed, useful after PostEditUndo which changes which graph these nodes point to */
	void FixupInputAndOutputSink();

private:
	/** Rename the BoundGraph to a unique name
		- this is a special case rename: RenameGraphCloseToName() assumes that we are only concurrned with the immediate Outer() scope,
		  but in the case of the BoundGraph we must also look into the Outer of the composite node(a Graph), and make sure no graphs in its SubGraph
		  array are already using the name.  */
	void RenameBoundGraphCloseToName(const FString& Name);

	/** Determine if the name already used by another graph in composite nodes chain */
	bool IsCompositeNameAvailable( const FString& NewName );

	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;
};



