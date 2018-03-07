// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_SetFieldsInStruct.h"
#include "UObject/StructOnScope.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "MakeStructHandler.h"
#include "KismetCompiler.h"
#include "BlueprintEditorSettings.h"

#define LOCTEXT_NAMESPACE "K2Node_MakeStruct"

struct SetFieldsInStructHelper
{
	static const TCHAR* StructRefPinName()
	{
		return TEXT("StructRef");
	}

	static const TCHAR* StructOutPinName()
	{
		return TEXT("StructOut");
	}
};

class FKCHandler_SetFieldsInStruct : public FKCHandler_MakeStruct
{
public:
	FKCHandler_SetFieldsInStruct(FKismetCompilerContext& InCompilerContext)
		: FKCHandler_MakeStruct(InCompilerContext)
	{
		UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
		bAutoGenerateGotoForPure = false;
	}

	virtual UEdGraphPin* FindStructPinChecked(UEdGraphNode* InNode) const override
	{
		check(CastChecked<UK2Node_SetFieldsInStruct>(InNode));
		UEdGraphPin* FoundPin = InNode->FindPinChecked(SetFieldsInStructHelper::StructRefPinName());
		check(EGPD_Input == FoundPin->Direction);
		return FoundPin;
	}

	virtual void RegisterNet(FKismetFunctionContext& Context, UEdGraphPin* Net) override
	{
		UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();

		if (Net->Direction == EGPD_Output)
		{
			if (Net->ReferencePassThroughConnection)
			{
				UEdGraphPin* InputPinNet = FEdGraphUtilities::GetNetFromPin(Net->ReferencePassThroughConnection);
				FBPTerminal** InputPinTerm = Context.NetMap.Find(InputPinNet);
				if (InputPinTerm && !(*InputPinTerm)->bPassedByReference)
				{
					// We need a net for the output pin which we have thus far prevented from being registered
					FKCHandler_MakeStruct::RegisterNet(Context, Net);
				}
			}
		}
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		FKCHandler_MakeStruct::RegisterNets(Context, Node);

		UEdGraphPin* ReturnPin = Node->FindPin(SetFieldsInStructHelper::StructOutPinName());
		UEdGraphPin* ReturnStructNet = FEdGraphUtilities::GetNetFromPin(ReturnPin);

		UEdGraphPin* InputPin = Node->FindPinChecked(SetFieldsInStructHelper::StructRefPinName());
		UEdGraphPin* InputPinNet = FEdGraphUtilities::GetNetFromPin(InputPin);
		FBPTerminal** InputTermRef = Context.NetMap.Find(InputPinNet);
		
		if (InputTermRef == nullptr)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("MakeStruct_NoTerm_Error", "Failed to generate a term for the @@ pin; was it a struct reference that was left unset?").ToString(), InputPin);
		}
		else
		{
			FBPTerminal* InputTerm = *InputTermRef;
			if (InputTerm->bPassedByReference) //InputPinNet->PinType.bIsReference)
			{
				// Forward the net to the output pin because it's being passed by-ref and this pin is a by-ref pin
				Context.NetMap.Add(ReturnStructNet, InputTerm);
			}
		}
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		FKCHandler_MakeStruct::Compile(Context, Node);

		UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
		{
			UEdGraphPin* InputPin = Node->FindPinChecked(SetFieldsInStructHelper::StructRefPinName());
			UEdGraphPin* InputPinNet = FEdGraphUtilities::GetNetFromPin(InputPin);
			FBPTerminal** InputTerm = Context.NetMap.Find(InputPinNet);

			// If the InputTerm was not a by-ref, then we need to place the modified structure into the local output term with an AssignStatement
			if (InputTerm && !(*InputTerm)->bPassedByReference)
			{
				UEdGraphPin* ReturnPin = Node->FindPin(SetFieldsInStructHelper::StructOutPinName());
				UEdGraphPin* ReturnStructNet = FEdGraphUtilities::GetNetFromPin(ReturnPin);
				FBPTerminal** ReturnTerm = Context.NetMap.Find(ReturnStructNet);

				FBlueprintCompiledStatement& AssignStatement = Context.AppendStatementForNode(Node);
				AssignStatement.Type = KCST_Assignment;
				// The return term is a reference no matter the way we received it.
				(*ReturnTerm)->bPassedByReference = true;
				AssignStatement.LHS = *ReturnTerm;
				AssignStatement.RHS.Add(*InputTerm);
			}
		}

		GenerateSimpleThenGoto(Context, *Node);
	}
};

