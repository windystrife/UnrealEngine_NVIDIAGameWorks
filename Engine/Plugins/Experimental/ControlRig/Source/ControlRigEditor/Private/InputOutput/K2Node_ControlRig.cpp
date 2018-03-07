// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_ControlRig.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "ControlRig.h"
#include "Textures/SlateIcon.h"
#include "ControlRigComponent.h"
#include "K2Node_ControlRigEvaluator.h"
#include "ControlRigField.h"

#define LOCTEXT_NAMESPACE "K2Node_ControlRig"

UK2Node_ControlRig::UK2Node_ControlRig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_ControlRig::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

FText UK2Node_ControlRig::GetMenuCategory() const
{
	return LOCTEXT("AnimationMenuCategoryName", "Animation");
}

TArray<TSharedRef<IControlRigField>> UK2Node_ControlRig::GetInputVariableInfo() const
{
	TArray<FName> DisabledPins;
	return GetInputVariableInfo(DisabledPins);
}

TArray<TSharedRef<IControlRigField>> UK2Node_ControlRig::GetInputVariableInfo(const TArray<FName>& DisabledPins) const
{
	TArray<TSharedRef<IControlRigField>> Fields;
	GetInputFields(DisabledPins, Fields);
	return Fields;
}

void UK2Node_ControlRig::GetInputParameterPins(const TArray<FName>& DisabledPins, TArray<UEdGraphPin*>& OutPins, TArray<TSharedRef<IControlRigField>>& OutFieldInfo) const
{
	TArray<TSharedRef<IControlRigField>> VariableInfos = GetInputVariableInfo(DisabledPins);

	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin)
		{
			// check each property
			for (const TSharedRef<IControlRigField>& VariableInfo : VariableInfos)
			{
				if (Pin->Direction == GetInputDirection() && Pin->PinName == VariableInfo->GetPinString())
				{
					OutPins.Add(Pin);
					OutFieldInfo.Add(VariableInfo);
					break;
				}
			}
		}
	}
}

TArray<TSharedRef<IControlRigField>> UK2Node_ControlRig::GetOutputVariableInfo() const
{
	TArray<FName> DisabledPins;
	return GetOutputVariableInfo(DisabledPins);
}

TArray<TSharedRef<IControlRigField>> UK2Node_ControlRig::GetOutputVariableInfo(const TArray<FName>& DisabledPins) const
{
	TArray<TSharedRef<IControlRigField>> Fields;
	GetOutputFields(DisabledPins, Fields);
	return Fields;
}

void UK2Node_ControlRig::GetOutputParameterPins(const TArray<FName>& DisabledPins, TArray<UEdGraphPin*>& OutPins, TArray<TSharedRef<IControlRigField>>& OutFieldInfo) const
{
	TArray<TSharedRef<IControlRigField>> VariableInfos = GetOutputVariableInfo(DisabledPins);

	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin)
		{
			// check each property
			for (const TSharedRef<IControlRigField>& VariableInfo : VariableInfos)
			{
				if (Pin->Direction == GetOutputDirection() && Pin->PinName == VariableInfo->GetPinString())
				{
					OutPins.Add(Pin);
					OutFieldInfo.Add(VariableInfo);
					break;
				}
			}
		}
	}
}

UClass* UK2Node_ControlRig::GetControlRigClassFromBlueprint(const UBlueprint* Blueprint)
{
	if (Blueprint->SkeletonGeneratedClass && Blueprint->SkeletonGeneratedClass->IsChildOf(UControlRig::StaticClass()))
	{
		return Blueprint->SkeletonGeneratedClass;
	}
	else if (Blueprint->GeneratedClass)
	{
		if (Blueprint->GeneratedClass->IsChildOf(UControlRig::StaticClass()))
		{
			return Blueprint->GeneratedClass;
		}
		else if (Blueprint->GeneratedClass->IsChildOf(UControlRigComponent::StaticClass()))
		{
			if (const UControlRigComponent* Component = Cast<const UControlRigComponent>(Blueprint->GeneratedClass->GetDefaultObject(false)))
			{
				if (const UControlRig* ControlRig = Component->ControlRig)
				{
					return ControlRig->GetClass();
				}
			}
		}
	}

	return nullptr;
}

