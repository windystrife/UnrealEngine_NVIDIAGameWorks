// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "ScopedTransaction.h"

#include "KismetCompiler.h"

#define LOCTEXT_NAMESPACE "CommutativeAssociativeBinaryOperatorNode"

int32 UK2Node_CommutativeAssociativeBinaryOperator::GetMaxInputPinsNum()
{
	return (TCHAR('Z') - TCHAR('A'));
}

FString UK2Node_CommutativeAssociativeBinaryOperator::GetNameForPin(int32 PinIndex)
{
	check(PinIndex < GetMaxInputPinsNum());
	FString Name;
	Name.AppendChar(TCHAR('A') + PinIndex);
	return Name;
}

UK2Node_CommutativeAssociativeBinaryOperator::UK2Node_CommutativeAssociativeBinaryOperator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumAdditionalInputs = 0;
}

UEdGraphPin* UK2Node_CommutativeAssociativeBinaryOperator::FindOutPin() const
{
	for(int32 PinIdx=0; PinIdx<Pins.Num(); PinIdx++)
	{
		if(EEdGraphPinDirection::EGPD_Output == Pins[PinIdx]->Direction)
		{
			return Pins[PinIdx];
		}
	}
	return NULL;
}

UEdGraphPin* UK2Node_CommutativeAssociativeBinaryOperator::FindSelfPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	for(int32 PinIdx=0; PinIdx<Pins.Num(); PinIdx++)
	{
		if(Pins[PinIdx]->PinName == K2Schema->PN_Self)
		{
			return Pins[PinIdx];
		}
	}
	return NULL;
}

bool UK2Node_CommutativeAssociativeBinaryOperator::CanAddPin() const
{
	return (NumAdditionalInputs + BinaryOperatorInputsNum) < GetMaxInputPinsNum();
}

bool UK2Node_CommutativeAssociativeBinaryOperator::CanRemovePin(const UEdGraphPin* Pin) const
{
	return (
		Pin &&
		Pin->ParentPin == nullptr &&
		NumAdditionalInputs &&
		(INDEX_NONE != Pins.IndexOfByKey(Pin)) &&
		(EEdGraphPinDirection::EGPD_Input == Pin->Direction)
	);
}

UEdGraphPin* UK2Node_CommutativeAssociativeBinaryOperator::GetInputPin(int32 InputPinIndex)
{
	const UEdGraphPin* OutPin = FindOutPin();
	const UEdGraphPin* SelfPin = FindSelfPin();

	int32 CurrentInputIndex = 0;
	for(int32 PinIdx=0; PinIdx<Pins.Num(); PinIdx++)
	{
		UEdGraphPin* CurrentPin = Pins[PinIdx];
		if((CurrentPin != OutPin) && (CurrentPin != SelfPin))
		{
			if(CurrentInputIndex == InputPinIndex)
			{
				return CurrentPin;
			}
			CurrentInputIndex++;
		}
	}
	return NULL;
}

FEdGraphPinType UK2Node_CommutativeAssociativeBinaryOperator::GetType() const
{
	for (int32 PinIt = 0; PinIt < Pins.Num(); PinIt++)
	{
		if (Pins[PinIt] != FindSelfPin())
		{
			return Pins[PinIt]->PinType;
		}
	}
	return FEdGraphPinType();
}

void UK2Node_CommutativeAssociativeBinaryOperator::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	const UFunction* Function = GetTargetFunction();
	if(NULL != Function)
	{
		check(Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
	
#if DO_CHECK
		ensure(FindOutPin());
		ensure((FindSelfPin() ? 4 : 3) ==  Pins.Num());
		{
			//VALIDATION
			const FEdGraphPinType InputType = GetType();
			int32 NativeInputPinsNum = 0;
			for (int32 PinIt = 0; PinIt < Pins.Num(); PinIt++)
			{
				const UEdGraphPin* Pin = Pins[PinIt];
				if (Pin != FindSelfPin())
				{
					ensure(InputType == Pin->PinType);
					NativeInputPinsNum += (EEdGraphPinDirection::EGPD_Input ==  Pin->Direction) ? 1 : 0;
				}
			}
			ensure(BinaryOperatorInputsNum == NativeInputPinsNum);
		}
#endif // DO_CHECK

		for (int32 i = 0; i < NumAdditionalInputs; ++i)
		{
			AddInputPinInner(i);
		}
	}
	else
	{
		const UClass* FunctionParentClass = FunctionReference.GetMemberParentClass(GetBlueprintClassFromNode());
		Message_Error(FString::Printf(
			*LOCTEXT("NoFunction_Error", "CommutativeAssociativeBinaryOperator has no function: '%s' class: '%s'").ToString(), 
			*FunctionReference.GetMemberName().ToString(),
			(FunctionParentClass ? *FunctionParentClass->GetName() : TEXT("None"))
			));
	}
}

void UK2Node_CommutativeAssociativeBinaryOperator::AddInputPinInner(int32 AdditionalPinIndex)
{
	const FEdGraphPinType InputType = GetType();
	CreatePin(EGPD_Input, 
		InputType.PinCategory, 
		InputType.PinSubCategory, 
		InputType.PinSubCategoryObject.Get(), 
		*GetNameForPin(AdditionalPinIndex + BinaryOperatorInputsNum),
		InputType.ContainerType, 
		InputType.bIsReference, 
		false,
		INDEX_NONE,
		InputType.PinValueType
	);
}

