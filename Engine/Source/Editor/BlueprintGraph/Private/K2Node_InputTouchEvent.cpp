// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_InputTouchEvent.h"
#include "Engine/InputTouchDelegateBinding.h"

UK2Node_InputTouchEvent::UK2Node_InputTouchEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bConsumeInput = true;
	bOverrideParentBinding = true;
	bInternalEvent = true;
}

UClass* UK2Node_InputTouchEvent::GetDynamicBindingClass() const
{
	return UInputTouchDelegateBinding::StaticClass();
}

void UK2Node_InputTouchEvent::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	UInputTouchDelegateBinding* InputTouchBindingObject = CastChecked<UInputTouchDelegateBinding>(BindingObject);

	FBlueprintInputTouchDelegateBinding Binding;
	Binding.InputKeyEvent = InputKeyEvent;
	Binding.bConsumeInput = bConsumeInput;
	Binding.bExecuteWhenPaused = bExecuteWhenPaused;
	Binding.bOverrideParentBinding = bOverrideParentBinding;
	Binding.FunctionNameToBind = CustomFunctionName;

	InputTouchBindingObject->InputTouchDelegateBindings.Add(Binding);
}
