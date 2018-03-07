// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_Message.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LevelStreaming.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_TemporaryVariable.h"
#include "KismetCompilerMisc.h"
#include "KismetCompiler.h"
#include "Kismet/KismetArrayLibrary.h"

#define LOCTEXT_NAMESPACE "K2Node_Message"

UK2Node_Message::UK2Node_Message(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UK2Node_Message::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FText NodeName;
	if (UFunction* Function = GetTargetFunction())
	{
		if (!CachedNodeTitles.IsTitleCached(TitleType, this))
		{
			FText NodeNameText = UK2Node_CallFunction::GetUserFacingFunctionName(Function);
			if (TitleType == ENodeTitleType::MenuTitle)
			{
				// FText::Format() is slow, so we cache this to save on performance
				CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("ListTitle", "{0} (Message)"), NodeNameText), this);
			}
			else
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("NodeName"), NodeNameText);
				Args.Add(TEXT("OuterClassName"), FText::FromString(Function->GetOuterUClass()->GetName()));

				FText NodeTitle = FText::Format(NSLOCTEXT("K2Node", "CallInterfaceContext", "{NodeName}\nUsing Interface {OuterClassName}"), Args);
				// FText::Format() is slow, so we cache this to save on performance
				CachedNodeTitles.SetCachedTitle(TitleType, NodeTitle, this);
			}
		}
	}
	else
	{
		return NSLOCTEXT("K2Node", "InvalidMessageNode", "Invalid Message Node");
	}
	return CachedNodeTitles.GetCachedTitle(TitleType);
}

void UK2Node_Message::AllocateDefaultPins()
{
	UFunction* MessageNodeFunction = GetTargetFunction();
	// since we have branching logic in ExpandNode(), this has to be an impure
	// node with exec pins
	//
	// @TODO: make it so we can have impure message nodes using a custom 
	//        FNodeHandlingFunctor, instead of ExpandNode()
	if (MessageNodeFunction && MessageNodeFunction->HasAnyFunctionFlags(FUNC_BlueprintPure))
	{
		// Input - Execution Pin
		CreatePin(EGPD_Input,  UEdGraphSchema_K2::PC_Exec, FString(), nullptr, UEdGraphSchema_K2::PN_Execute);
		// Output - Execution Pin
		CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, FString(), nullptr, UEdGraphSchema_K2::PN_Then);
	}

	Super::AllocateDefaultPins();
}

UEdGraphPin* UK2Node_Message::CreateSelfPin(const UFunction* Function)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* SelfPin = CreatePin(EGPD_Input, K2Schema->PC_Object, FString(), UObject::StaticClass(), K2Schema->PN_Self);
	SelfPin->bDefaultValueIsIgnored = true;
	return SelfPin;
}

void UK2Node_Message::FixupSelfMemberContext()
{
	// Do nothing; the function either exists and works, or doesn't and doesn't
}

FName UK2Node_Message::GetCornerIcon() const
{
	return TEXT("Graph.Message.MessageIcon");
}

FNodeHandlingFunctor* UK2Node_Message::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FNodeHandlingFunctor(CompilerContext);
}

