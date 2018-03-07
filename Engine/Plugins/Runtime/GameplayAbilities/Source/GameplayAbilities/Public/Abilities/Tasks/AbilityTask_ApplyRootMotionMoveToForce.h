// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_ApplyRootMotion_Base.h"
#include "AbilityTask_ApplyRootMotionMoveToForce.generated.h"

class UCharacterMovementComponent;
class UCurveVector;
class UGameplayTasksComponent;
enum class ERootMotionFinishVelocityMode : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FApplyRootMotionMoveToForceDelegate);

class AActor;

/**
 *	Applies force to character's movement
 */
UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_ApplyRootMotionMoveToForce : public UAbilityTask_ApplyRootMotion_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionMoveToForceDelegate OnTimedOut;

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionMoveToForceDelegate OnTimedOutAndDestinationReached;

	/** Apply force to character's movement */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_ApplyRootMotionMoveToForce* ApplyRootMotionMoveToForce(UGameplayAbility* OwningAbility, FName TaskInstanceName, FVector TargetLocation, float Duration, bool bSetNewMovementMode, EMovementMode MovementMode, bool bRestrictSpeedToExpected, UCurveVector* PathOffsetCurve, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, float ClampVelocityOnFinish);

	/** Tick function for this task, if bTickingTask == true */
	virtual void TickTask(float DeltaTime) override;

	virtual void PreDestroyFromReplication() override;
	virtual void OnDestroy(bool AbilityIsEnding) override;

protected:

	virtual void SharedInitAndApply() override;

protected:

	UPROPERTY(Replicated)
	FVector StartLocation;

	UPROPERTY(Replicated)
	FVector TargetLocation;

	UPROPERTY(Replicated)
	float Duration;

	UPROPERTY(Replicated)
	bool bSetNewMovementMode;

	UPROPERTY(Replicated)
	TEnumAsByte<EMovementMode> NewMovementMode;

	/** If enabled, we limit velocity to the initial expected velocity to go distance to the target over Duration.
	 *  This prevents cases of getting really high velocity the last few frames of the root motion if you were being blocked by
	 *  collision. Disabled means we do everything we can to velocity during the move to get to the TargetLocation. */
	UPROPERTY(Replicated)
	bool bRestrictSpeedToExpected;

	UPROPERTY(Replicated)
	UCurveVector* PathOffsetCurve;

	EMovementMode PreviousMovementMode;
};