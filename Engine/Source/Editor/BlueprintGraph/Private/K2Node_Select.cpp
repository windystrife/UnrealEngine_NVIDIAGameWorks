// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_Select.h"
#include "Misc/CoreMisc.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet/KismetMathLibrary.h"
#include "EdGraphUtilities.h"
#include "BPTerminal.h"
#include "BlueprintCompiledStatement.h"
#include "KismetCompilerMisc.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node_Select"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_Select

class FKCHandler_SelectRef : public FNodeHandlingFunctor
{
protected:
	TMap<UEdGraphNode*, FBPTerminal*> DefaultTermMap;

public:
	FKCHandler_SelectRef(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_Select* SelectNode = Cast<UK2Node_Select>(Node);
		UEdGraphPin* ReturnPin = SelectNode ? SelectNode->GetReturnValuePin() : nullptr;
		if (!ReturnPin)
		{
			Context.MessageLog.Error(*LOCTEXT("Error_NoReturnPin", "No return pin in @@").ToString(), Node);
			return;
		}

		// return inline term
		if (Context.NetMap.Find(ReturnPin))
		{
			Context.MessageLog.Error(*LOCTEXT("Error_ReturnTermAlreadyRegistered", "ICE: Return term is already registered @@").ToString(), Node);
			return;
		}

		{
			FBPTerminal* Term = new (Context.InlineGeneratedValues) FBPTerminal();
			Term->CopyFromPin(ReturnPin, Context.NetNameMap->MakeValidName(ReturnPin));
			Context.NetMap.Add(ReturnPin, Term);
		}

		//Register Default term
		{
			TArray<UEdGraphPin*> OptionPins;
			SelectNode->GetOptionPins(OptionPins);
			if (!OptionPins.Num())
			{
				Context.MessageLog.Error(*LOCTEXT("Error_NoOptionPin", "No option pin in @@").ToString(), Node);
				return;
			}

			FString DefaultTermName = Context.NetNameMap->MakeValidName(Node) + TEXT("_Default");
			FBPTerminal* DefaultTerm = Context.CreateLocalTerminalFromPinAutoChooseScope(OptionPins[0], DefaultTermName);
			DefaultTermMap.Add(Node, DefaultTerm);
		}

		FNodeHandlingFunctor::RegisterNets(Context, Node);
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_Select* SelectNode = CastChecked<UK2Node_Select>(Node);
		FBPTerminal* DefaultTerm = nullptr;
		FBPTerminal* ReturnTerm = nullptr;
		FBPTerminal* IndexTerm = nullptr;

		{
			UEdGraphPin* IndexPin = SelectNode->GetIndexPin();
			UEdGraphPin* IndexPinNet = IndexPin ? FEdGraphUtilities::GetNetFromPin(IndexPin) : nullptr;
			FBPTerminal** IndexTermPtr = IndexPinNet ? Context.NetMap.Find(IndexPinNet) : nullptr;
			IndexTerm = IndexTermPtr ? *IndexTermPtr : nullptr;

			UEdGraphPin* ReturnPin = SelectNode->GetReturnValuePin();
			UEdGraphPin* ReturnPinNet = ReturnPin ? FEdGraphUtilities::GetNetFromPin(ReturnPin) : nullptr;
			FBPTerminal** ReturnTermPtr = ReturnPinNet ? Context.NetMap.Find(ReturnPinNet) : nullptr;
			ReturnTerm = ReturnTermPtr ? *ReturnTermPtr : nullptr;

			FBPTerminal** DefaultTermPtr = DefaultTermMap.Find(SelectNode);
			DefaultTerm = DefaultTermPtr ? *DefaultTermPtr : nullptr;
			
			if (!ReturnTerm || !IndexTerm || !DefaultTerm)
			{
				Context.MessageLog.Error(*LOCTEXT("Error_InvalidSelect", "ICE: invalid select node @@").ToString(), Node);
				return;
			}
		}

		FBlueprintCompiledStatement* SelectStatement = new FBlueprintCompiledStatement();
		SelectStatement->Type = EKismetCompiledStatementType::KCST_SwitchValue;
		Context.AllGeneratedStatements.Add(SelectStatement);
		ReturnTerm->InlineGeneratedParameter = SelectStatement;
		SelectStatement->RHS.Add(IndexTerm);

		TArray<UEdGraphPin*> OptionPins;
		SelectNode->GetOptionPins(OptionPins);
		for (int32 OptionIdx = 0; OptionIdx < OptionPins.Num(); OptionIdx++)
		{
			{
				FBPTerminal* LiteralTerm = Context.CreateLocalTerminal(ETerminalSpecification::TS_Literal);
				LiteralTerm->Type = IndexTerm->Type;
				LiteralTerm->bIsLiteral = true;
				const UEnum* NodeEnum = SelectNode->GetEnum();
				LiteralTerm->Name = NodeEnum ? OptionPins[OptionIdx]->PinName : FString::Printf(TEXT("%d"), OptionIdx); //-V595

				if (!CompilerContext.GetSchema()->DefaultValueSimpleValidation(LiteralTerm->Type, LiteralTerm->Name, LiteralTerm->Name, nullptr, FText()))
				{
					Context.MessageLog.Error(*FString::Printf(*LOCTEXT("Error_InvalidOptionValue", "Invalid option value '%s' in @@").ToString(), *LiteralTerm->Name), Node);
					return;
				}
				SelectStatement->RHS.Add(LiteralTerm);
			}
			{
				UEdGraphPin* NetPin = OptionPins[OptionIdx] ? FEdGraphUtilities::GetNetFromPin(OptionPins[OptionIdx]) : nullptr;
				FBPTerminal** ValueTermPtr = NetPin ? Context.NetMap.Find(NetPin) : nullptr;
				FBPTerminal* ValueTerm = ValueTermPtr ? *ValueTermPtr : nullptr;
				if (!ensure(ValueTerm))
				{
					Context.MessageLog.Error(*LOCTEXT("Error_NoTermFound", "No term registered for pin @@").ToString(), NetPin);
					return;
				}
				SelectStatement->RHS.Add(ValueTerm);
			}
		}

		SelectStatement->RHS.Add(DefaultTerm);
	}
};

