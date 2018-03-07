// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/InputDelegateBinding.h"
#include "InputAxisDelegateBinding.generated.h"

class UInputComponent;

USTRUCT()
struct ENGINE_API FBlueprintInputAxisDelegateBinding : public FBlueprintInputDelegateBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName InputAxisName;

	UPROPERTY()
	FName FunctionNameToBind;

	FBlueprintInputAxisDelegateBinding()
		: FBlueprintInputDelegateBinding()
		, InputAxisName(NAME_None)
		, FunctionNameToBind(NAME_None)
	{
	}
};

UCLASS()
class ENGINE_API UInputAxisDelegateBinding : public UInputDelegateBinding
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FBlueprintInputAxisDelegateBinding> InputAxisDelegateBindings;

	//~ Begin UInputDelegateBinding Interface
	virtual void BindToInputComponent(UInputComponent* InputComponent) const override;
	//~ End UInputDelegateBinding Interface
};
