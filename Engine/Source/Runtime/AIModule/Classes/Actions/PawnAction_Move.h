// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "AI/Navigation/NavFilters/NavigationQueryFilter.h"
#include "Actions/PawnAction.h"
#include "Navigation/PathFollowingComponent.h"
#include "PawnAction_Move.generated.h"

class AAIController;

UENUM()
namespace EPawnActionMoveMode
{
	enum Type
	{
		UsePathfinding,
		StraightLine,
	};
}

UCLASS()
class AIMODULE_API UPawnAction_Move : public UPawnAction
{
	GENERATED_UCLASS_BODY()
protected:
	UPROPERTY(Category = PawnAction, EditAnywhere, BlueprintReadWrite)
	AActor* GoalActor;

	UPROPERTY(Category = PawnAction, EditAnywhere, BlueprintReadWrite)
	FVector GoalLocation;

	UPROPERTY(Category = PawnAction, EditAnywhere, meta = (ClampMin = "0.01"), BlueprintReadWrite)
	float AcceptableRadius;

	/** "None" will result in default filter being used */
	UPROPERTY(Category = PawnAction, EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UNavigationQueryFilter> FilterClass;

	UPROPERTY(Category = PawnAction, EditAnywhere, BlueprintReadWrite)
	uint32 bAllowStrafe : 1;
	
	/** if set to true (default) will make action succeed when the pawn's collision component overlaps with goal's collision component */
	UPROPERTY()
	uint32 bFinishOnOverlap : 1;

	/** if set, movement will use path finding */
	UPROPERTY()
	uint32 bUsePathfinding : 1;

	/** if set, use incomplete path when goal can't be reached */
	UPROPERTY()
	uint32 bAllowPartialPath : 1;

	/** if set, GoalLocation will be projected on navigation before using  */
	UPROPERTY()
	uint32 bProjectGoalToNavigation : 1;

	/** if set, path to GoalActor will be updated with goal's movement */
	UPROPERTY()
	uint32 bUpdatePathToGoal : 1;

	/** if set, other actions with the same priority will be aborted when path is changed */
	UPROPERTY()
	uint32 bAbortChildActionOnPathChange : 1;

public:
	virtual void BeginDestroy() override;

	static UPawnAction_Move* CreateAction(UWorld& World, AActor* GoalActor, EPawnActionMoveMode::Type Mode);
	static UPawnAction_Move* CreateAction(UWorld& World, const FVector& GoalLocation, EPawnActionMoveMode::Type Mode);

	static bool CheckAlreadyAtGoal(AAIController& Controller, const FVector& TestLocation, float Radius);
	static bool CheckAlreadyAtGoal(AAIController& Controller, const AActor& TestGoal, float Radius);

	virtual void HandleAIMessage(UBrainComponent*, const FAIMessage&) override;

	void SetPath(FNavPathSharedRef InPath);
	virtual void OnPathUpdated(FNavigationPath* UpdatedPath, ENavPathEvent::Type Event);

	void SetAcceptableRadius(float NewAcceptableRadius) { AcceptableRadius = NewAcceptableRadius; }
	void SetFinishOnOverlap(bool bNewFinishOnOverlap) { bFinishOnOverlap = bNewFinishOnOverlap; }
	void EnableStrafing(bool bNewStrafing) { bAllowStrafe = bNewStrafing; }
	void EnablePathUpdateOnMoveGoalLocationChange(bool bEnable) { bUpdatePathToGoal = bEnable; }
	void EnableGoalLocationProjectionToNavigation(bool bEnable) { bProjectGoalToNavigation = bEnable; }
	void EnableChildAbortionOnPathUpdate(bool bEnable) { bAbortChildActionOnPathChange = bEnable; }
	void SetFilterClass(TSubclassOf<UNavigationQueryFilter> NewFilterClass) { FilterClass = NewFilterClass; }
	void SetAllowPartialPath(bool bEnable) { bAllowPartialPath = bEnable; }

protected:
	/** currently followed path */
	FNavPathSharedPtr Path;

	FDelegateHandle PathObserverDelegateHandle;
	
	/** Handle for efficient management of DeferredPerformMoveAction timer */
	FTimerHandle TimerHandle_DeferredPerformMoveAction;

	/** Handle for efficient management of TryToRepath timer */
	FTimerHandle TimerHandle_TryToRepath;

	void ClearPath();
	virtual bool Start() override;
	virtual bool Pause(const UPawnAction* PausedBy) override;
	virtual bool Resume() override;
	virtual void OnFinished(EPawnActionResult::Type WithResult) override;
	virtual EPawnActionAbortState::Type PerformAbort(EAIForceParam::Type ShouldForce) override;
	virtual bool IsPartialPathAllowed() const;

	virtual EPathFollowingRequestResult::Type RequestMove(AAIController& Controller);
	
	bool PerformMoveAction();
	void DeferredPerformMoveAction();

	void TryToRepath();
	void ClearPendingRepath();
	void ClearTimers();
};