class FKCHandler_Select : public FNodeHandlingFunctor
{
protected:
	TMap<UEdGraphNode*, FBPTerminal*> BoolTermMap;

public:
	FKCHandler_Select(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		FNodeHandlingFunctor::RegisterNets(Context, Node);

		// Create the net for the return value manually as it's a special case Output Direction pin
		UK2Node_Select* SelectNode = Cast<UK2Node_Select>(Node);
		UEdGraphPin* ReturnPin = SelectNode->GetReturnValuePin();

		FBPTerminal* Term = Context.CreateLocalTerminalFromPinAutoChooseScope(ReturnPin, Context.NetNameMap->MakeValidName(ReturnPin));
		Context.NetMap.Add(SelectNode->GetReturnValuePin(), Term);

		// Create a term to determine if the compare was successful or not
		FBPTerminal* BoolTerm = Context.CreateLocalTerminal();
		BoolTerm->Type.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		BoolTerm->Source = Node;
		BoolTerm->Name = Context.NetNameMap->MakeValidName(Node) + TEXT("_CmpSuccess");
		BoolTermMap.Add(Node, BoolTerm);
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		// Cast the node and get all the input pins
		UK2Node_Select* SelectNode = Cast<UK2Node_Select>(Node);
		TArray<UEdGraphPin*> OptionPins;
		SelectNode->GetOptionPins(OptionPins);
		UEdGraphPin* IndexPin = SelectNode->GetIndexPin();

		// Get the kismet term for the (Condition or Index) that will determine which option to use
		UEdGraphPin* PinToTry = FEdGraphUtilities::GetNetFromPin(IndexPin);
		FBPTerminal** ConditionTerm = Context.NetMap.Find(PinToTry);

		// Get the kismet term for the return value
		UEdGraphPin* ReturnPin = SelectNode->GetReturnValuePin();
		FBPTerminal** ReturnTerm = Context.NetMap.Find(ReturnPin);

		// Don't proceed if there is no return value or there is no selection
		if (ConditionTerm != NULL && ReturnTerm != NULL)
		{
			FName ConditionalFunctionName = "";
			UClass* ConditionalFunctionClass = NULL;
			SelectNode->GetConditionalFunction(ConditionalFunctionName, &ConditionalFunctionClass);
			UFunction* ConditionFunction = FindField<UFunction>(ConditionalFunctionClass, ConditionalFunctionName);

			// Find the local boolean for use in the equality call function below (BoolTerm = result of EqualEqual_IntInt or NotEqual_BoolBool)
			FBPTerminal* BoolTerm = BoolTermMap.FindRef(SelectNode);

			// We need to keep a pointer to the previous IfNot statement so it can be linked to the next conditional statement
			FBlueprintCompiledStatement* PrevIfNotStatement = NULL;

			// Keep an array of all the unconditional goto statements so we can clean up their jumps after the noop statement is created
			TArray<FBlueprintCompiledStatement*> GotoStatementList;

			// Loop through all the options
			for (int32 OptionIdx = 0; OptionIdx < OptionPins.Num(); OptionIdx++)
			{
				// Create a CallFunction statement with the condition function from the Select class
				FBlueprintCompiledStatement& Statement = Context.AppendStatementForNode(Node);
				Statement.Type = KCST_CallFunction;
				Statement.FunctionToCall = ConditionFunction;
				Statement.FunctionContext = NULL;
				Statement.bIsParentContext = false;
				// BoolTerm will be the return value of the condition statement
				Statement.LHS = BoolTerm;
				// The condition passed into the Select node
				Statement.RHS.Add(*ConditionTerm);
				// Create a local int for use in the equality call function below (LiteralTerm = the right hand side of the EqualEqual_IntInt or NotEqual_BoolBool statement)
				FBPTerminal* LiteralTerm = Context.CreateLocalTerminal(ETerminalSpecification::TS_Literal);
				LiteralTerm->bIsLiteral = true;
				LiteralTerm->Type.PinCategory = UEdGraphSchema_K2::PC_Int;

				if (UEnum* NodeEnum = SelectNode->GetEnum())
				{
					int32 EnumValue = NodeEnum->GetValueByName(FName(*OptionPins[OptionIdx]->PinName));
					LiteralTerm->Name = FString::Printf(TEXT("%d"), EnumValue);
				}
				else
				{
					LiteralTerm->Name = FString::Printf(TEXT("%d"), OptionIdx);
				}
				Statement.RHS.Add(LiteralTerm);
				// If there is a previous IfNot statement, hook this one to that one for jumping
				if (PrevIfNotStatement)
				{
					Statement.bIsJumpTarget = true;
					PrevIfNotStatement->TargetLabel = &Statement;
				}

				// Create a GotoIfNot statement using the BoolTerm from above as the condition
				FBlueprintCompiledStatement* IfNotStatement = &Context.AppendStatementForNode(Node);
				IfNotStatement->Type = KCST_GotoIfNot;
				IfNotStatement->LHS = BoolTerm;

				// Create an assignment statement
				FBlueprintCompiledStatement& AssignStatement = Context.AppendStatementForNode(Node);
				AssignStatement.Type = KCST_Assignment;
				AssignStatement.LHS = *ReturnTerm;
				// Get the kismet term from the option pin
				UEdGraphPin* OptionPinToTry = FEdGraphUtilities::GetNetFromPin(OptionPins[OptionIdx]);
				FBPTerminal** OptionTerm = Context.NetMap.Find(OptionPinToTry);
				if (!OptionTerm)
				{
					Context.MessageLog.Error(*LOCTEXT("Error_UnregisterOptionPin", "Unregister option pin @@").ToString(), OptionPins[OptionIdx]);
					return;
				}
				AssignStatement.RHS.Add(*OptionTerm);

				// Create an unconditional goto to exit the node
				FBlueprintCompiledStatement& GotoStatement = Context.AppendStatementForNode(Node);
				GotoStatement.Type = KCST_UnconditionalGoto;
				GotoStatementList.Add(&GotoStatement);

				// If this is the last IfNot statement, hook the jump to an error message
				if (OptionIdx == OptionPins.Num() - 1)
				{
					// Create a CallFunction statement for doing a print string of our error message
					FBlueprintCompiledStatement& PrintStatement = Context.AppendStatementForNode(Node);
					PrintStatement.Type = KCST_CallFunction;
					PrintStatement.bIsJumpTarget = true;
					FName PrintStringFunctionName = "";
					UClass* PrintStringFunctionClass = NULL;
					SelectNode->GetPrintStringFunction(PrintStringFunctionName, &PrintStringFunctionClass);
					UFunction* PrintFunction = FindField<UFunction>(PrintStringFunctionClass, PrintStringFunctionName);
					PrintStatement.FunctionToCall = PrintFunction;
					PrintStatement.FunctionContext = NULL;
					PrintStatement.bIsParentContext = false;

					// Create a local int for use in the equality call function below (LiteralTerm = the right hand side of the EqualEqual_IntInt or NotEqual_BoolBool statement)
					FBPTerminal* LiteralStringTerm = Context.CreateLocalTerminal(ETerminalSpecification::TS_Literal);
					LiteralStringTerm->bIsLiteral = true;
					LiteralStringTerm->Type.PinCategory = UEdGraphSchema_K2::PC_String;

					FString SelectionNodeType(TEXT("NONE"));
					if (IndexPin)
					{
						UEnum* EnumObject = Cast<UEnum>(IndexPin->PinType.PinSubCategoryObject.Get());
						if (EnumObject != NULL)
						{
							SelectionNodeType = EnumObject->GetName();
						}
						else
						{
							// Not an enum, so just use the basic type
							SelectionNodeType = IndexPin->PinType.PinCategory;
						}
					}

					const UEdGraph* OwningGraph = Context.MessageLog.FindSourceObjectTypeChecked<UEdGraph>( SelectNode->GetGraph() );
					LiteralStringTerm->Name =
						FString::Printf(*LOCTEXT("SelectNodeIndexWarning", "Graph %s: Selection Node of type %s failed! Out of bounds indexing of the options. There are only %d options available.").ToString(),
						(OwningGraph) ? *OwningGraph->GetFullName() : TEXT("NONE"),
						*SelectionNodeType,
						OptionPins.Num());
					PrintStatement.RHS.Add(LiteralStringTerm);

					// Hook the IfNot statement's jump target to this statement
					IfNotStatement->TargetLabel = &PrintStatement;
				}

				PrevIfNotStatement = IfNotStatement;
			}

			// Create a noop to jump to so the unconditional goto statements can exit the node after successful assignment
			FBlueprintCompiledStatement& NopStatement = Context.AppendStatementForNode(Node);
			NopStatement.Type = KCST_Nop;
			NopStatement.bIsJumpTarget = true;
			// Loop through the unconditional goto statements and fix their jump targets
			for (FBlueprintCompiledStatement* GotoStatement : GotoStatementList)
			{
				GotoStatement->TargetLabel = &NopStatement;
			}
		}
	}
};

