// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_ExecutionSequence.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "KismetCompilerMisc.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node_MultiGate"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_ExecutionSequence

class FKCHandler_ExecutionSequence : public FNodeHandlingFunctor
{
public:
	FKCHandler_ExecutionSequence(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		// Make sure that the input pin is connected and valid for this block
		FEdGraphPinType ExpectedPinType;
		ExpectedPinType.PinCategory = UEdGraphSchema_K2::PC_Exec;

		UEdGraphPin* ExecTriggeringPin = Context.FindRequiredPinByName(Node, UEdGraphSchema_K2::PN_Execute, EGPD_Input);
		if ((ExecTriggeringPin == NULL) || !Context.ValidatePinType(ExecTriggeringPin, ExpectedPinType))
		{
			CompilerContext.MessageLog.Error(*FString::Printf(*LOCTEXT("NoValidExecutionPinForExecSeq_Error", "@@ must have a valid execution pin @@").ToString()), Node, ExecTriggeringPin);
			return;
		}
		else if (ExecTriggeringPin->LinkedTo.Num() == 0)
		{
			CompilerContext.MessageLog.Warning(*FString::Printf(*LOCTEXT("NodeNeverExecuted_Warning", "@@ will never be executed").ToString()), Node);
			return;
		}

		// Find the valid, connected output pins, and add them to the processing list
		TArray<UEdGraphPin*> OutputPins;
		for (int32 i = 0; i < Node->Pins.Num(); i++)
		{
			UEdGraphPin* CurrentPin = Node->Pins[i];
			if ((CurrentPin->Direction == EGPD_Output) && (CurrentPin->PinName.StartsWith(CompilerContext.GetSchema()->PN_Then)) && (CurrentPin->LinkedTo.Num() > 0))
			{
				OutputPins.Add(CurrentPin);
			}
		}

		//@TODO: Sort the pins by the number appended to the pin!

		// Process the pins, if there are any valid entries
		if (OutputPins.Num() > 0)
		{
			if (Context.IsDebuggingOrInstrumentationRequired() && (OutputPins.Num() > 1))
			{
				const FString NodeComment = Node->NodeComment.IsEmpty() ? Node->GetName() : Node->NodeComment;

				// Assuming sequence X goes to A, B, C, we want to emit:
				//   X: push X1
				//      goto A
				//  X1: debug site
				//      push X2
				//      goto B
				//  X2: debug site
				//      goto C

				// A push statement we need to patch up on the next pass (e.g., push X1 before we know where X1 is)
				FBlueprintCompiledStatement* LastPushStatement = NULL;

				for (int32 i = 0; i < OutputPins.Num(); ++i)
				{
					// Emit the debug site and patch up the previous jump if we're on subsequent steps
					const bool bNotFirstIndex = i > 0;
					if (bNotFirstIndex)
					{
						// Emit a debug site
						FBlueprintCompiledStatement& DebugSiteAndJumpTarget = Context.AppendStatementForNode(Node);
						DebugSiteAndJumpTarget.Type = Context.GetBreakpointType();
						DebugSiteAndJumpTarget.Comment = NodeComment;
						DebugSiteAndJumpTarget.bIsJumpTarget = true;

						// Patch up the previous push jump target
						check(LastPushStatement);
						LastPushStatement->TargetLabel = &DebugSiteAndJumpTarget;
					}

					// Emit a push to get to the next step in the sequence, unless we're the last one or this is an instrumented build
					const bool bNotLastIndex = ((i + 1) < OutputPins.Num());
					if (bNotLastIndex)
					{
						FBlueprintCompiledStatement& PushExecutionState = Context.AppendStatementForNode(Node);
						PushExecutionState.Type = KCST_PushState;
						LastPushStatement = &PushExecutionState;
					}

					// Emit the goto to the actual state
					FBlueprintCompiledStatement& GotoSequenceLinkedState = Context.AppendStatementForNode(Node);
					GotoSequenceLinkedState.Type = KCST_UnconditionalGoto;
					Context.GotoFixupRequestMap.Add(&GotoSequenceLinkedState, OutputPins[i]);
				}

				check(LastPushStatement);
			}
			else
			{
				// Directly emit pushes to execute the remaining branches
				for (int32 i = OutputPins.Num() - 1; i > 0; i--)
				{
					FBlueprintCompiledStatement& PushExecutionState = Context.AppendStatementForNode(Node);
					PushExecutionState.Type = KCST_PushState;
					Context.GotoFixupRequestMap.Add(&PushExecutionState, OutputPins[i]);
				}

				// Immediately jump to the first pin
				UEdGraphNode* NextNode = OutputPins[0]->LinkedTo[0]->GetOwningNode();
				FBlueprintCompiledStatement& NextExecutionState = Context.AppendStatementForNode(Node);
				NextExecutionState.Type = KCST_UnconditionalGoto;
				Context.GotoFixupRequestMap.Add(&NextExecutionState, OutputPins[0]);
			}
		}
		else
		{
			FBlueprintCompiledStatement& NextExecutionState = Context.AppendStatementForNode(Node);
			NextExecutionState.Type = KCST_EndOfThread;
			CompilerContext.MessageLog.Warning(*LOCTEXT("NoValidOutput_Warning", "@@ has no valid output").ToString(), Node);
		}
	}
};

