// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"
#include "Engine/DynamicBlueprintBinding.h"
#include "ComponentDelegateBinding.generated.h"

/** Entry for a delegate to assign after a blueprint has been instanced */
USTRUCT()
struct ENGINE_API FBlueprintComponentDelegateBinding
{
	GENERATED_USTRUCT_BODY()

	/** Name of component property that contains delegate we want to assign to. */
	UPROPERTY()
	FName ComponentPropertyName;

	/** Name of property on the component that we want to assign to. */
	UPROPERTY()
	FName DelegatePropertyName;

	/** Name of function that we want to bind to the delegate. */
	UPROPERTY()
	FName FunctionNameToBind;

	FBlueprintComponentDelegateBinding()
		: ComponentPropertyName(NAME_None)
		, DelegatePropertyName(NAME_None)
		, FunctionNameToBind(NAME_None)
	{ }
};


UCLASS()
class ENGINE_API UComponentDelegateBinding
	: public UDynamicBlueprintBinding
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FBlueprintComponentDelegateBinding> ComponentDelegateBindings;

	//~ Begin DynamicBlueprintBinding Interface
	virtual void BindDynamicDelegates(UObject* InInstance) const override;
	virtual void UnbindDynamicDelegates(UObject* InInstance) const override;
	virtual void UnbindDynamicDelegatesForProperty(UObject* InInstance, const UObjectProperty* InObjectProperty) const override;
	//~ End DynamicBlueprintBinding Interface

private:
	// Utility method used to find the target delegate given an instance and a binding descriptor
	static FMulticastScriptDelegate* FindComponentTargetDelegate(const UObject* InInstance, const FBlueprintComponentDelegateBinding& InBinding, const UObjectProperty* InObjectProperty = nullptr);
};