UClass* UK2Node_ControlRig::GetControlRigClass() const
{
	const UClass* FoundClass = GetControlRigClassImpl();
	if (FoundClass)
	{
		ControlRigClass = FSoftClassPath(FoundClass);
	}

	if (ControlRigClass.IsValid())
	{
		UClass* LoadedClass = ControlRigClass.ResolveClass();
		if (LoadedClass == nullptr)
		{
			return ControlRigClass.TryLoadClass<UControlRig>();
		}
		return LoadedClass;
	}

	return nullptr;
}

FSlateIcon UK2Node_ControlRig::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = GetNodeTitleColor();
	static FSlateIcon Icon("EditorStyle", "Kismet.AllClasses.FunctionIcon");
	return Icon;
}

FNodeHandlingFunctor* UK2Node_ControlRig::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_Passthru(CompilerContext);
}

bool UK2Node_ControlRig::HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const
{
	if (OptionalOutput)
	{
		OptionalOutput->Add(GetControlRigClass());
	}

	return true;
}

void UK2Node_ControlRig::HandleVariableRenamed(UBlueprint* InBlueprint, UClass* InVariableClass, UEdGraph* InGraph, const FName& InOldVarName, const FName& InNewVarName)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	Modify();

	// first rename any disabled inputs/outputs
	for (FName& DisabledInput : DisabledInputs)
	{
		if (DisabledInput == InOldVarName)
		{
			DisabledInput = InNewVarName;
		}
	}

	for (FName& DisabledOutput : DisabledOutputs)
	{
		if (DisabledOutput == InOldVarName)
		{
			DisabledOutput = InNewVarName;
		}
	}

	RenameUserDefinedPin(InOldVarName.ToString(), InNewVarName.ToString());
}

bool UK2Node_ControlRig::ReferencesVariable(const FName& InVarName, const UStruct* InScope) const
{
	TArray<TSharedRef<IControlRigField>> InputInfo = GetInputVariableInfo();
	for (const TSharedRef<IControlRigField>& Input : InputInfo)
	{
		if (Input->GetName() == InVarName)
		{
			return true;
		}
	}

	TArray<TSharedRef<IControlRigField>> OutputInfo = GetOutputVariableInfo();
	for (const TSharedRef<IControlRigField>& Output : OutputInfo)
	{
		if (Output->GetName() == InVarName)
		{
			return true;
		}
	}

	return false;
}

bool UK2Node_ControlRig::HasInputPin(const FName& PinName) const
{
	TArray<TSharedRef<IControlRigField>> VariableInfos = GetInputVariableInfo();
	for (const TSharedRef<IControlRigField>& VariableInfo : VariableInfos)
	{
		if (VariableInfo->GetName() == PinName)
		{
			return true;
		}
	}

	return false;
}

bool UK2Node_ControlRig::HasOutputPin(const FName& PinName) const
{
	TArray<TSharedRef<IControlRigField>> VariableInfos = GetOutputVariableInfo();
	for (const TSharedRef<IControlRigField>& VariableInfo : VariableInfos)
	{
		if (VariableInfo->GetName() == PinName)
		{
			return true;
		}
	}

	return false;
}

void UK2Node_ControlRig::SetInputPinDisabled(const FName& PinName, bool bDisabled)
{
	if (bDisabled)
	{
		DisabledInputs.AddUnique(PinName);
	}
	else
	{
		DisabledInputs.Remove(PinName);
	}
}

