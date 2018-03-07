// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayEffectTypes.h"
#include "Abilities/GameplayAbilityTargetDataFilter.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitGameplayEffectApplied.generated.h"

class UAbilitySystemComponent;

UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_WaitGameplayEffectApplied : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UFUNCTION()
	void OnApplyGameplayEffectCallback(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle);

	virtual void Activate() override;

	FGameplayTargetDataFilterHandle Filter;
	FGameplayTagRequirements SourceTagRequirements;
	FGameplayTagRequirements TargetTagRequirements;

	FGameplayTagQuery SourceTagQuery;
	FGameplayTagQuery TargetTagQuery;

	bool TriggerOnce;
	bool ListenForPeriodicEffects;

	void SetExternalActor(AActor* InActor);

protected:

	UAbilitySystemComponent* GetASC();

	virtual void BroadcastDelegate(AActor* Avatar, FGameplayEffectSpecHandle SpecHandle, FActiveGameplayEffectHandle ActiveHandle) { }
	virtual void RegisterDelegate() { }
	virtual void RemoveDelegate() { }

	virtual void OnDestroy(bool AbilityEnded) override;

	bool RegisteredCallback;
	bool UseExternalOwner;

	UPROPERTY()
	UAbilitySystemComponent* ExternalOwner;

	// If we are in the process of broadcasting and should not accept additional GE callbacks
	bool Locked;
};