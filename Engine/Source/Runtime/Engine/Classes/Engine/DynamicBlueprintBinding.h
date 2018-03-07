// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "DynamicBlueprintBinding.generated.h"

UCLASS(abstract)
class ENGINE_API UDynamicBlueprintBinding
	: public UObject
{
	GENERATED_UCLASS_BODY()

	virtual void BindDynamicDelegates(UObject* InInstance) const { }
	virtual void UnbindDynamicDelegates(UObject* Instance) const { }
	virtual void UnbindDynamicDelegatesForProperty(UObject* InInstance, const UObjectProperty* InObjectProperty) const { }
};
