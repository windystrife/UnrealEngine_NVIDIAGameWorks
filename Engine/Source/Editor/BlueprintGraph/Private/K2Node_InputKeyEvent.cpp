// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_InputKeyEvent.h"
#include "Engine/InputKeyDelegateBinding.h"

UK2Node_InputKeyEvent::UK2Node_InputKeyEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bConsumeInput = true;
	bOverrideParentBinding = true;
	bInternalEvent = true;
}

UClass* UK2Node_InputKeyEvent::GetDynamicBindingClass() const
{
	return UInputKeyDelegateBinding::StaticClass();
}

void UK2Node_InputKeyEvent::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	UInputKeyDelegateBinding* InputKeyBindingObject = CastChecked<UInputKeyDelegateBinding>(BindingObject);

	FBlueprintInputKeyDelegateBinding Binding;
	Binding.InputChord = InputChord;
	Binding.InputKeyEvent = InputKeyEvent;
	Binding.bConsumeInput = bConsumeInput;
	Binding.bExecuteWhenPaused = bExecuteWhenPaused;
	Binding.bOverrideParentBinding = bOverrideParentBinding;
	Binding.FunctionNameToBind = CustomFunctionName;

	InputKeyBindingObject->InputKeyDelegateBindings.Add(Binding);
}
