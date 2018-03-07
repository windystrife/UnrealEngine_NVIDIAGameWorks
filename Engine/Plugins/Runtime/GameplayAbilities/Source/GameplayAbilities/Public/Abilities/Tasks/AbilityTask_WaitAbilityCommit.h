// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTagContainer.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitAbilityCommit.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitAbilityCommitDelegate, UGameplayAbility*, ActivatedAbility);

/**
 *	Waits for the actor to activate another ability
 */
UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_WaitAbilityCommit : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitAbilityCommitDelegate	OnCommit;

	virtual void Activate() override;

	UFUNCTION()
	void OnAbilityCommit(UGameplayAbility *ActivatedAbility);

	/** Wait until a new ability (of the same or different type) is commited. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE", DisplayName = "Wait For New Ability Commit"))
	static UAbilityTask_WaitAbilityCommit* WaitForAbilityCommit(UGameplayAbility* OwningAbility, FGameplayTag WithTag, FGameplayTag WithoutTage, bool TriggerOnce=true);

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE", DisplayName = "Wait For New Ability Commit Query"))
	static UAbilityTask_WaitAbilityCommit* WaitForAbilityCommit_Query(UGameplayAbility* OwningAbility, FGameplayTagQuery Query, bool TriggerOnce=true);

	FGameplayTag WithTag;
	FGameplayTag WithoutTag;
	bool TriggerOnce;


	FGameplayTagQuery Query;

protected:

	virtual void OnDestroy(bool AbilityEnded) override;

	FDelegateHandle OnAbilityCommitDelegateHandle;
};