UK2Node_SetFieldsInStruct::UK2Node_SetFieldsInStruct(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_SetFieldsInStruct::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (Schema && StructType)
	{
		CreatePin(EGPD_Input, Schema->PC_Exec, FString(), nullptr, Schema->PN_Execute);
		CreatePin(EGPD_Output, Schema->PC_Exec, FString(), nullptr, Schema->PN_Then);

		UEdGraphPin* InPin = CreatePin(EGPD_Input, Schema->PC_Struct, FString(), StructType, SetFieldsInStructHelper::StructRefPinName(), EPinContainerType::None, true);

		UEdGraphPin* OutPin = CreatePin(EGPD_Output, Schema->PC_Struct, FString(), StructType, SetFieldsInStructHelper::StructOutPinName(), EPinContainerType::None, true);

		// Input pin will forward the ref to the output, if the input value is not a reference connection, a copy is made and modified instead and provided as a reference until the function is called again.
		InPin->AssignByRefPassThroughConnection(OutPin);

		OutPin->PinToolTip = LOCTEXT("SetFieldsInStruct_OutPinTooltip", "Reference to the input struct").ToString();
		{
			FStructOnScope StructOnScope(StructType);
			FSetFieldsInStructPinManager OptionalPinManager(StructOnScope.GetStructMemory());
			OptionalPinManager.RebuildPropertyList(ShowPinForProperties, StructType);
			OptionalPinManager.CreateVisiblePins(ShowPinForProperties, StructType, EGPD_Input, this);
		}
	}
}

FText UK2Node_SetFieldsInStruct::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (StructType == nullptr)
	{
		return LOCTEXT("SetFieldsInNullStructNodeTitle", "Set members in <unknown struct>");
	}
	else if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("StructName"), FText::FromName(StructType->GetFName()));
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("SetFieldsInStructNodeTitle", "Set members in {StructName}"), Args), this);
	}
	return CachedNodeTitle;
}

FText UK2Node_SetFieldsInStruct::GetTooltipText() const
{
	if (StructType == nullptr)
	{
		return LOCTEXT("SetFieldsInStruct_NullTooltip", "Adds a node that modifies an '<unknown struct>'");
	}
	else if (CachedTooltip.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedTooltip.SetCachedText(FText::Format(
			LOCTEXT("SetFieldsInStruct_Tooltip", "Adds a node that modifies a '{0}'"),
			FText::FromName(StructType->GetFName())
		), this);
	}
	return CachedTooltip;
}

FSlateIcon UK2Node_SetFieldsInStruct::GetIconAndTint(FLinearColor& OutColor) const
{
	return UK2Node_Variable::GetIconAndTint(OutColor);
}

void UK2Node_SetFieldsInStruct::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	UEdGraphPin* FoundPin = FindPin(SetFieldsInStructHelper::StructRefPinName());
	if (!FoundPin || (FoundPin->LinkedTo.Num() <= 0))
	{
		FText ErrorMessage = LOCTEXT("SetStructFields_NoStructRefError", "The @@ pin must be connected to the struct that you wish to set.");
		MessageLog.Error(*ErrorMessage.ToString(), FoundPin);
	}
}

FNodeHandlingFunctor* UK2Node_SetFieldsInStruct::CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_SetFieldsInStruct(CompilerContext);
}