void UK2Node_ControlRig::SetOutputPinDisabled(const FName& PinName, bool bDisabled)
{
	if (bDisabled)
	{
		DisabledOutputs.AddUnique(PinName);
	}
	else
	{
		DisabledOutputs.Remove(PinName);
	}
}

TSharedPtr<IControlRigField> UK2Node_ControlRig::CreateControlRigField(UField* Field) const
{
	if (UProperty* Property = Cast<UProperty>(Field))
	{
		return MakeShareable(new FControlRigProperty(Property));
	}

	return nullptr;
}

TSharedPtr<IControlRigField> UK2Node_ControlRig::CreateLabeledControlRigField(UField* Field, const FString& Label, bool bIsInputContext) const
{
	if (UFunction* Function = Cast<UFunction>(Field))
	{
		// check the function we are using follows a named parameter signature
		// i.e. Func(Name, Param) or RetVal Func(Name)

		const bool bHasInputMetaData = Function->HasMetaData(UControlRig::AnimationInputMetaName);
		const bool bHasOutputMetaData = Function->HasMetaData(UControlRig::AnimationOutputMetaName);

		const bool bInternalNodeContext = (GetInputDirection() == EGPD_Output || GetOutputDirection() == EGPD_Input);

		// Account for reversed functionality of inputs/outputs when we are ControlRig-internal
		const bool bIsSetter = ((bHasInputMetaData && !bInternalNodeContext && bIsInputContext) || (bHasOutputMetaData && bInternalNodeContext && !bIsInputContext));
		const bool bIsGetter = ((bHasOutputMetaData && !bInternalNodeContext && !bIsInputContext) || (bHasInputMetaData && bInternalNodeContext && bIsInputContext));

		if (bIsSetter && bIsGetter)
		{
			// We don't support both a setter and getter at the same time in a particular context
			return nullptr;
		}

		bool bHasNameProperty = false;
		UProperty* ReturnValueProperty = nullptr;
		UProperty* NameProperty = nullptr;
		UProperty* ValueProperty = nullptr;
		int32 PropertyCount = 0;
		for (TFieldIterator<UProperty> PropIt(Function); PropIt; ++PropIt)
		{
			UProperty* Property = *PropIt;
			if (Property)
			{
				if (bIsSetter)
				{
					// check for value parameter
					if (ValueProperty == nullptr && NameProperty != nullptr && !Property->HasAnyPropertyFlags(CPF_ReturnParm))
					{
						ValueProperty = Property;
					}

					// check for name parameter
					if (NameProperty == nullptr && Property->IsA(UNameProperty::StaticClass()) && !Property->HasAnyPropertyFlags(CPF_ReturnParm))
					{
						NameProperty = Property;
					}
				}

				if (bIsGetter)
				{
					// check for return value
					if (ReturnValueProperty == nullptr && Property->HasAnyPropertyFlags(CPF_ReturnParm))
					{
						ReturnValueProperty = Property;
					}

					// check for name parameter
					if (NameProperty == nullptr && Property->IsA(UNameProperty::StaticClass()) && !Property->HasAnyPropertyFlags(CPF_ReturnParm))
					{
						NameProperty = Property;
					}
				}

				PropertyCount++;
			}
		}

		// Check if the signature is satisfactory
		bool bValidSetter = bIsSetter && PropertyCount == 2 && ValueProperty != nullptr && NameProperty != nullptr;
		bool bValidGetter = bIsGetter && PropertyCount == 2 && ReturnValueProperty != nullptr && NameProperty != nullptr;

		// Now check our labeled inputs/outputs to see if we want to use this function
		if (bValidSetter)
		{
			return MakeShareable(new FControlRigFunction_Name(*Label, Function, NameProperty, ValueProperty));
		}
		else if (bValidGetter)
		{
			return MakeShareable(new FControlRigFunction_Name(*Label, Function, NameProperty, ReturnValueProperty));
		}
	}
	
	return nullptr;
}

