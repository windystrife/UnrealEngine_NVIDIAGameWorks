// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BPFunctionDragDropAction.h"
#include "EdGraph/EdGraph.h"
#include "EditorStyleSet.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintNodeBinder.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FunctionDragDropAction"

/*******************************************************************************
* Static File Helpers
*******************************************************************************/

/**
 * A default for function node drag-drop operations to use if one wasn't 
 * supplied. Checks to see if a function node is allowed to be placed where it 
 * is currently dragged.
 * 
 * @param  DropActionIn		The action that will be executed when the user drops the dragged item.
 * @param  HoveredGraphIn	A pointer to the graph that the user currently has the item dragged over.
 * @param  ImpededReasonOut	If this returns false, this will be filled out with a reason to present the user with.
 * @param  FunctionIn		The function that the associated node would be calling.
 * @return True is the dragged palette item can be dropped where it currently is, false if not.
 */
static bool CanFunctionBeDropped(TSharedPtr<FEdGraphSchemaAction> /*DropActionIn*/, UEdGraph* HoveredGraphIn, FText& ImpededReasonOut, UFunction const* FunctionIn)
{
	bool bCanBePlaced = true;
	if (HoveredGraphIn == NULL)
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("DropOnlyInGraph", "Nodes can only be placed inside the blueprint graph");
	}
	else if (!HoveredGraphIn->GetSchema()->IsA(UEdGraphSchema_K2::StaticClass()))
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("CannotCreateInThisSchema", "Cannot call functions in this type of graph");
	}
	else if (FunctionIn == NULL)
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("InvalidFuncAction", "Invalid function for placement");
	}
	else if ((HoveredGraphIn->GetSchema()->GetGraphType(HoveredGraphIn) == EGraphType::GT_Function) && FunctionIn->HasMetaData(FBlueprintMetadata::MD_Latent))
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("CannotCreateLatentInGraph", "Cannot call latent functions in function graphs");
	}

	return bCanBePlaced;
}

/**
 * Checks to see if a macro node is allowed to be placed where it is currently 
 * dragged.
 * 
 * @param  DropActionIn		The action that will be executed when the user drops the dragged item.
 * @param  HoveredGraphIn	A pointer to the graph that the user currently has the item dragged over.
 * @param  ImpededReasonOut	If this returns false, this will be filled out with a reason to present the user with.
 * @param  MacroGraph		The macro graph that the node would be executing.
 * @param  bInIsLatentMacro	True if the macro has latent functions in it
 * @return True is the dragged palette item can be dropped where it currently is, false if not.
 */
static bool CanMacroBeDropped(TSharedPtr<FEdGraphSchemaAction> /*DropActionIn*/, UEdGraph* HoveredGraphIn, FText& ImpededReasonOut, UEdGraph* MacroGraph, bool bIsLatentMacro)
{
	bool bCanBePlaced = true;
	if (HoveredGraphIn == NULL)
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("DropOnlyInGraph", "Nodes can only be placed inside the blueprint graph");
	}
	else if (!HoveredGraphIn->GetSchema()->IsA(UEdGraphSchema_K2::StaticClass()))
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("CannotCreateInThisSchema_Macro", "Cannot call macros in this type of graph");
	}
	else if (MacroGraph == HoveredGraphIn)
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("CannotRecurseMacro", "Cannot place a macro instance in its own graph");
	}
	else if( bIsLatentMacro && HoveredGraphIn->GetSchema()->GetGraphType(HoveredGraphIn) == GT_Function)
	{
		bCanBePlaced = false;
		ImpededReasonOut = LOCTEXT("CannotPlaceLatentMacros", "Cannot place a macro instance with latent functions in function graphs!");
	}

	return bCanBePlaced;
}

/*******************************************************************************
* FKismetDragDropAction
*******************************************************************************/

//------------------------------------------------------------------------------
void FKismetDragDropAction::HoverTargetChanged()
{
	UEdGraph* TheHoveredGraph = GetHoveredGraph();

	FText CannotDropReason = FText::GetEmpty();
	if (ActionWillShowExistingNode())
	{
		FSlateBrush const* ShowsExistingIcon = FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.ShowNode"));
		FText DragingText = FText::Format(LOCTEXT("ShowExistingNode", "Show '{0}'"), SourceAction->GetMenuDescription());
		SetSimpleFeedbackMessage(ShowsExistingIcon, FLinearColor::White, DragingText);
	}
	// it should be obvious that we can't drop on anything but a graph, so no need to point that out
	else if ((TheHoveredGraph == nullptr) || !CanBeDroppedDelegate.IsBound() || CanBeDroppedDelegate.Execute(SourceAction, TheHoveredGraph, CannotDropReason))
	{
		FMyBlueprintItemDragDropAction::HoverTargetChanged();
	}
	else 
	{
		SetFeedbackMessageError(CannotDropReason);
	}
}

