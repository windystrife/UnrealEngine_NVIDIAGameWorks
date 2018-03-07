// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/InputAxisKeyDelegateBinding.h"
#include "InputVectorAxisDelegateBinding.generated.h"

class UInputComponent;

UCLASS()
class ENGINE_API UInputVectorAxisDelegateBinding : public UInputAxisKeyDelegateBinding
{
	GENERATED_UCLASS_BODY()

	//~ Begin UInputDelegateBinding Interface
	virtual void BindToInputComponent(UInputComponent* InputComponent) const override;
	//~ End UInputDelegateBinding Interface
};