void UK2Node_Message::ExpandLevelStreamingHandlers(class FKismetCompilerContext& InCompilerContext, UEdGraph* InSourceGraph, UEdGraphPin* InStartingExecPin, UEdGraphPin* InMessageSelfPin, UK2Node_DynamicCast* InCastToInterfaceNode)
{
	const UEdGraphSchema_K2* Schema = InCompilerContext.GetSchema();
	
	/**
	 * Create intermediate nodes
	 */

	// Create a  GetLevelScriptActor CallFunction node, this will be used if the cast to ULevelStreaming was successful
	UK2Node_CallFunction* GetLevelScriptActorNode = InCompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, InSourceGraph);
	GetLevelScriptActorNode->SetFromFunction(ULevelStreaming::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(ULevelStreaming, GetLevelScriptActor)));
	GetLevelScriptActorNode->AllocateDefaultPins();

	UEdGraphPin* PinToCastToInterface = nullptr;
	{
		PinToCastToInterface = InMessageSelfPin;

		// Move all pin connections from the message self pin to the GetLevelScriptActorNode's self pin
		{
			InCompilerContext.MovePinLinksToIntermediate(*InMessageSelfPin, *Schema->FindSelfPin(*GetLevelScriptActorNode, EGPD_Input));

			// The last pin on the function node is the ALevelScriptActor
			UEdGraphPin* FuncResultPin = GetLevelScriptActorNode->Pins[GetLevelScriptActorNode->Pins.Num() - 1];
			ensure(!FuncResultPin->PinType.PinSubCategoryObject->IsA(ALevelScriptActor::StaticClass()));

			// We will want to cast the resulting level Blueprint to the appropriate interface
			PinToCastToInterface = FuncResultPin;
		}

		// Move all connections from the starting exec pin to the cast node
		InCompilerContext.MovePinLinksToIntermediate(*InStartingExecPin, *InCastToInterfaceNode->GetExecPin());
	}

	// Connect the Interface cast node to the generated pins
	{
		// The source pin of the Interface cast node connects to the temporary variable (storing either an ALevelScriptActor or another UObject)
		UEdGraphPin* CastToInterfaceSourceObjectPin = InCastToInterfaceNode->GetCastSourcePin();
		Schema->TryCreateConnection(PinToCastToInterface, CastToInterfaceSourceObjectPin);
	}
}

static bool IsLevelStreamingClass(UClass* InClass)
{
	if (InClass)
	{
		UClass* Parent = ULevelStreaming::StaticClass();
		return (Parent == InClass || InClass->IsChildOf(Parent));
	}
	return false;
}