UK2Node_Select::UK2Node_Select(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	NumOptionPins = 2;

	IndexPinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
	IndexPinType.PinSubCategory = UEdGraphSchema_K2::PSC_Index;
	IndexPinType.PinSubCategoryObject = nullptr;

	OrphanedPinSaveMode = ESaveOrphanPinMode::SaveNone;
}

void UK2Node_Select::AllocateDefaultPins()
{
	// To refresh, just in case it changed
	SetEnum(Enum, true);

	// No need to reconstruct the node after force setting the enum, we are at the start of reconstruction already
	bReconstructNode = false;

	if (Enum)
	{
		NumOptionPins = EnumEntries.Num();
	}

	static const FBoolConfigValueHelper UseSelectRef(TEXT("Kismet"), TEXT("bUseSelectRef"), GEngineIni);

	// Create the option pins
	for (int32 Idx = 0; Idx < NumOptionPins; Idx++)
	{
		UEdGraphPin* NewPin = nullptr;

		if (Enum)
		{
			const FString PinName = EnumEntries[Idx].ToString();
			UEdGraphPin* TempPin = FindPin(PinName);
			if (!TempPin)
			{
				NewPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, FString(), nullptr, PinName);
			}
		}
		else
		{
			const FString PinName = FString::Printf(TEXT("Option %d"), Idx);
			NewPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, FString(), nullptr, PinName);
		}

		if (NewPin)
		{
			NewPin->bDisplayAsMutableRef = UseSelectRef;
			if (IndexPinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
			{
				NewPin->PinFriendlyName = (Idx == 0 ? GFalse : GTrue);
			}
			else if (Idx < EnumEntryFriendlyNames.Num())
			{
				NewPin->PinFriendlyName = EnumEntryFriendlyNames[Idx];
			}
		}
	}

	// Create the index wildcard pin
	CreatePin(EGPD_Input, IndexPinType.PinCategory, IndexPinType.PinSubCategory, IndexPinType.PinSubCategoryObject.Get(), TEXT("Index"));

	// Create the return value
	UEdGraphPin* ReturnPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, FString(), nullptr, UEdGraphSchema_K2::PN_ReturnValue);
	ReturnPin->bDisplayAsMutableRef = UseSelectRef;

	Super::AllocateDefaultPins();
}