bool UK2Node_ControlRig::CanCreateLabeledControlRigField(UField* Field, bool bIsInputContext) const
{
	return CreateLabeledControlRigField(Field, TEXT("Template"), bIsInputContext).IsValid();
}

void UK2Node_ControlRig::GetInputFields(const TArray<FName>& DisabledPins, TArray<TSharedRef<IControlRigField>>& OutFields) const
{
	OutFields.Reset();

	if (UClass* MyControlRigClass = GetControlRigClass())
	{
		for (TFieldIterator<UProperty> PropertyIt(MyControlRigClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			if (PropertyIt->HasMetaData(UControlRig::AnimationInputMetaName))
			{
				TSharedPtr<IControlRigField> ControlRigField = CreateControlRigField(*PropertyIt);
				if(ControlRigField.IsValid() && !DisabledPins.Contains(ControlRigField->GetName()))
				{
					OutFields.Add(ControlRigField.ToSharedRef());
				}
			}
		}

		TArray<UField*> LabeledFields;
		GetPotentialLabeledInputFields(LabeledFields);

		for (UField* LabeledField : LabeledFields)
		{
			for (const FUserLabeledField& LabeledInput : LabeledInputs)
			{
				if (LabeledInput.FieldName == LabeledField->GetFName())
				{
					TSharedPtr<IControlRigField> ControlRigField = CreateLabeledControlRigField(LabeledField, LabeledInput.Label, true);
					if (ControlRigField.IsValid())
					{
						OutFields.Add(ControlRigField.ToSharedRef());
					}
				}
			}
		}
	}
}

void UK2Node_ControlRig::GetOutputFields(const TArray<FName>& DisabledPins, TArray<TSharedRef<IControlRigField>>& OutFields) const
{
	OutFields.Reset();

	if (UClass* MyControlRigClass = GetControlRigClass())
	{
		for (TFieldIterator<UProperty> PropertyIt(MyControlRigClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			if (PropertyIt->HasMetaData(UControlRig::AnimationOutputMetaName))
			{
				TSharedPtr<IControlRigField> ControlRigField = CreateControlRigField(*PropertyIt);
				if (ControlRigField.IsValid() && !DisabledPins.Contains(ControlRigField->GetName()))
				{
					OutFields.Add(ControlRigField.ToSharedRef());
				}
			}
		}

		TArray<UField*> LabeledFields;
		GetPotentialLabeledOutputFields(LabeledFields);

		for (UField* LabeledField : LabeledFields)
		{
			for (const FUserLabeledField& LabeledOutput : LabeledOutputs)
			{
				if (LabeledOutput.FieldName == LabeledField->GetFName())
				{
					TSharedPtr<IControlRigField> ControlRigField = CreateLabeledControlRigField(LabeledField, LabeledOutput.Label, false);
					if (ControlRigField.IsValid())
					{
						OutFields.Add(ControlRigField.ToSharedRef());
					}
				}
			}
		}
	}
}

void UK2Node_ControlRig::GetPotentialLabeledInputFields(TArray<UField*>& OutFields) const
{
	if (UClass* MyControlRigClass = GetControlRigClass())
	{
		for (TFieldIterator<UFunction> FunctionIt(MyControlRigClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
		{
			if (FunctionIt->HasMetaData(UControlRig::AnimationInputMetaName))
			{
				if (CanCreateLabeledControlRigField(*FunctionIt, true))
				{
					OutFields.Add(*FunctionIt);
				}
			}
		}
	}
}

void UK2Node_ControlRig::GetPotentialLabeledOutputFields(TArray<UField *>& OutFields) const
{
	if (UClass* MyControlRigClass = GetControlRigClass())
	{
		for (TFieldIterator<UFunction> FunctionIt(MyControlRigClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
		{
			if (FunctionIt->HasMetaData(UControlRig::AnimationOutputMetaName))
			{
				if (CanCreateLabeledControlRigField(*FunctionIt, false))
				{
					OutFields.Add(*FunctionIt);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