void UK2Node_Message::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

	UEdGraphPin* ExecPin = Schema->FindExecutionPin(*this, EGPD_Input);
	const bool bExecPinConnected = ExecPin && (ExecPin->LinkedTo.Num() > 0);
	UEdGraphPin* ThenPin = Schema->FindExecutionPin(*this, EGPD_Output);
	const bool bThenPinConnected = ThenPin && (ThenPin->LinkedTo.Num() > 0);

	// Skip ourselves if our exec isn't wired up
	if (bExecPinConnected)
	{
		// Make sure our interface is valid
		if (FunctionReference.GetMemberParentClass(GetBlueprintClassFromNode()) == NULL)
		{
			CompilerContext.MessageLog.Error(*FString::Printf(*LOCTEXT("MessageNodeInvalid_Error", "Message node @@ has an invalid interface.").ToString()), this);
			return;
		}

		UFunction* MessageNodeFunction = GetTargetFunction();
		if (MessageNodeFunction == NULL)
		{
			//@TODO: Why do this here in the compiler, it's already done on AllocateDefaultPins() during on-load node reconstruction
			MessageNodeFunction = FMemberReference::FindRemappedField<UFunction>(FunctionReference.GetMemberParentClass(GetBlueprintClassFromNode()), FunctionReference.GetMemberName());
		}

		if (MessageNodeFunction == NULL)
		{
			CompilerContext.MessageLog.Error(*FString::Printf(*LOCTEXT("MessageNodeInvalidFunction_Error", "Unable to find function with name %s for Message node @@.").ToString(), *(FunctionReference.GetMemberName().ToString())), this);
			return;
		}

		// Check to make sure we have a target
		UEdGraphPin* MessageSelfPin = Schema->FindSelfPin(*this, EGPD_Input);
		if( !MessageSelfPin || MessageSelfPin->LinkedTo.Num() == 0 )
		{
			CompilerContext.MessageLog.Error(*FString::Printf(*LOCTEXT("MessageNodeSelfPin_Error", "Message node @@ must have a valid target or reference to self.").ToString()), this);
			return;
		}

		// First, create an internal cast-to-interface node
		UK2Node_DynamicCast* CastToInterfaceNode = CompilerContext.SpawnIntermediateNode<UK2Node_DynamicCast>(this, SourceGraph);
		CastToInterfaceNode->TargetType = MessageNodeFunction->GetOuterUClass()->GetAuthoritativeClass();
		CastToInterfaceNode->SetPurity(false);
		CastToInterfaceNode->AllocateDefaultPins();

		UEdGraphPin* CastToInterfaceResultPin = CastToInterfaceNode->GetCastResultPin();
		if( !CastToInterfaceResultPin )
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("InvalidInterfaceClass_Error", "Node @@ has an invalid target interface class").ToString(), this);
			return;
		}

		// If the message pin is linked and what it is linked to is not a ULevelStreaming pin reference but is a child of ULevelStreaming, 
		// then we need to leave the possibility that it could be a ULevelStreaming and will need to make an attempt at casting to one
		if (MessageSelfPin->LinkedTo.Num() > 0 && IsLevelStreamingClass(Cast<UClass>(MessageSelfPin->LinkedTo[0]->PinType.PinSubCategoryObject.Get())))
		{
			ExpandLevelStreamingHandlers(CompilerContext, SourceGraph, ExecPin, MessageSelfPin, CastToInterfaceNode);
		}
		else
		{
			// Move the connections on the Message node's self pin to the ULevelStreaming Cast node's source pin
			{
				CompilerContext.MovePinLinksToIntermediate(*MessageSelfPin, *CastToInterfaceNode->GetCastSourcePin());
				CastToInterfaceNode->NotifyPinConnectionListChanged(CastToInterfaceNode->GetCastSourcePin());
			}

			// Connect the incoming exec pin to the ULevelStreaming cast node's exec pin, which is the exec flow's entry into this
			if (ExecPin != nullptr)
			{
				// Wire up the connections
				CompilerContext.MovePinLinksToIntermediate(*ExecPin, *CastToInterfaceNode->GetExecPin());
			}
		}

		CastToInterfaceResultPin->PinType.PinSubCategoryObject = *CastToInterfaceNode->TargetType;

		// Next, create the function call node
		UK2Node_CallFunction* FunctionCallNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		FunctionCallNode->bIsInterfaceCall = true;
		FunctionCallNode->FunctionReference = FunctionReference;
		FunctionCallNode->AllocateDefaultPins();

		UEdGraphPin* CastToInterfaceValidPin = CastToInterfaceNode->GetValidCastPin();
		check(CastToInterfaceValidPin != nullptr);

		UEdGraphPin* LastOutCastFaildPin   = CastToInterfaceNode->GetInvalidCastPin();
		check(LastOutCastFaildPin != nullptr);
		UEdGraphPin* LastOutCastSuccessPin = CastToInterfaceValidPin;

		// Wire up the connections
		if (UEdGraphPin* CallFunctionExecPin = Schema->FindExecutionPin(*FunctionCallNode, EGPD_Input))
		{
			// Connect the CallFunctionExecPin to both Assignment nodes for the assignment of the UObject cast
			CallFunctionExecPin->MakeLinkTo(CastToInterfaceValidPin);
			LastOutCastSuccessPin = Schema->FindExecutionPin(*FunctionCallNode, EGPD_Output);
		}
		
		// Self pin
		UEdGraphPin* FunctionCallSelfPin = Schema->FindSelfPin(*FunctionCallNode, EGPD_Input);
		CastToInterfaceResultPin->MakeLinkTo(FunctionCallSelfPin);

		UFunction* ArrayClearFunction = UKismetArrayLibrary::StaticClass()->FindFunctionByName(FName(TEXT("Array_Clear")));
		check(ArrayClearFunction);

		bool const bIsPureMessageFunc = Super::IsNodePure();
		// Variable pins - Try to associate variable inputs to the message node with the variable inputs and outputs to the call function node
		for( int32 i = 0; i < Pins.Num(); i++ )
		{
			UEdGraphPin* CurrentPin = Pins[i];
			if( CurrentPin && (CurrentPin->PinType.PinCategory != Schema->PC_Exec) && (CurrentPin->PinName != Schema->PN_Self) )
			{
				// Try to find a match for the pin on the function call node
				UEdGraphPin* FunctionCallPin = FunctionCallNode->FindPin(CurrentPin->PinName);
				if( FunctionCallPin )
				{
					// Move pin links if the pin is connected...
					CompilerContext.MovePinLinksToIntermediate(*CurrentPin, *FunctionCallPin);

					// when cast fails all return values must be cleared.
					if (EEdGraphPinDirection::EGPD_Output == CurrentPin->Direction)
					{
						UEdGraphPin* VarOutPin = FunctionCallPin;
						if (bIsPureMessageFunc)
						{
							// since we cannot directly use the output from the
							// function call node (since it is pure, and invoking
							// it with a null target would cause an error), we 
							// have to use a temporary variable in it's place...
							UK2Node_TemporaryVariable* TempVar = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
							TempVar->VariableType = CurrentPin->PinType;
							TempVar->AllocateDefaultPins();

							VarOutPin = TempVar->GetVariablePin();
							// nodes using the function's outputs directly, now
							// use this TempVar node instead
							CompilerContext.MovePinLinksToIntermediate(*FunctionCallPin, *VarOutPin);

							// on a successful cast, the temp var is filled with
							// the function's value, on a failed cast, the var 
							// is filled with a default value (DefaultValueNode, 
							// below)... this is the node for the success case:
							UK2Node_AssignmentStatement* AssignTempVar = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
							AssignTempVar->AllocateDefaultPins();
							
							// assign the output from the pure function node to
							// the TempVar (either way, this message node is 
							// returning the TempVar's value, so on a successful 
							// cast we want it to have the function's result)
							UEdGraphPin* ValueInPin = AssignTempVar->GetValuePin();
							Schema->TryCreateConnection(FunctionCallPin, ValueInPin);
							AssignTempVar->PinConnectionListChanged(ValueInPin);

							UEdGraphPin* VarInPin = AssignTempVar->GetVariablePin();
							Schema->TryCreateConnection(VarOutPin, VarInPin);
							AssignTempVar->PinConnectionListChanged(VarInPin);
							// fold this AssignTempVar node into the cast's 
							// success execution chain
							Schema->TryCreateConnection(AssignTempVar->GetExecPin(), LastOutCastSuccessPin);
							LastOutCastSuccessPin = AssignTempVar->GetThenPin();
						}

						UK2Node* DefaultValueNode = nullptr;
						UEdGraphPin* DefaultValueThenPin = nullptr;
						if (CurrentPin->PinType.IsArray())
						{
							UK2Node_CallArrayFunction* ClearArray = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(this, SourceGraph);
							DefaultValueNode = ClearArray;
							ClearArray->SetFromFunction(ArrayClearFunction);
							ClearArray->AllocateDefaultPins();

							UEdGraphPin* ArrayPin = ClearArray->GetTargetArrayPin();
							check(ArrayPin);
							Schema->TryCreateConnection(ArrayPin, VarOutPin);
							ClearArray->PinConnectionListChanged(ArrayPin);

							DefaultValueThenPin = ClearArray->GetThenPin();
						} 
						else
						{
							UK2Node_AssignmentStatement* AssignDefaultValue = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
							DefaultValueNode = AssignDefaultValue;
							AssignDefaultValue->AllocateDefaultPins();

							Schema->TryCreateConnection(AssignDefaultValue->GetVariablePin(), VarOutPin);
							AssignDefaultValue->PinConnectionListChanged(AssignDefaultValue->GetVariablePin());
							Schema->SetPinAutogeneratedDefaultValueBasedOnType(AssignDefaultValue->GetValuePin());

							DefaultValueThenPin = AssignDefaultValue->GetThenPin();
						}

						UEdGraphPin* DefaultValueExecPin = DefaultValueNode->GetExecPin();
						check(DefaultValueExecPin);
						Schema->TryCreateConnection(DefaultValueExecPin, LastOutCastFaildPin);
						LastOutCastFaildPin = DefaultValueThenPin;
						check(LastOutCastFaildPin);
					}
				}
				else
				{
					UE_LOG(LogK2Compiler, Log, TEXT("%s"), *LOCTEXT("NoPinConnectionFound_Error", "Unable to find connection for pin!  Check AllocateDefaultPins() for consistency!").ToString());
				}
			}
		}

		if( bThenPinConnected )
		{
			check(LastOutCastFaildPin != nullptr);
			// Failure case for the cast runs straight through to the exit
			CompilerContext.CopyPinLinksToIntermediate(*ThenPin, *LastOutCastFaildPin);

			check(LastOutCastSuccessPin != nullptr);
			// Copy all links from the invalid cast case above to the call function node
			CompilerContext.MovePinLinksToIntermediate(*ThenPin, *LastOutCastSuccessPin);
		}
	}

	// Break all connections to the original node, so it will be pruned
	BreakAllNodeLinks();
}

#undef LOCTEXT_NAMESPACE