//------------------------------------------------------------------------------
FReply FKismetDragDropAction::DroppedOnPanel( const TSharedRef< SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph)
{	
	FReply Reply = FReply::Unhandled();

	FText CannotDropReason = FText::GetEmpty();
	if (!CanBeDroppedDelegate.IsBound() || CanBeDroppedDelegate.Execute(SourceAction, GetHoveredGraph(), CannotDropReason))
	{
		Reply = FMyBlueprintItemDragDropAction::DroppedOnPanel(Panel, ScreenPosition, GraphPosition, Graph);
	}

	if (Reply.IsEventHandled())
	{
		AnalyticCallback.ExecuteIfBound();
	}

	return Reply;
}

//------------------------------------------------------------------------------
bool FKismetDragDropAction::ActionWillShowExistingNode() const
{
	bool bWillFocusOnExistingNode = false;

	UEdGraph* TheHoveredGraph = GetHoveredGraph();
	if (SourceAction.IsValid() && (TheHoveredGraph != nullptr))
	{
		bWillFocusOnExistingNode = (SourceAction->GetTypeId() == FEdGraphSchemaAction_K2TargetNode::StaticGetTypeId()) ||
			(SourceAction->GetTypeId() == FEdGraphSchemaAction_K2InputAction::StaticGetTypeId());

		if (!bWillFocusOnExistingNode)
		{
			if (SourceAction->GetTypeId() == FEdGraphSchemaAction_K2AddEvent::StaticGetTypeId())
			{
				FEdGraphSchemaAction_K2AddEvent* AddEventAction = (FEdGraphSchemaAction_K2AddEvent*)SourceAction.Get();
				bWillFocusOnExistingNode = AddEventAction->EventHasAlreadyBeenPlaced(FBlueprintEditorUtils::FindBlueprintForGraph(TheHoveredGraph));
			}
			else if (SourceAction->GetTypeId() == FEdGraphSchemaAction_K2Event::StaticGetTypeId())
			{
				FEdGraphSchemaAction_K2Event* FuncAction = (FEdGraphSchemaAction_K2Event*)SourceAction.Get();
				UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(FuncAction->NodeTemplate);
				// Drag and dropping custom event's will place a Call Function and will not focus the existing event
				if (CustomEvent == nullptr)
				{
					bWillFocusOnExistingNode = true;
				}
			}
		}
	}

	return bWillFocusOnExistingNode;
}

/*******************************************************************************
* FKismetFunctionDragDropAction
*******************************************************************************/

//------------------------------------------------------------------------------
TSharedRef<FKismetFunctionDragDropAction> FKismetFunctionDragDropAction::New(
	TSharedPtr<FEdGraphSchemaAction>	InActionNode,
	FName								InFunctionName, 
	UClass*								InOwningClass, 
	FMemberReference const&				InCallOnMember, 
	FNodeCreationAnalytic				AnalyticCallback, 
	FCanBeDroppedDelegate				CanBeDroppedDelegate /* = FCanBeDroppedDelegate() */)
{
	TSharedRef<FKismetFunctionDragDropAction> Operation = MakeShareable(new FKismetFunctionDragDropAction);
	Operation->FunctionName     = InFunctionName;
	Operation->OwningClass      = InOwningClass;
	Operation->CallOnMember     = InCallOnMember;
	Operation->AnalyticCallback = AnalyticCallback;
	Operation->CanBeDroppedDelegate = CanBeDroppedDelegate;
	Operation->SourceAction = InActionNode;

	if (!CanBeDroppedDelegate.IsBound())
	{
		Operation->CanBeDroppedDelegate = FCanBeDroppedDelegate::CreateStatic(&CanFunctionBeDropped, Operation->GetFunctionProperty());
	}

	Operation->Construct();
	return Operation;
}

//------------------------------------------------------------------------------
FKismetFunctionDragDropAction::FKismetFunctionDragDropAction()
	: OwningClass(NULL)
{

}

//------------------------------------------------------------------------------
FReply FKismetFunctionDragDropAction::DroppedOnPanel(TSharedRef<SWidget> const& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph)
{
	return DroppedOnPin(ScreenPosition, GraphPosition);
}