UK2Node_ExecutionSequence::UK2Node_ExecutionSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_ExecutionSequence::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	CreatePin(EGPD_Input, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_Execute);

	// Add two default pins
	CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, GetPinNameGivenIndex(0));
	CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, GetPinNameGivenIndex(1));

	Super::AllocateDefaultPins();
}

FText UK2Node_ExecutionSequence::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "Sequence", "Sequence");
}

FSlateIcon UK2Node_ExecutionSequence::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Sequence_16x");
	return Icon;
}

FLinearColor UK2Node_ExecutionSequence::GetNodeTitleColor() const
{
	return FLinearColor::White;
}

FText UK2Node_ExecutionSequence::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "ExecutePinInOrder_Tooltip", "Executes a series of pins in order");
}

FString UK2Node_ExecutionSequence::GetUniquePinName()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	FString NewPinName;
	int32 i = 0;
	while (true)
	{
		NewPinName = GetPinNameGivenIndex(i++);
		if (!FindPin(NewPinName))
		{
			break;
		}
	}

	return NewPinName;
}

void UK2Node_ExecutionSequence::AddInputPin()
{
	Modify();
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, GetUniquePinName());
}

void UK2Node_ExecutionSequence::RemovePinFromExecutionNode(UEdGraphPin* TargetPin) 
{
	UK2Node_ExecutionSequence* OwningSeq = Cast<UK2Node_ExecutionSequence>( TargetPin->GetOwningNode() );
	if (OwningSeq)
	{
		OwningSeq->Pins.Remove(TargetPin);
		TargetPin->MarkPendingKill();

		// Renumber the pins so the numbering is compact
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		int32 ThenIndex = 0;
		for (int32 i = 0; i < OwningSeq->Pins.Num(); ++i)
		{
			UEdGraphPin* PotentialPin = OwningSeq->Pins[i];
			if (K2Schema->IsExecPin(*PotentialPin) && (PotentialPin->Direction == EGPD_Output))
			{
				PotentialPin->PinName = GetPinNameGivenIndex(ThenIndex);
				++ThenIndex;
			}
		}
	}
}

bool UK2Node_ExecutionSequence::CanRemoveExecutionPin() const
{
	int32 NumOutPins = 0;

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	for (int32 i = 0; i < Pins.Num(); ++i)
	{
		UEdGraphPin* PotentialPin = Pins[i];
		if (K2Schema->IsExecPin(*PotentialPin) && (PotentialPin->Direction == EGPD_Output))
		{
			NumOutPins++;
		}
	}

	return (NumOutPins > 2);
}

FString UK2Node_ExecutionSequence::GetPinNameGivenIndex(int32 Index) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	return K2Schema->PN_Then + FString::Printf(TEXT("_%d"), Index);
}

void UK2Node_ExecutionSequence::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::AllocateDefaultPins();

	// Create the execution input pin
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	CreatePin(EGPD_Input, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_Execute);

	// Create a new pin for each old execution output pin, and coerce the names to match on both sides
	int32 ExecOutPinCount = 0;
	for (int32 i = 0; i < OldPins.Num(); ++i)
	{
		UEdGraphPin* TestPin = OldPins[i];
		if (K2Schema->IsExecPin(*TestPin) && (TestPin->Direction == EGPD_Output))
		{
			FString NewPinName = GetPinNameGivenIndex(ExecOutPinCount);
			ExecOutPinCount++;

			// Make sure the old pin and new pin names match
			TestPin->PinName = NewPinName;

			// Create the new output pin to match
			CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, NewPinName);
		}
	}
}

UEdGraphPin * UK2Node_ExecutionSequence::GetThenPinGivenIndex(int32 Index)
{
	FString PinName = GetPinNameGivenIndex(Index);
	return FindPin(PinName);
}

FNodeHandlingFunctor* UK2Node_ExecutionSequence::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_ExecutionSequence(CompilerContext);
}

void UK2Node_ExecutionSequence::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_ExecutionSequence::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::FlowControl);
}

#undef LOCTEXT_NAMESPACE
