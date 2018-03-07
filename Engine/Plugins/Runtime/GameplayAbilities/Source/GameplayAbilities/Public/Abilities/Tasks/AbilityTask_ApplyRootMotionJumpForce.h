// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_ApplyRootMotion_Base.h"
#include "AbilityTask_ApplyRootMotionJumpForce.generated.h"

class UCharacterMovementComponent;
class UCurveFloat;
class UCurveVector;
class UGameplayTasksComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FApplyRootMotionJumpForceDelegate);

class AActor;

/**
 *	Applies force to character's movement
 */
UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_ApplyRootMotionJumpForce : public UAbilityTask_ApplyRootMotion_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionJumpForceDelegate OnFinish;

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionJumpForceDelegate OnLanded;

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks")
	void Finish();

	UFUNCTION()
	void OnLandedCallback(const FHitResult& Hit);

	/** Apply force to character's movement */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_ApplyRootMotionJumpForce* ApplyRootMotionJumpForce(UGameplayAbility* OwningAbility, FName TaskInstanceName, FRotator Rotation, float Distance, float Height, float Duration, float MinimumLandedTriggerTime,
		bool bFinishOnLanded, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, float ClampVelocityOnFinish, UCurveVector* PathOffsetCurve, UCurveFloat* TimeMappingCurve);

	virtual void Activate() override;

	/** Tick function for this task, if bTickingTask == true */
	virtual void TickTask(float DeltaTime) override;

	virtual void PreDestroyFromReplication() override;
	virtual void OnDestroy(bool AbilityIsEnding) override;

protected:

	virtual void SharedInitAndApply() override;

	/**
	* Work-around for OnLanded being called during bClientUpdating in movement replay code
	* Don't want to trigger our Landed logic during a replay, so we wait until next frame
	* If we don't, we end up removing root motion from a replay root motion set instead
	* of the real one
	*/
	void TriggerLanded();

protected:

	UPROPERTY(Replicated)
	FRotator Rotation;

	UPROPERTY(Replicated)
	float Distance;

	UPROPERTY(Replicated)
	float Height;

	UPROPERTY(Replicated)
	float Duration;

	UPROPERTY(Replicated)
	float MinimumLandedTriggerTime;

	UPROPERTY(Replicated)
	bool bFinishOnLanded;

	UPROPERTY(Replicated)
	UCurveVector* PathOffsetCurve;

	/** 
	 *  Maps real time to movement fraction curve to affect the speed of the
	 *  movement through the path
	 *  Curve X is 0 to 1 normalized real time (a fraction of the duration)
	 *  Curve Y is 0 to 1 is what percent of the move should be at a given X
	 *  Default if unset is a 1:1 correspondence
	 */
	UPROPERTY(Replicated)
	UCurveFloat* TimeMappingCurve;

	bool bHasLanded;
};