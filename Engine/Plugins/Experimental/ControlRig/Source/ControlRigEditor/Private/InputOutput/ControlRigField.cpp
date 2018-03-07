// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigField.h"
#include "KismetCompiler.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableSet.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"

#define LOCTEXT_NAMESPACE "ControlRigField"

FControlRigProperty::FControlRigProperty(UProperty* InProperty)
	: Property(InProperty)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	K2Schema->ConvertPropertyToPinType(Property, PinType);
}

void FControlRigProperty::ExpandPin(UClass* Class, FKismetCompilerContext& InCompilerContext, UEdGraph* InSourceGraph, UEdGraphNode* InSourceNode, UEdGraphPin* InPin, UEdGraphPin* InSelfPin, bool bMoveExecPins, UEdGraphPin*& InOutExecPin) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	switch (InPin->Direction)
	{
	case EGPD_Input:
		{
			bool bUsedAccessor = false;
			FName FunctionName = *(FString(TEXT("Set") + InPin->GetName()));
			UFunction* AccessorFunction = Class->FindFunctionByName(FunctionName);
			if (AccessorFunction)
			{
				UK2Node_CallFunction* CallAccessorFunction = InCompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(InSourceNode, InSourceGraph);
				if (InSelfPin)
				{
					CallAccessorFunction->FunctionReference.SetExternalMember(AccessorFunction->GetFName(), Class);
				}
				else
				{
					CallAccessorFunction->FunctionReference.SetSelfMember(AccessorFunction->GetFName());
				}
				
				CallAccessorFunction->AllocateDefaultPins();

				FString VariableName = FString(TEXT("In")) + InPin->GetName();

				UEdGraphPin* AccessorVariablePin = CallAccessorFunction->FindPin(VariableName, EGPD_Input);
				UEdGraphPin* AccessorSelfPin = CallAccessorFunction->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
				UEdGraphPin* AccessorExecPin = CallAccessorFunction->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
				UEdGraphPin* AccessorThenPin = CallAccessorFunction->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
				if (AccessorVariablePin && K2Schema->ArePinTypesCompatible(AccessorVariablePin->PinType, InPin->PinType) && AccessorSelfPin && AccessorExecPin && AccessorThenPin)
				{
					// hook target up
					if (InSelfPin)
					{
						InSelfPin->MakeLinkTo(AccessorSelfPin);
					}

					if (InPin->LinkedTo.Num() > 0)
					{
						// Copy the connection
						InCompilerContext.MovePinLinksToIntermediate(*InPin, *AccessorVariablePin);
					}
					else
					{
						// Copy literal
						AccessorVariablePin->DefaultObject = InPin->DefaultObject;
						AccessorVariablePin->DefaultValue = InPin->DefaultValue;
					}

					// hook exec path
					if (bMoveExecPins)
					{
						InCompilerContext.MovePinLinksToIntermediate(*InOutExecPin, *AccessorExecPin);
						InOutExecPin = AccessorThenPin;
					}
					else
					{
						AccessorExecPin->MakeLinkTo(InOutExecPin);
						InOutExecPin = AccessorThenPin;
					}

					bUsedAccessor = true;
				}
			}

			if (!bUsedAccessor)
			{
				UK2Node_VariableSet* VariableSet = InCompilerContext.SpawnIntermediateNode<UK2Node_VariableSet>(InSourceNode, InSourceGraph);
				if (InSelfPin)
				{
					VariableSet->VariableReference.SetExternalMember(*InPin->GetName(), Class);
				}
				else
				{
					VariableSet->VariableReference.SetSelfMember(*InPin->GetName());
				}
				VariableSet->AllocateDefaultPins();

				UEdGraphPin* VariableSetVariablePin = VariableSet->FindPinChecked(InPin->GetName(), EGPD_Input);
				UEdGraphPin* VariableSetSelfPin = VariableSet->FindPinChecked(UEdGraphSchema_K2::PN_Self, EGPD_Input);
				UEdGraphPin* VariableSetExecPin = VariableSet->FindPinChecked(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
				UEdGraphPin* VariableSetThenPin = VariableSet->FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output);

				// hook target up
				if (InSelfPin)
				{
					InSelfPin->MakeLinkTo(VariableSetSelfPin);
				}

				if (InPin->LinkedTo.Num() > 0)
				{
					// Copy the connection
					InCompilerContext.MovePinLinksToIntermediate(*InPin, *VariableSetVariablePin);
				}
				else
				{
					// Copy literal
					VariableSetVariablePin->DefaultObject = InPin->DefaultObject;
					VariableSetVariablePin->DefaultValue = InPin->DefaultValue;
				}

				// hook exec path
				if (bMoveExecPins)
				{
					InCompilerContext.MovePinLinksToIntermediate(*InOutExecPin, *VariableSetExecPin);
					InOutExecPin = VariableSetThenPin;
				}
				else
				{
					VariableSetExecPin->MakeLinkTo(InOutExecPin);
					InOutExecPin = VariableSetThenPin;
				}

				// expand variable set nodes as then wont have been caught in the early expansion pass
				VariableSet->ExpandNode(InCompilerContext, InSourceGraph);
			}
		}
		break;
	case EGPD_Output:
		{
			if (InPin->LinkedTo.Num() > 0)
			{
				UK2Node_VariableGet* VariableGet = InCompilerContext.SpawnIntermediateNode<UK2Node_VariableGet>(InSourceNode, InSourceGraph);
				if (InSelfPin)
				{
					VariableGet->VariableReference.SetExternalMember(*InPin->GetName(), Class);
				}
				else
				{
					VariableGet->VariableReference.SetSelfMember(*InPin->GetName());
				}
				
				VariableGet->AllocateDefaultPins();

				UEdGraphPin* VariableGetVariablePin = VariableGet->FindPinChecked(InPin->GetName(), EGPD_Output);
				UEdGraphPin* VariableGetSelfPin = VariableGet->FindPinChecked(UEdGraphSchema_K2::PN_Self, EGPD_Input);

				// hook self up
				if (InSelfPin)
				{
					InSelfPin->MakeLinkTo(VariableGetSelfPin);
				}

				// Copy the connection
				InCompilerContext.MovePinLinksToIntermediate(*InPin, *VariableGetVariablePin);

				// expand variable set nodes as then wont have been caught in the early expansion pass
				VariableGet->ExpandNode(InCompilerContext, InSourceGraph);
			}
		}
		break;
	}
}