void UK2Node_Select::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (FromPin)
	{
		// Attempt to autowire to the index pin as users generally drag off of something intending to use
		// it as an index in a select statement rather than an arbitrary entry:
		const UEdGraphSchema_K2* K2Schema = CastChecked<UEdGraphSchema_K2>(GetSchema());
		UEdGraphPin* IndexPin = GetIndexPin();
		ECanCreateConnectionResponse ConnectResponse = K2Schema->CanCreateConnection(FromPin, IndexPin).Response;
		if (ConnectResponse == ECanCreateConnectionResponse::CONNECT_RESPONSE_MAKE)
		{
			if (K2Schema->TryCreateConnection(FromPin, IndexPin))
			{
				FromPin->GetOwningNode()->NodeConnectionListChanged();
				this->NodeConnectionListChanged();
				return;
			}
		}
	}

	// No connection made, just use default autowire logic:
	Super::AutowireNewNode(FromPin);
}

FText UK2Node_Select::GetTooltipText() const
{
	return LOCTEXT("SelectNodeTooltip", "Return the option at Index, (first option is indexed at 0)");
}

FText UK2Node_Select::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Select", "Select");
}

UK2Node::ERedirectType UK2Node_Select::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	// Check to see if the new pin name matches the old pin name (case insensitive - since the base uses Stricmp() to compare pin names, we also ignore case here).
	if(Enum != nullptr && NewPinIndex < NumOptionPins && !NewPin->PinName.Equals(OldPin->PinName, ESearchCase::IgnoreCase))
	{
		// The names don't match, so check for an enum redirect from the old pin name.
		int32 EnumIndex = Enum->GetIndexByNameString(OldPin->PinName);
		if(EnumIndex != INDEX_NONE)
		{
			// Found a redirect. Attempt to match it to the new pin name.
			FString NewPinName = Enum->GetNameStringByIndex(EnumIndex);
			if(NewPinName.Equals(NewPin->PinName, ESearchCase::IgnoreCase))
			{
				// The redirect is a match, so we can reconstruct this pin using the old pin's state.
				return UK2Node::ERedirectType_Name;
			}
		}
	}

	// Fall back to base class functionality for all other cases.
	return Super::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
}

