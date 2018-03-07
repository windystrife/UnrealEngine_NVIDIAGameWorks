// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Engine/MemberReference.h"
#include "BlueprintEditor.h"
#include "Editor/GraphEditor/Public/GraphEditorDragDropAction.h"
#include "MyBlueprintItemDragDropAction.h"

class UEdGraph;

/*******************************************************************************
* FKismetDragDropAction
*******************************************************************************/

class KISMET_API FKismetDragDropAction : public FMyBlueprintItemDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FKismetDragDropAction, FMyBlueprintItemDragDropAction)
		
	// FGraphEditorDragDropAction interface
	virtual void HoverTargetChanged() override;
	virtual FReply DroppedOnPanel( const TSharedRef< class SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	// End of FGraphSchemaActionDragDropAction

	DECLARE_DELEGATE_RetVal_ThreeParams(bool, FCanBeDroppedDelegate, TSharedPtr<FEdGraphSchemaAction> /*DropAction*/, UEdGraph* /*HoveredGraphIn*/, FText& /*ImpededReasonOut*/);

	static TSharedRef<FKismetDragDropAction> New(TSharedPtr<FEdGraphSchemaAction> InActionNode, FNodeCreationAnalytic AnalyticCallback, FCanBeDroppedDelegate CanBeDroppedDelegate)
	{
		TSharedRef<FKismetDragDropAction> Operation = MakeShareable(new FKismetDragDropAction);
		Operation->SourceAction = InActionNode;
		Operation->AnalyticCallback = AnalyticCallback;
		Operation->CanBeDroppedDelegate = CanBeDroppedDelegate;
		Operation->Construct();
		return Operation;
	}

protected:
	bool ActionWillShowExistingNode() const;

	/** */
	FCanBeDroppedDelegate CanBeDroppedDelegate;
};

/*******************************************************************************
* FKismetFunctionDragDropAction
*******************************************************************************/

class KISMET_API FKismetFunctionDragDropAction : public FKismetDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FKismetFunctionDragDropAction, FKismetDragDropAction)

	FKismetFunctionDragDropAction();

	// FGraphEditorDragDropAction interface
	virtual FReply DroppedOnPanel( const TSharedRef< class SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	virtual FReply DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	// End of FGraphEditorDragDropAction

	static TSharedRef<FKismetFunctionDragDropAction> New(TSharedPtr<FEdGraphSchemaAction> InActionNode, FName InFunctionName, UClass* InOwningClass, const FMemberReference& InCallOnMember, FNodeCreationAnalytic AnalyticCallback, FCanBeDroppedDelegate CanBeDroppedDelegate = FCanBeDroppedDelegate());

protected:
	/** Name of function being dragged */
	FName FunctionName;
	/** Class that function belongs to */
	UClass* OwningClass;
	/** Call on member reference */
	FMemberReference CallOnMember;

	/** Looks up the functions field on OwningClass using FunctionName */
	UFunction const* GetFunctionProperty() const;

	/** Constructs an action to execute, placing a function call node for the associated function */
	class UBlueprintFunctionNodeSpawner* GetDropAction(UEdGraph& Graph) const;
};

/*******************************************************************************
* FKismetMacroDragDropAction
*******************************************************************************/

class KISMET_API FKismetMacroDragDropAction : public FKismetDragDropAction
{
public:
	DRAG_DROP_OPERATOR_TYPE(FKismetMacroDragDropAction, FKismetDragDropAction)

	FKismetMacroDragDropAction();

	// FGraphEditorDragDropAction interface
	virtual FReply DroppedOnPanel( const TSharedRef< class SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	// End of FGraphEditorDragDropAction

	static TSharedRef<FKismetMacroDragDropAction> New(TSharedPtr<FEdGraphSchemaAction> InActionNode, FName InMacroName, UBlueprint* InBlueprint, UEdGraph* InMacro, FNodeCreationAnalytic AnalyticCallback);

protected:
	// FMyBlueprintItemDragDropAction interface
	virtual UBlueprint* GetSourceBlueprint() const override
	{
		return Blueprint;
	}
	// End of FMyBlueprintItemDragDropAction interface

protected:
	/** Name of macro being dragged */
	FName MacroName;
	/** Graph for the macro being dragged */
	UEdGraph* Macro;
	/** Blueprint we are operating on */
	UBlueprint* Blueprint;
};