FControlRigFunction_Name::FControlRigFunction_Name(const FName& InLabel, UFunction* InFunction, UProperty* InNameProperty, UProperty* InValueProperty)
	: Label(InLabel)
	, Function(InFunction)
	, NameProperty(InNameProperty)
	, ValueProperty(InValueProperty)
{
	FString TrimmedFunctionName = FName::NameToDisplayString(InFunction->GetName(), false);
	if (TrimmedFunctionName.StartsWith(TEXT("Set")) || TrimmedFunctionName.StartsWith(TEXT("Get")))
	{
		TrimmedFunctionName = TrimmedFunctionName.RightChop(3);
	}

	TrimmedFunctionName.TrimStartInline();

	FFormatNamedArguments NamedArguments;
	NamedArguments.Add(TEXT("UserLabel"), FText::FromName(InLabel));
	NamedArguments.Add(TEXT("TrimmedFunctionName"), FText::FromString(TrimmedFunctionName));

	DisplayText = FText::Format(LOCTEXT("LabeledValueFormat", "{UserLabel} {TrimmedFunctionName}"), NamedArguments);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	K2Schema->ConvertPropertyToPinType(ValueProperty, PinType);
}

void FControlRigFunction_Name::ExpandPin(UClass* Class, FKismetCompilerContext& InCompilerContext, UEdGraph* InSourceGraph, UEdGraphNode* InSourceNode, UEdGraphPin* InPin, UEdGraphPin* InSelfPin, bool bMoveExecPins, UEdGraphPin*& InOutExecPin) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	switch (InPin->Direction)
	{
	case EGPD_Input:
		{
			// Call the 'setter' function
			UK2Node_CallFunction* CallSetterFunction = InCompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(InSourceNode, InSourceGraph);
			if (InSelfPin)
			{
				CallSetterFunction->FunctionReference.SetExternalMember(Function->GetFName(), Class);
			}
			else
			{
				CallSetterFunction->FunctionReference.SetSelfMember(Function->GetFName());
			}

			CallSetterFunction->AllocateDefaultPins();

			UEdGraphPin* SetterNamePin = CallSetterFunction->FindPin(NameProperty->GetName(), EGPD_Input);
			UEdGraphPin* SetterVariablePin = CallSetterFunction->FindPin(ValueProperty->GetName(), EGPD_Input);
			UEdGraphPin* SetterSelfPin = CallSetterFunction->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
			UEdGraphPin* SetterExecPin = CallSetterFunction->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
			UEdGraphPin* SetterThenPin = CallSetterFunction->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
			if (SetterNamePin && SetterVariablePin && K2Schema->ArePinTypesCompatible(SetterVariablePin->PinType, InPin->PinType) && SetterSelfPin && SetterExecPin && SetterThenPin)
			{
				// Set name default
				SetterNamePin->DefaultValue = Label.ToString();

				// hook target up
				if (InSelfPin)
				{
					InSelfPin->MakeLinkTo(SetterSelfPin);
				}

				if (InPin->LinkedTo.Num() > 0)
				{
					// Copy the connection
					InCompilerContext.MovePinLinksToIntermediate(*InPin, *SetterVariablePin);
				}
				else
				{
					// Copy literal
					SetterVariablePin->DefaultObject = InPin->DefaultObject;
					SetterVariablePin->DefaultValue = InPin->DefaultValue;
				}

				// hook exec path
				SetterExecPin->MakeLinkTo(InOutExecPin);
				InOutExecPin = SetterThenPin;
			}
		}
		break;
	case EGPD_Output:
		{
			// Call the 'getter' function
			UK2Node_CallFunction* CallGetterFunction = InCompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(InSourceNode, InSourceGraph);
			if (InSelfPin)
			{
				CallGetterFunction->FunctionReference.SetExternalMember(Function->GetFName(), Class);
			}
			else
			{
				CallGetterFunction->FunctionReference.SetSelfMember(Function->GetFName());
			}

			CallGetterFunction->AllocateDefaultPins();

			UEdGraphPin* GetterNamePin = CallGetterFunction->FindPin(NameProperty->GetName(), EGPD_Input);
			UEdGraphPin* GetterVariablePin = CallGetterFunction->FindPin(ValueProperty->GetName(), EGPD_Output);
			UEdGraphPin* GetterSelfPin = CallGetterFunction->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
			if (GetterNamePin && GetterVariablePin && K2Schema->ArePinTypesCompatible(GetterVariablePin->PinType, InPin->PinType) && GetterSelfPin)
			{
				// Set name default
				GetterNamePin->DefaultValue = Label.ToString();

				// hook self up
				if (InSelfPin)
				{
					InSelfPin->MakeLinkTo(GetterSelfPin);
				}

				// Copy the connection
				InCompilerContext.MovePinLinksToIntermediate(*InPin, *GetterVariablePin);
			}
		}
		break;
	}
}

#undef LOCTEXT_NAMESPACE