void UK2Node_Select::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// See if this node was saved in the old version with a boolean as the condition
	UEdGraphPin* OldConditionPin = nullptr;
	UEdGraphPin* OldIndexPin = nullptr;
	UEdGraphPin* OldReturnPin = nullptr;
	for (UEdGraphPin* OldPin : OldPins)
	{
		if (OldPin->PinName == TEXT("bPickOption0"))
		{
			OldConditionPin = OldPin;
		}
		else if (OldPin->PinName == TEXT("Index"))
		{
			OldIndexPin = OldPin;
		}
		else if (OldPin->PinName == Schema->PN_ReturnValue)
		{
			OldReturnPin = OldPin;
		}
	}

	UEdGraphPin* ReturnPin = GetReturnValuePin();
	check(ReturnPin);

	if (OldReturnPin && (ReturnPin->PinType.PinCategory == Schema->PC_Wildcard))
	{
		// Always copy type from node prior, if pins have changed those will error at compilation time
		ReturnPin->PinType = OldReturnPin->PinType;
	}

	UEdGraphPin* IndexPin = GetIndexPin();
	check(IndexPin);

	// If we are fixing up an old bool node (swap the options and copy the condition links)
	if (OldConditionPin)
	{
		// Set the index pin type
		IndexPinType.PinCategory = Schema->PC_Boolean;
		IndexPinType.PinSubCategory.Reset();
		IndexPinType.PinSubCategoryObject = nullptr;

		// Set the pin type and Copy the pin
		IndexPin->PinType = IndexPinType;
		Schema->CopyPinLinks(*OldConditionPin, *IndexPin);
		// If we copy links, we need to send a notification
		if (IndexPin->LinkedTo.Num() > 0)
		{
			PinConnectionListChanged(IndexPin);
		}

		UEdGraphPin* OptionPin0 = FindPin("Option 0");
		UEdGraphPin* OptionPin1 = FindPin("Option 1");

		for (UEdGraphPin* OldPin : OldPins)
		{
			if (OldPin->PinName == OptionPin0->PinName)
			{
				Schema->MovePinLinks(*OldPin, *OptionPin1);
			}
			else if (OldPin->PinName == OptionPin1->PinName)
			{
				Schema->MovePinLinks(*OldPin, *OptionPin0);
			}
		}
	}

	// If the index pin has links or a default value but is a wildcard, this is an old int pin so convert it
	if (OldIndexPin &&
		IndexPinType.PinCategory == Schema->PC_Wildcard &&
		(OldIndexPin->LinkedTo.Num() > 0 || !OldIndexPin->DefaultValue.IsEmpty()))
	{
		IndexPinType.PinCategory = Schema->PC_Int;
		IndexPinType.PinSubCategory.Reset();
		IndexPinType.PinSubCategoryObject = nullptr;
		IndexPin->PinType = IndexPinType;
	}

	// Set up default values for index and option pins now that the information is available
	Schema->SetPinAutogeneratedDefaultValueBasedOnType(IndexPin);

	const bool bFillTypeFromReturn = ReturnPin->PinType.PinCategory != Schema->PC_Wildcard;
	TArray<UEdGraphPin*> OptionPins;
	GetOptionPins(OptionPins);
	for (UEdGraphPin* Pin : OptionPins)
	{
		const bool bTypeShouldBeFilled = Pin && (Pin->PinType.PinCategory == Schema->PC_Wildcard);
		if (bTypeShouldBeFilled && bFillTypeFromReturn)
		{
			Pin->PinType = ReturnPin->PinType;			
		}
		Schema->SetPinAutogeneratedDefaultValueBasedOnType(Pin);
	}
}