//------------------------------------------------------------------------------
FReply FKismetFunctionDragDropAction::DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition)
{
	FReply Reply = FReply::Unhandled();

	UEdGraph* Graph = GetHoveredGraph();
	check(Graph); 

	// The ActionNode set during construction points to the Graph, this is suitable for displaying the mouse decorator but needs to be more complete based on the current graph
	UBlueprintFunctionNodeSpawner* FunctionNodeSpawner = GetDropAction(*Graph);

	if (FunctionNodeSpawner)
	{
		FText CannotDropReason = FText::GetEmpty();
		if (!CanBeDroppedDelegate.IsBound() || CanBeDroppedDelegate.Execute(nullptr, Graph, CannotDropReason))
		{
			UFunction const* Function = GetFunctionProperty();
			if ((Function != NULL) && UEdGraphSchema_K2::CanUserKismetCallFunction(Function))
			{
				AnalyticCallback.ExecuteIfBound();

				const FScopedTransaction Transaction(LOCTEXT("KismetFunction_DroppedOnPanel", "Function Dropped on Graph"));

				IBlueprintNodeBinder::FBindingSet Bindings;
				UEdGraphNode* ResultNode = FunctionNodeSpawner->Invoke(Graph, Bindings, GraphPosition);

				// Autowire the node if we were dragging on top of a pin
				if (ResultNode != nullptr)
				{
					if (UEdGraphPin* FromPin = GetHoveredPin())
					{
						ResultNode->AutowireNewNode(FromPin);
					}
				}

				Reply = FReply::Handled();
			}
		}
	}

	return Reply;
}

//------------------------------------------------------------------------------
UFunction const* FKismetFunctionDragDropAction::GetFunctionProperty() const
{
	check(OwningClass != nullptr);
	check(FunctionName != NAME_None);

	UFunction* Function = FindField<UFunction>(OwningClass, FunctionName);
	return Function;
}

//------------------------------------------------------------------------------
UBlueprintFunctionNodeSpawner* FKismetFunctionDragDropAction::GetDropAction(UEdGraph& Graph) const
{
	if (UEdGraph const* const TheHoveredGraph = &Graph)
	{
		if (UBlueprint* DropOnBlueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TheHoveredGraph))
		{
			// make temp list builder
			FGraphActionListBuilderBase TempListBuilder;
			TempListBuilder.OwnerOfTemporaries = NewObject<UEdGraph>(DropOnBlueprint);
			TempListBuilder.OwnerOfTemporaries->SetFlags(RF_Transient);

			UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();
			check(K2Schema != nullptr);

			if (UFunction const* Function = GetFunctionProperty())
			{
				return UBlueprintFunctionNodeSpawner::Create(Function);
			}
		}
	}
	return nullptr;
}

/*******************************************************************************
* FKismetMacroDragDropAction
*******************************************************************************/

//------------------------------------------------------------------------------
TSharedRef<FKismetMacroDragDropAction> FKismetMacroDragDropAction::New(
	TSharedPtr<FEdGraphSchemaAction> InActionNode,
	FName                 InMacroName, 
	UBlueprint*           InBlueprint, 
	UEdGraph*             InMacro, 
	FNodeCreationAnalytic AnalyticCallback)
{
	TSharedRef<FKismetMacroDragDropAction> Operation = MakeShareable(new FKismetMacroDragDropAction);
	Operation->SourceAction = InActionNode;
	Operation->MacroName = InMacroName;
	Operation->Macro = InMacro;
	Operation->Blueprint = InBlueprint;
	Operation->AnalyticCallback = AnalyticCallback;

	// Check to see if the macro has any latent functions in it, some graph types do not allow for latent functions
	bool bIsLatentMacro = FBlueprintEditorUtils::CheckIfGraphHasLatentFunctions(Operation->Macro);

	Operation->CanBeDroppedDelegate = FCanBeDroppedDelegate::CreateStatic(&CanMacroBeDropped, InMacro, bIsLatentMacro);

	Operation->Construct();
	return Operation;
}

//------------------------------------------------------------------------------
FKismetMacroDragDropAction::FKismetMacroDragDropAction()
	: Macro(nullptr)
	, Blueprint(nullptr)
{

}

//------------------------------------------------------------------------------
FReply FKismetMacroDragDropAction::DroppedOnPanel(TSharedRef<SWidget> const& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph)
{	
	check(Macro);
	check(CanBeDroppedDelegate.IsBound());

	FReply Reply = FReply::Unhandled();

	FText CannotDropReason = FText::GetEmpty();
	if (CanBeDroppedDelegate.Execute(TSharedPtr<FEdGraphSchemaAction>(), &Graph, CannotDropReason))
	{
		UK2Node_MacroInstance* MacroTemplate = NewObject<UK2Node_MacroInstance>();
		MacroTemplate->SetMacroGraph(Macro);
		AnalyticCallback.ExecuteIfBound();
		
		FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_MacroInstance>(&Graph, MacroTemplate, GraphPosition);
		Reply = FReply::Handled();
	}
	return Reply;
}

#undef LOCTEXT_NAMESPACE