void UK2Node_CommutativeAssociativeBinaryOperator::AddInputPin()
{
	if(CanAddPin())
	{
		FScopedTransaction Transaction( LOCTEXT("AddPinTx", "AddPin") );
		Modify();

		AddInputPinInner(NumAdditionalInputs);
		++NumAdditionalInputs;
	
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

void UK2Node_CommutativeAssociativeBinaryOperator::RemoveInputPin(UEdGraphPin* Pin)
{
	if(CanRemovePin(Pin))
	{
		FScopedTransaction Transaction( LOCTEXT("RemovePinTx", "RemovePin") );
		Modify();

		if (RemovePin(Pin))
		{
			--NumAdditionalInputs;

			int32 NameIndex = 0;
			const UEdGraphPin* OutPin = FindOutPin();
			const UEdGraphPin* SelfPin = FindSelfPin();
			for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
			{
				UEdGraphPin* LocalPin = Pins[PinIndex];
				if(LocalPin && (LocalPin != OutPin) && (LocalPin != SelfPin))
				{
					const FString PinName = GetNameForPin(NameIndex);
					if(PinName != LocalPin->PinName)
					{
						LocalPin->Modify();
						LocalPin->PinName = PinName;
					}
					NameIndex++;
				}
			}
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
		}
	}
}

void UK2Node_CommutativeAssociativeBinaryOperator::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	Super::GetContextMenuActions(Context);

	if (!Context.bIsDebugging)
	{
		static FName CommutativeAssociativeBinaryOperatorNodeName = FName("CommutativeAssociativeBinaryOperatorNode");
		FText CommutativeAssociativeBinaryOperatorStr = LOCTEXT("CommutativeAssociativeBinaryOperatorNode", "Operator Node");
		if (Context.Pin != NULL)
		{
			if(CanRemovePin(Context.Pin))
			{
				Context.MenuBuilder->BeginSection(CommutativeAssociativeBinaryOperatorNodeName, CommutativeAssociativeBinaryOperatorStr);
				Context.MenuBuilder->AddMenuEntry(
					LOCTEXT("RemovePin", "Remove pin"),
					LOCTEXT("RemovePinTooltip", "Remove this input pin"),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateUObject(this, &UK2Node_CommutativeAssociativeBinaryOperator::RemoveInputPin, const_cast<UEdGraphPin*>(Context.Pin))
					)
				);
				Context.MenuBuilder->EndSection();
			}
		}
		else if(CanAddPin())
		{
			Context.MenuBuilder->BeginSection(CommutativeAssociativeBinaryOperatorNodeName, CommutativeAssociativeBinaryOperatorStr);
			Context.MenuBuilder->AddMenuEntry(
				LOCTEXT("AddPin", "Add pin"),
				LOCTEXT("AddPinTooltip", "Add another input pin"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateUObject(this, &UK2Node_CommutativeAssociativeBinaryOperator::AddInputPin)
				)
			);
			Context.MenuBuilder->EndSection();
		}
	}
}

void UK2Node_CommutativeAssociativeBinaryOperator::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (NumAdditionalInputs > 0)
	{
		const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

		UEdGraphPin* LastOutPin = NULL;
		const UFunction* const Function = GetTargetFunction();

		const UEdGraphPin* SrcOutPin = FindOutPin();
		const UEdGraphPin* SrcSelfPin = FindSelfPin();
		UEdGraphPin* SrcFirstInput = GetInputPin(0);
		check(SrcFirstInput);

		for(int32 PinIndex = 0; PinIndex < Pins.Num(); PinIndex++)
		{
			UEdGraphPin* CurrentPin = Pins[PinIndex];
			if( (CurrentPin == SrcFirstInput) || (CurrentPin == SrcOutPin) || (SrcSelfPin == CurrentPin) )
			{
				continue;
			}

			UK2Node_CommutativeAssociativeBinaryOperator* NewOperator = SourceGraph->CreateIntermediateNode<UK2Node_CommutativeAssociativeBinaryOperator>();
			NewOperator->SetFromFunction(Function);
			NewOperator->AllocateDefaultPins();
			CompilerContext.MessageLog.NotifyIntermediateObjectCreation(NewOperator, this);

			UEdGraphPin* NewOperatorInputA = NewOperator->GetInputPin(0);
			check(NewOperatorInputA);
			if(LastOutPin)
			{
				Schema->TryCreateConnection(LastOutPin, NewOperatorInputA);
			}
			else
			{
				// handle first created node (SrcFirstInput is skipped, and has no own node).
				CompilerContext.MovePinLinksToIntermediate(*SrcFirstInput, *NewOperatorInputA);
			}

			UEdGraphPin* NewOperatorInputB = NewOperator->GetInputPin(1);
			check(NewOperatorInputB);
			CompilerContext.MovePinLinksToIntermediate(*CurrentPin, *NewOperatorInputB);

			LastOutPin = NewOperator->FindOutPin();
		}

		check(LastOutPin);

		UEdGraphPin* TrueOutPin = FindOutPin();
		check(TrueOutPin);
		CompilerContext.MovePinLinksToIntermediate(*TrueOutPin, *LastOutPin);

		BreakAllNodeLinks();
	}
}

#undef LOCTEXT_NAMESPACE