void UK2Node_Select::PostReconstructNode()
{
	// After ReconstructNode we must be sure that no additional reconstruction is required
	bReconstructNode = false;

	UEdGraphPin* ReturnPin = GetReturnValuePin();
	const bool bFillTypeFromConnected = ReturnPin && (ReturnPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard);

	if (bFillTypeFromConnected)
	{
		FEdGraphPinType PinType = ReturnPin->PinType;

		if (ReturnPin->LinkedTo.Num() > 0)
		{
			PinType = ReturnPin->LinkedTo[0]->PinType;
		}
		else
		{
			TArray<UEdGraphPin*> OptionPins;
			GetOptionPins(OptionPins);
			for (UEdGraphPin* Pin : OptionPins)
			{
				if (Pin && Pin->LinkedTo.Num() > 0)
				{
					PinType = Pin->LinkedTo[0]->PinType;
					break;
				}
			}
		}

		ReturnPin->PinType = PinType;
		PinTypeChanged(ReturnPin);
	}

	Super::PostReconstructNode();
}

/** Determine if any pins are connected, if so make all the other pins the same type, if not, make sure pins are switched back to wildcards */
void UK2Node_Select::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// If this is the Enum pin we need to set the enum and reconstruct the node
	if (Pin == GetIndexPin())
	{
		// If the index pin was just linked to another pin
		if (Pin->LinkedTo.Num() > 0 && Pin->PinType.PinCategory == Schema->PC_Wildcard)
		{
			UEdGraphPin* LinkPin = Pin->LinkedTo[0];

			if (Pin->PinType != LinkPin->PinType)
			{
				Pin->PinType = LinkPin->PinType;

				PinTypeChanged(Pin);
			}
		}
	}
	else
	{
		// Grab references to all option pins and the return pin
		TArray<UEdGraphPin*> OptionPins;
		GetOptionPins(OptionPins);
		UEdGraphPin* ReturnPin = FindPin(Schema->PN_ReturnValue);

		// See if this pin is one of the wildcard pins
		bool bIsWildcardPin = (Pin == ReturnPin || OptionPins.Find(Pin) != INDEX_NONE) && Pin->PinType.PinCategory == Schema->PC_Wildcard;

		// If the pin was one of the wildcards we have to handle it specially
		if (bIsWildcardPin)
		{
			// If the pin is linked, make sure the other wildcard pins match
			if (Pin->LinkedTo.Num() > 0)
			{
				UEdGraphPin* LinkPin = Pin->LinkedTo[0];

				if (Pin->PinType != LinkPin->PinType)
				{
					Pin->PinType = LinkPin->PinType;

					PinTypeChanged(Pin);
				}
			}
		}
	}
}

UEdGraphPin* UK2Node_Select::GetReturnValuePin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin(K2Schema->PN_ReturnValue);
	check(Pin != NULL);
	return Pin;
}

UEdGraphPin* UK2Node_Select::GetIndexPin() const
{
	UEdGraphPin* Pin = GetIndexPinUnchecked();
	check(Pin != NULL);
	return Pin;
}

UEdGraphPin* UK2Node_Select::GetIndexPinUnchecked() const
{
	return FindPin("Index");
}

void UK2Node_Select::GetOptionPins(TArray<UEdGraphPin*>& OptionPins) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	OptionPins.Empty();

	// If the select node is currently dealing with an enum
	if (IndexPinType.PinCategory == K2Schema->PC_Byte &&
		IndexPinType.PinSubCategory.IsEmpty() &&
		IndexPinType.PinSubCategoryObject != nullptr &&
		IndexPinType.PinSubCategoryObject->IsA(UEnum::StaticClass()))
	{
		for (UEdGraphPin* Pin : Pins)
		{
			if (EnumEntries.Contains(FName(*Pin->PinName)))
			{
				OptionPins.Add(Pin);
			}
		}
	}
	else
	{
		for (UEdGraphPin* Pin : Pins)
		{
			if (Pin->PinName.Left(6) == "Option")
			{
				OptionPins.Add(Pin);
			}
		}
	}
}

void UK2Node_Select::GetConditionalFunction(FName& FunctionName, UClass** FunctionClass)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	if (IndexPinType.PinCategory == K2Schema->PC_Boolean)
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_BoolBool);
	}
	else if (IndexPinType.PinCategory == K2Schema->PC_Byte)
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_ByteByte);
	}
	else if (IndexPinType.PinCategory == K2Schema->PC_Int)
	{
		FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, EqualEqual_IntInt);
	}

	*FunctionClass = UKismetMathLibrary::StaticClass();
}

void UK2Node_Select::GetPrintStringFunction(FName& FunctionName, UClass** FunctionClass)
{
	FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintWarning);
	*FunctionClass = UKismetSystemLibrary::StaticClass();
}

void UK2Node_Select::AddOptionPinToNode()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Increment the pin count
	NumOptionPins++;
	// We guarantee at least 2 options by default and since we just increased the count
	// to more than 2, we need to make sure we're now dealing with an index for selection
	// instead of the default boolean check
	if (IndexPinType.PinCategory == K2Schema->PC_Boolean)
	{
		IndexPinType.PinCategory = K2Schema->PC_Int;
		GetIndexPin()->BreakAllPinLinks();
	}
	// We will let the AllocateDefaultPins call handle the actual addition via ReconstructNode
	ReconstructNode();
}

