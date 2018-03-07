// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "AbilitySystemInterface.generated.h"

class UAbilitySystemComponent;

/** Interface for actors that expose access to an ability system component */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UAbilitySystemInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class GAMEPLAYABILITIES_API IAbilitySystemInterface
{
	GENERATED_IINTERFACE_BODY()

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const = 0;
};