bool UK2Node_SetFieldsInStruct::ShowCustomPinActions(const UEdGraphPin* Pin, bool bIgnorePinsNum)
{
	const int32 MinimalPinsNum = 5;
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	const auto Node = Pin ? Cast<const UK2Node_SetFieldsInStruct>(Pin->GetOwningNodeUnchecked()) : NULL;
	return Node
		&& ((Node->Pins.Num() > MinimalPinsNum) || bIgnorePinsNum)
		&& (EGPD_Input == Pin->Direction)
		&& (Pin->PinName != SetFieldsInStructHelper::StructRefPinName())
		&& !Schema->IsMetaPin(*Pin);
}

void UK2Node_SetFieldsInStruct::RemoveFieldPins(UEdGraphPin* Pin, EPinsToRemove Selection)
{
	if (ShowCustomPinActions(Pin, false) && (Pin->GetOwningNodeUnchecked() == this))
	{
		// Pretend that the action was done on the hidden parent pin if the pin is split
		while (Pin->ParentPin != nullptr)
		{
			Pin = Pin->ParentPin;
		}

		const bool bHideSelected = (Selection == EPinsToRemove::GivenPin);
		const bool bHideNotSelected = (Selection == EPinsToRemove::AllOtherPins);
		bool bWasChanged = false;
		for (FOptionalPinFromProperty& OptionalProperty : ShowPinForProperties)
		{
			const bool bSelected = (Pin->PinName == OptionalProperty.PropertyName.ToString());
			const bool bHide = (bSelected && bHideSelected) || (!bSelected && bHideNotSelected);
			if (OptionalProperty.bShowPin && bHide)
			{
				bWasChanged = true;
				OptionalProperty.bShowPin = false;
				Pin->bSavePinIfOrphaned = false;
			}
		}

		if (bWasChanged)
		{
			ReconstructNode();
		}
	}
}

bool UK2Node_SetFieldsInStruct::AllPinsAreShown() const
{
	UEdGraphPin* InputPin = FindPinChecked(SetFieldsInStructHelper::StructRefPinName(), EGPD_Input);

	// If the input struct pin is currently split, don't allow option to restore members
	if (InputPin != nullptr && InputPin->SubPins.Num() > 0)
	{
		return true;
	}

	for (const FOptionalPinFromProperty& OptionalProperty : ShowPinForProperties)
	{
		if (!OptionalProperty.bShowPin)
		{
			return false;
		}
	}

	return true;
}

void UK2Node_SetFieldsInStruct::RestoreAllPins()
{
	bool bWasChanged = false;
	for (FOptionalPinFromProperty& OptionalProperty : ShowPinForProperties)
	{
		if (!OptionalProperty.bShowPin)
		{
			bWasChanged = true;
			OptionalProperty.bShowPin = true;
		}
	}

	if (bWasChanged)
	{
		ReconstructNode();
	}
}

void UK2Node_SetFieldsInStruct::FSetFieldsInStructPinManager::GetRecordDefaults(UProperty* TestProperty, FOptionalPinFromProperty& Record) const
{
	FMakeStructPinManager::GetRecordDefaults(TestProperty, Record);

	Record.bShowPin = false;
}

bool UK2Node_SetFieldsInStruct::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (MyPin->bNotConnectable)
	{
		OutReason = LOCTEXT("SetFieldsInStructConnectionDisallowed", "This pin must enable the override to set a value!").ToString();
		return true;
	}

	return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

bool UK2Node_SetFieldsInStruct::CanSplitPin(const UEdGraphPin* Pin) const
{
	if (Super::CanSplitPin(Pin))
	{
		UEdGraphPin* InputPin = FindPinChecked(SetFieldsInStructHelper::StructRefPinName(), EGPD_Input);
		if (Pin == InputPin)
		{
			for (const FOptionalPinFromProperty& OptionalProperty : ShowPinForProperties)
			{
				if (OptionalProperty.bShowPin)
				{
					return false;
				}
			}
		}

		return true;
	}
	else
	{
		return false;
	}
}


#undef LOCTEXT_NAMESPACE