void UK2Node_Select::RemoveOptionPinToNode()
{
	// Increment the pin count
	NumOptionPins--;
	// We will let the AllocateDefaultPins call handle the actual subtraction via ReconstructNode
	ReconstructNode();
}

void UK2Node_Select::SetEnum(UEnum* InEnum, bool bForceRegenerate)
{
	UEnum* PrevEnum = Enum;
	Enum = InEnum;

	OrphanedPinSaveMode = (Enum ? ESaveOrphanPinMode::SaveAll : ESaveOrphanPinMode::SaveNone);

	if (bForceRegenerate || (PrevEnum != Enum))
	{
		// regenerate enum name list
		EnumEntries.Reset();
		EnumEntryFriendlyNames.Reset();

		if (Enum)
		{
			for (int32 EnumIndex = 0; EnumIndex < Enum->NumEnums() - 1; ++EnumIndex)
			{
				bool const bShouldBeHidden = Enum->HasMetaData(TEXT("Hidden"), EnumIndex ) || Enum->HasMetaData(TEXT("Spacer"), EnumIndex );
				if (!bShouldBeHidden)
				{
					FString EnumValueName = Enum->GetNameStringByIndex(EnumIndex);
					FText EnumFriendlyName = Enum->GetDisplayNameTextByIndex(EnumIndex);
					EnumEntries.Add(FName(*EnumValueName));
					EnumEntryFriendlyNames.Add(EnumFriendlyName);
				}
			}
		}

		bReconstructNode = true;
	}
}

void UK2Node_Select::NodeConnectionListChanged()
{
	Super::NodeConnectionListChanged();

	if (bReconstructNode)
	{
		ReconstructNode();

		UBlueprint* Blueprint = GetBlueprint();
		if(!Blueprint->bBeingCompiled)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			Blueprint->BroadcastChanged();
		}
	}
}

bool UK2Node_Select::CanAddOptionPinToNode() const
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	if (IndexPinType.PinCategory == Schema->PC_Byte &&
		IndexPinType.PinSubCategoryObject.IsValid() &&
		IndexPinType.PinSubCategoryObject.Get()->IsA(UEnum::StaticClass()))
	{
		return false;
	}
	else if (IndexPinType.PinCategory == Schema->PC_Boolean)
	{
		return false;
	}

	return true;
}

bool UK2Node_Select::CanRemoveOptionPinToNode() const
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	if (IndexPinType.PinCategory == Schema->PC_Byte &&
		(NULL != Cast<UEnum>(IndexPinType.PinSubCategoryObject.Get())))
	{
		return false;
	}
	else if (IndexPinType.PinCategory == Schema->PC_Boolean)
	{
		return false;
	}

	return true;
}

void UK2Node_Select::ChangePinType(UEdGraphPin* Pin)
{
	PinTypeChanged(Pin);

	if (bReconstructNode)
	{
		ReconstructNode();
	}

	UBlueprint* Blueprint = GetBlueprint();
	if (!Blueprint->bBeingCompiled)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		Blueprint->BroadcastChanged();
	}
}

bool UK2Node_Select::CanChangePinType(UEdGraphPin* Pin) const
{
	// If this is the index pin, only allow type switching if nothing is linked to the pin
	if (Pin == GetIndexPin())
	{
		if (Pin->LinkedTo.Num() > 0)
		{
			return false;
		}
	}
	// Else it's one of the wildcard pins that share their type, so make sure none of them have a link
	else
	{
		if (GetReturnValuePin()->LinkedTo.Num() > 0)
		{
			return false;
		}
		else
		{
			TArray<UEdGraphPin*> OptionPins;
			GetOptionPins(OptionPins);
			for (UEdGraphPin* OptionPin : OptionPins)
			{
				if (OptionPin && OptionPin->LinkedTo.Num() > 0)
				{
					return false;
				}
			}
		}
	}
	return true;
}

