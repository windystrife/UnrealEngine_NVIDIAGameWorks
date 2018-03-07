// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "Engine/InputDelegateBinding.h"
#include "InputAxisKeyDelegateBinding.generated.h"

class UInputComponent;

USTRUCT()
struct ENGINE_API FBlueprintInputAxisKeyDelegateBinding : public FBlueprintInputDelegateBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FKey AxisKey;

	UPROPERTY()
	FName FunctionNameToBind;

	FBlueprintInputAxisKeyDelegateBinding()
		: FBlueprintInputDelegateBinding()
		, FunctionNameToBind(NAME_None)
	{
	}
};

UCLASS()
class ENGINE_API UInputAxisKeyDelegateBinding : public UInputDelegateBinding
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FBlueprintInputAxisKeyDelegateBinding> InputAxisKeyDelegateBindings;

	//~ Begin UInputDelegateBinding Interface
	virtual void BindToInputComponent(UInputComponent* InputComponent) const override;
	//~ End UInputDelegateBinding Interface
};
