// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "GameFramework/RootMotionSource.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Net/UnrealNetwork.h"

UAbilityTask_ApplyRootMotionMoveToForce::UAbilityTask_ApplyRootMotionMoveToForce(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bSetNewMovementMode = false;
	NewMovementMode = EMovementMode::MOVE_Walking;
	PreviousMovementMode = EMovementMode::MOVE_None;
	bRestrictSpeedToExpected = false;
	PathOffsetCurve = nullptr;
}

UAbilityTask_ApplyRootMotionMoveToForce* UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(UGameplayAbility* OwningAbility, FName TaskInstanceName, FVector TargetLocation, float Duration, bool bSetNewMovementMode, EMovementMode MovementMode, bool bRestrictSpeedToExpected, UCurveVector* PathOffsetCurve, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, float ClampVelocityOnFinish)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(Duration);

	UAbilityTask_ApplyRootMotionMoveToForce* MyTask = NewAbilityTask<UAbilityTask_ApplyRootMotionMoveToForce>(OwningAbility, TaskInstanceName);

	MyTask->ForceName = TaskInstanceName;
	MyTask->TargetLocation = TargetLocation;
	MyTask->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER); // Avoid negative or divide-by-zero cases
	MyTask->bSetNewMovementMode = bSetNewMovementMode;
	MyTask->NewMovementMode = MovementMode;
	MyTask->bRestrictSpeedToExpected = bRestrictSpeedToExpected;
	MyTask->PathOffsetCurve = PathOffsetCurve;
	MyTask->FinishVelocityMode = VelocityOnFinishMode;
	MyTask->FinishSetVelocity = SetVelocityOnFinish;
	MyTask->FinishClampVelocity = ClampVelocityOnFinish;
	if (MyTask->GetAvatarActor() != nullptr)
	{
		MyTask->StartLocation = MyTask->GetAvatarActor()->GetActorLocation();
	}
	else
	{
		checkf(false, TEXT("UAbilityTask_ApplyRootMotionMoveToForce called without valid avatar actor to get start location from."));
		MyTask->StartLocation = TargetLocation;
	}
	MyTask->SharedInitAndApply();

	return MyTask;
}

void UAbilityTask_ApplyRootMotionMoveToForce::SharedInitAndApply()
{
	if (AbilitySystemComponent->AbilityActorInfo->MovementComponent.IsValid())
	{
		MovementComponent = Cast<UCharacterMovementComponent>(AbilitySystemComponent->AbilityActorInfo->MovementComponent.Get());
		StartTime = GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			if (bSetNewMovementMode)
			{
				PreviousMovementMode = MovementComponent->MovementMode;
				MovementComponent->SetMovementMode(NewMovementMode);
			}

			ForceName = ForceName.IsNone() ? FName("AbilityTaskApplyRootMotionMoveToForce") : ForceName;
			FRootMotionSource_MoveToForce* MoveToForce = new FRootMotionSource_MoveToForce();
			MoveToForce->InstanceName = ForceName;
			MoveToForce->AccumulateMode = ERootMotionAccumulateMode::Override;
			MoveToForce->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
			MoveToForce->Priority = 1000;
			MoveToForce->TargetLocation = TargetLocation;
			MoveToForce->StartLocation = StartLocation;
			MoveToForce->Duration = Duration;
			MoveToForce->bRestrictSpeedToExpected = bRestrictSpeedToExpected;
			MoveToForce->PathOffsetCurve = PathOffsetCurve;
			MoveToForce->FinishVelocityParams.Mode = FinishVelocityMode;
			MoveToForce->FinishVelocityParams.SetVelocity = FinishSetVelocity;
			MoveToForce->FinishVelocityParams.ClampVelocity = FinishClampVelocity;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(MoveToForce);

			if (Ability)
			{
				Ability->SetMovementSyncPoint(ForceName);
			}
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilityTask_ApplyRootMotionMoveToForce called in Ability %s with null MovementComponent; Task Instance Name %s."), 
			Ability ? *Ability->GetName() : TEXT("NULL"), 
			*InstanceName.ToString());
	}
}

void UAbilityTask_ApplyRootMotionMoveToForce::TickTask(float DeltaTime)
{
	if (bIsFinished)
	{
		return;
	}

	Super::TickTask(DeltaTime);

	AActor* MyActor = GetAvatarActor();
	if (MyActor)
	{
		const bool bTimedOut = HasTimedOut();
		const float ReachedDestinationDistanceSqr = 50.f * 50.f;
		const bool bReachedDestination = FVector::DistSquared(TargetLocation, MyActor->GetActorLocation()) < ReachedDestinationDistanceSqr;

		if (bTimedOut)
		{
			// Task has finished
			bIsFinished = true;
			if (!bIsSimulating)
			{
				MyActor->ForceNetUpdate();
				if (bReachedDestination)
				{
					if (ShouldBroadcastAbilityTaskDelegates())
					{
						OnTimedOutAndDestinationReached.Broadcast();
					}
				}
				else
				{
					if (ShouldBroadcastAbilityTaskDelegates())
					{
						OnTimedOut.Broadcast();
					}
				}
				EndTask();
			}
		}
	}
	else
	{
		bIsFinished = true;
		EndTask();
	}
}

void UAbilityTask_ApplyRootMotionMoveToForce::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToForce, StartLocation);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToForce, TargetLocation);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToForce, Duration);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToForce, bSetNewMovementMode);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToForce, NewMovementMode);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToForce, bRestrictSpeedToExpected);
	DOREPLIFETIME(UAbilityTask_ApplyRootMotionMoveToForce, PathOffsetCurve);
}

void UAbilityTask_ApplyRootMotionMoveToForce::PreDestroyFromReplication()
{
	bIsFinished = true;
	EndTask();
}

void UAbilityTask_ApplyRootMotionMoveToForce::OnDestroy(bool AbilityIsEnding)
{
	if (MovementComponent)
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);

		if (bSetNewMovementMode)
		{
			MovementComponent->SetMovementMode(NewMovementMode);
		}
	}

	Super::OnDestroy(AbilityIsEnding);
}