void UK2Node_Select::PinTypeChanged(UEdGraphPin* Pin)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	if (Pin == GetIndexPin())
	{
		if (IndexPinType != Pin->PinType)
		{
			IndexPinType = Pin->PinType;

			// Since it is an interactive action we want the pins to go away regardless of the new type
			for (UEdGraphPin* PinToDiscard : Pins)
			{
				PinToDiscard->bSavePinIfOrphaned = false;
			}

			if (IndexPinType.PinSubCategoryObject.IsValid())
			{
				SetEnum(Cast<UEnum>(IndexPinType.PinSubCategoryObject.Get()));
			}
			else if (Enum)
			{
				SetEnum(nullptr);
			}

			// Remove all but two options if we switched to a bool index
			if (IndexPinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
			{
				NumOptionPins = 2;
			}

			if (!Schema->IsPinDefaultValid(Pin, Pin->DefaultValue, Pin->DefaultObject, Pin->DefaultTextValue).IsEmpty())
			{
				Schema->ResetPinToAutogeneratedDefaultValue(Pin);
			}

			bReconstructNode = true;
		}
	}
	else
	{
		// Set the return value
		UEdGraphPin* ReturnPin = GetReturnValuePin();
	
		// Recombine the sub pins back into the ReturnPin
		if (ReturnPin->SubPins.Num() > 0)
		{
			Schema->RecombinePin(ReturnPin->SubPins[0]);
		}
		ReturnPin->PinType = Pin->PinType;

		// Recombine all option pins back into their root
		TArray<UEdGraphPin*> OptionPins;
 		GetOptionPins(OptionPins);
		for (UEdGraphPin* OptionPin : OptionPins)
		{
			// Recombine the sub pins back into the OptionPin
			if (OptionPin->ParentPin == nullptr && OptionPin->SubPins.Num() > 0)
			{
				Schema->RecombinePin(OptionPin->SubPins[0]);
			}
		}

		// Get the options again and set them
		GetOptionPins(OptionPins);
		for (UEdGraphPin* OptionPin : OptionPins)
		{
			if (OptionPin->PinType != Pin->PinType ||
				OptionPin == Pin)
			{
				OptionPin->PinType = Pin->PinType;
			}

			if (!Schema->IsPinDefaultValid(OptionPin, OptionPin->DefaultValue, OptionPin->DefaultObject, OptionPin->DefaultTextValue).IsEmpty())
			{
				Schema->ResetPinToAutogeneratedDefaultValue(OptionPin);
			}
		}

		bReconstructNode = true;
	}
}

void UK2Node_Select::PostPasteNode()
{
	Super::PostPasteNode();

	if (UEdGraphPin* IndexPin = GetIndexPinUnchecked())
	{
		// This information will be cleared and we want to restore it
		FString OldDefaultValue = IndexPin->DefaultValue;

		// Corrects data in the index pin that is not valid after pasting
		PinTypeChanged(IndexPin);

		// Restore the default value of the index pin
		IndexPin->DefaultValue = OldDefaultValue;
	}
}

FSlateIcon UK2Node_Select::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Select_16x");
	return Icon;
}

bool UK2Node_Select::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	if (OtherPin && (OtherPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec))
	{
		OutReason = LOCTEXT("ExecConnectionDisallowed", "Cannot connect with Exec pin.").ToString();
		return true;
	}

	return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

FNodeHandlingFunctor* UK2Node_Select::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	static const FBoolConfigValueHelper UseSelectRef(TEXT("Kismet"), TEXT("bUseSelectRef"), GEngineIni);
	return UseSelectRef
		? static_cast<FNodeHandlingFunctor*>(new FKCHandler_SelectRef(CompilerContext))
		: static_cast<FNodeHandlingFunctor*>(new FKCHandler_Select(CompilerContext));
}

void UK2Node_Select::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

FText UK2Node_Select::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Utilities);
}

void UK2Node_Select::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	static const FBoolConfigValueHelper UseSelectRef(TEXT("Kismet"), TEXT("bUseSelectRef"), GEngineIni);
	if (!UseSelectRef)
	{
		return;
	}

	bool bSuccess = false;
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	for (UEdGraphPin* Pin : Pins)
	{
		const bool bValidAutoRefPin = Pin && !Schema->IsMetaPin(*Pin) && (Pin->Direction == EGPD_Input) && (!Pin->LinkedTo.Num() || (GetIndexPin() == Pin));
		if (!bValidAutoRefPin)
		{
			continue;
		}

		// copy defaults as default values can be reset when the pin is connected
		const FString DefaultValue = Pin->DefaultValue;
		UObject* DefaultObject = Pin->DefaultObject;
		const FText DefaultTextValue = Pin->DefaultTextValue;
		bool bMatchesDefaults = Pin->DoesDefaultValueMatchAutogenerated();

		UEdGraphPin* ValuePin = UK2Node_CallFunction::InnerHandleAutoCreateRef(this, Pin, CompilerContext, SourceGraph, true);
		if (ValuePin)
		{
			if (bMatchesDefaults)
			{
				// Use the latest code to set default value
				Schema->SetPinAutogeneratedDefaultValueBasedOnType(ValuePin);
			}
			else
			{
				ValuePin->DefaultValue = DefaultValue;
				ValuePin->DefaultObject = DefaultObject;
				ValuePin->DefaultTextValue = DefaultTextValue;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
