// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_ApplyRootMotion_Base.h"
#include "AbilityTask_ApplyRootMotionRadialForce.generated.h"

class UCharacterMovementComponent;
class UCurveFloat;
class UGameplayTasksComponent;
enum class ERootMotionFinishVelocityMode : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FApplyRootMotionRadialForceDelegate);

class AActor;

/**
 *	Applies force to character's movement
 */
UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_ApplyRootMotionRadialForce : public UAbilityTask_ApplyRootMotion_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionRadialForceDelegate OnFinish;

	/** Apply force to character's movement */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_ApplyRootMotionRadialForce* ApplyRootMotionRadialForce(UGameplayAbility* OwningAbility, FName TaskInstanceName, FVector Location, AActor* LocationActor, float Strength, float Duration, float Radius, bool bIsPush, bool bIsAdditive, bool bNoZForce, UCurveFloat* StrengthDistanceFalloff, UCurveFloat* StrengthOverTime, bool bUseFixedWorldDirection, FRotator FixedWorldDirection, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, float ClampVelocityOnFinish);

	/** Tick function for this task, if bTickingTask == true */
	virtual void TickTask(float DeltaTime) override;

	virtual void PreDestroyFromReplication() override;
	virtual void OnDestroy(bool AbilityIsEnding) override;

protected:

	virtual void SharedInitAndApply() override;

protected:

	UPROPERTY(Replicated)
	FVector Location;

	UPROPERTY(Replicated)
	AActor* LocationActor;

	UPROPERTY(Replicated)
	float Strength;

	UPROPERTY(Replicated)
	float Duration;

	UPROPERTY(Replicated)
	float Radius;

	UPROPERTY(Replicated)
	bool bIsPush;

	UPROPERTY(Replicated)
	bool bIsAdditive;

	UPROPERTY(Replicated)
	bool bNoZForce;

	/** 
	 *  Strength of the force over distance to Location
	 *  Curve Y is 0 to 1 which is percent of full Strength parameter to apply
	 *  Curve X is 0 to 1 normalized distance (0 = 0cm, 1 = what Strength % at Radius units out)
	 */
	UPROPERTY(Replicated)
	UCurveFloat* StrengthDistanceFalloff;

	/** 
	 *  Strength of the force over time
	 *  Curve Y is 0 to 1 which is percent of full Strength parameter to apply
	 *  Curve X is 0 to 1 normalized time if this force has a limited duration (Duration > 0), or
	 *          is in units of seconds if this force has unlimited duration (Duration < 0)
	 */
	UPROPERTY(Replicated)
	UCurveFloat* StrengthOverTime;

	UPROPERTY(Replicated)
	bool bUseFixedWorldDirection;

	UPROPERTY(Replicated)
	FRotator FixedWorldDirection;
};