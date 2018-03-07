// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayCueInterface.h"
#include "GameFramework/DefaultPawn.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemTestPawn.generated.h"

class UAbilitySystemComponent;

UCLASS(Blueprintable, BlueprintType, notplaceable)
class GAMEPLAYABILITIES_API AAbilitySystemTestPawn : public ADefaultPawn, public IGameplayCueInterface, public IAbilitySystemInterface
{
	GENERATED_UCLASS_BODY()

	virtual void PostInitializeComponents() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
private:
	/** DefaultPawn collision component */
	UPROPERTY(Category = AbilitySystem, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UAbilitySystemComponent* AbilitySystemComponent;
public:

	//UPROPERTY(EditDefaultsOnly, Category=GameplayEffects)
	//UGameplayAbilitySet * DefaultAbilitySet;

	static FName AbilitySystemComponentName;

	/** Returns AbilitySystemComponent subobject **/
	class UAbilitySystemComponent* GetAbilitySystemComponent() { return AbilitySystemComponent; }
};
