// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_Repeat.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRepeatedActionDelegate, int32, ActionNumber);

/**
 *	Repeat a task a certain number of times at a given interval.
 */
UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_Repeat : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FRepeatedActionDelegate		OnPerformAction;

	UPROPERTY(BlueprintAssignable)
	FRepeatedActionDelegate		OnFinished;

	virtual FString GetDebugString() const override;

	void PerformAction();

	/** Start a task that repeats an action or set of actions. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_Repeat* RepeatAction(UGameplayAbility* OwningAbility, float TimeBetweenActions, int32 TotalActionCount);

	virtual void Activate() override;

protected:
	int32 ActionPerformancesDesired;
	int32 ActionCounter;
	float TimeBetweenActions;

	/** Handle for efficient management of PerformAction timer */
	FTimerHandle TimerHandle_PerformAction;

	virtual void OnDestroy(bool AbilityIsEnding) override;
};
