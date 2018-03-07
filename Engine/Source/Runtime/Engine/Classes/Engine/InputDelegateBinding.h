// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DynamicBlueprintBinding.h"
#include "InputDelegateBinding.generated.h"

class UInputComponent;

USTRUCT()
struct ENGINE_API FBlueprintInputDelegateBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint32 bConsumeInput:1;

	UPROPERTY()
	uint32 bExecuteWhenPaused:1;

	UPROPERTY()
	uint32 bOverrideParentBinding:1;

	FBlueprintInputDelegateBinding()
		: bConsumeInput(true)
		, bExecuteWhenPaused(false)
		, bOverrideParentBinding(true)
	{
	}
};

UCLASS(abstract)
class ENGINE_API UInputDelegateBinding : public UDynamicBlueprintBinding
{
	GENERATED_UCLASS_BODY()

	virtual void BindToInputComponent(UInputComponent* InputComponent) const { };
	static bool SupportsInputDelegate(const UClass* InClass);
	static void BindInputDelegates(const UClass* InClass, UInputComponent* InputComponent);
};
