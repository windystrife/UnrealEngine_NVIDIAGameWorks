// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Engine/Blueprint.h"
#include "BlueprintEditor.h"
#include "GraphEditorDragDropAction.h"
#include "MyBlueprintItemDragDropAction.h"

class UEdGraph;

/** DragDropAction class for dropping a Variable onto a graph */
class KISMET_API FKismetVariableDragDropAction : public FMyBlueprintItemDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FKismetVariableDragDropAction, FMyBlueprintItemDragDropAction)

	// FGraphEditorDragDropAction interface
	virtual void HoverTargetChanged() override;
	virtual FReply DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	virtual FReply DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	virtual FReply DroppedOnPanel(const TSharedRef< class SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	// End of FGraphEditorDragDropAction

	static TSharedRef<FKismetVariableDragDropAction> New(TSharedPtr<FEdGraphSchemaAction> InAction, FName InVariableName, UStruct* InVariableSource, FNodeCreationAnalytic AnalyticCallback)
	{
		TSharedRef<FKismetVariableDragDropAction> Operation = MakeShareable(new FKismetVariableDragDropAction);
		Operation->VariableName = InVariableName;
		Operation->VariableSource = InVariableSource;
		Operation->AnalyticCallback = AnalyticCallback;
		Operation->SourceAction = InAction;
		Operation->Construct();
		return Operation;
	}

	UProperty* GetVariableProperty()
	{
		if (VariableSource.IsValid() && VariableName != NAME_None)
		{
			return FindFieldChecked<UProperty>(VariableSource.Get(), VariableName);
		}
		return nullptr;
	}

protected:
	 /** Construct a FKismetVariableDragDropAction */
	FKismetVariableDragDropAction();

	/** Structure for required node construction parameters */
	struct FNodeConstructionParams
	{
		FVector2D GraphPosition;
		UEdGraph* Graph;
		FName VariableName;
		TWeakObjectPtr<UStruct> VariableSource;
	};

	// FGraphSchemaActionDragDropAction interface
	virtual void GetDefaultStatusSymbol(const FSlateBrush*& PrimaryBrushOut, FSlateColor& IconColorOut, FSlateBrush const*& SecondaryBrushOut, FSlateColor& SecondaryColorOut) const override;
	// End of FGraphSchemaActionDragDropAction interface

	// FMyBlueprintItemDragDropAction interface
	virtual UBlueprint* GetSourceBlueprint() const override;
	// End of FMyBlueprintItemDragDropAction interface

	/** Called when user selects to create a Getter for the variable */
	static void MakeGetter(FNodeConstructionParams InParams);
	/** Called when user selects to create a Setter for the variable */
	static void MakeSetter(FNodeConstructionParams InParams);
	/** Called too check if we can execute a setter on a given property */
	static bool CanExecuteMakeSetter(FNodeConstructionParams InParams, UProperty* InVariableProperty);

	/**
	 * Test new variable type against existing links for node and get any links that will break
	 *
	 * @param	Node						The node with existing links
	 * @param	NewVariableProperty			the property for the new variable type 
	 * @param	OutBroken						All of the links which are NOT compatible with the new type
	 */
	void GetLinksThatWillBreak(UEdGraphNode* Node, UProperty* NewVariableProperty, TArray<class UEdGraphPin*>& OutBroken);

	/** Indicates if replacing the variable node, with the new property will require any links to be broken*/
	bool WillBreakLinks( UEdGraphNode* Node, UProperty* NewVariableProperty ) 
	{
		TArray<class UEdGraphPin*> BadLinks;
		GetLinksThatWillBreak(Node,NewVariableProperty,BadLinks);
		return BadLinks.Num() > 0;
	}

	/**
	 * Checks if the property can be dropped in a graph
	 *
	 * @param InVariableProperty		The variable property to check with
	 * @param InGraph					The graph to check against placing the variable
	 */
	bool CanVariableBeDropped(const UProperty* InVariableProperty, const UEdGraph& InGraph) const;


	/** Returns the local variable's scope, if any */
	UStruct* GetLocalVariableScope() const;

protected:
	/** Name of variable being dragged */
	FName VariableName;
	/** Scope this variable belongs to */
	TWeakObjectPtr<UStruct> VariableSource;
};
