// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/InputDelegateBinding.h"
#include "UObject/Class.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InputActionDelegateBinding.h"
#include "Engine/InputAxisDelegateBinding.h"
#include "Engine/InputKeyDelegateBinding.h"
#include "Engine/InputTouchDelegateBinding.h"
#include "Engine/InputAxisKeyDelegateBinding.h"
#include "Engine/InputVectorAxisDelegateBinding.h"

UInputDelegateBinding::UInputDelegateBinding(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UInputDelegateBinding::SupportsInputDelegate(const UClass* InClass)
{
	return Cast<UDynamicClass>(InClass) || Cast<UBlueprintGeneratedClass>(InClass);
}

void UInputDelegateBinding::BindInputDelegates(const UClass* InClass, UInputComponent* InputComponent)
{
	static UClass* InputBindingClasses[] = { 
												UInputActionDelegateBinding::StaticClass(), 
												UInputAxisDelegateBinding::StaticClass(), 
												UInputKeyDelegateBinding::StaticClass(),
												UInputTouchDelegateBinding::StaticClass(),
												UInputAxisKeyDelegateBinding::StaticClass(),
												UInputVectorAxisDelegateBinding::StaticClass(),
										   };

	if (InClass)
	{
		BindInputDelegates(InClass->GetSuperClass(), InputComponent);

		for (int32 Index = 0; Index < ARRAY_COUNT(InputBindingClasses); ++Index)
		{
			UInputDelegateBinding* BindingObject = CastChecked<UInputDelegateBinding>(
				UBlueprintGeneratedClass::GetDynamicBindingObject(InClass, InputBindingClasses[Index])
				, ECastCheckedType::NullAllowed);
			if (BindingObject)
			{
				BindingObject->BindToInputComponent(InputComponent);
			}
		}
	}
}
