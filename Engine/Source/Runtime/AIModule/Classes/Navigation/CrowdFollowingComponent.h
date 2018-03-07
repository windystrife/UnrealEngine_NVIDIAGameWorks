// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EngineDefines.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "Navigation/CrowdAgentInterface.h"
#include "AI/Navigation/NavigationAvoidanceTypes.h"
#include "CrowdFollowingComponent.generated.h"

class INavLinkCustomInterface;
class UCharacterMovementComponent;
class UCrowdManager;

namespace ECrowdAvoidanceQuality
{
	enum Type
	{
		Low,
		Medium,
		Good,
		High,
	};
}

enum class ECrowdSimulationState : uint8
{
	Enabled,
	ObstacleOnly	UMETA(DisplayName="Disabled, avoided by others"),
	Disabled		UMETA(DisplayName="Disabled, ignored by others"),
};

UCLASS(BlueprintType)
class AIMODULE_API UCrowdFollowingComponent : public UPathFollowingComponent, public ICrowdAgentInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FVector CrowdAgentMoveDirection;

	virtual void BeginDestroy() override;

	// ICrowdAgentInterface BEGIN
	virtual FVector GetCrowdAgentLocation() const override;
	virtual FVector GetCrowdAgentVelocity() const override;
	virtual void GetCrowdAgentCollisions(float& CylinderRadius, float& CylinderHalfHeight) const override;
	virtual float GetCrowdAgentMaxSpeed() const override;
	virtual int32 GetCrowdAgentAvoidanceGroup() const override;
	virtual int32 GetCrowdAgentGroupsToAvoid() const override;
	virtual int32 GetCrowdAgentGroupsToIgnore() const override;
	// ICrowdAgentInterface END

	// PathFollowingComponent BEGIN
	virtual void Initialize() override;
	virtual void Cleanup() override;
	virtual void AbortMove(const UObject& Instigator, FPathFollowingResultFlags::Type AbortFlags, FAIRequestID RequestID = FAIRequestID::CurrentRequest, EPathFollowingVelocityMode VelocityMode = EPathFollowingVelocityMode::Reset) override;
	virtual void PauseMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest, EPathFollowingVelocityMode VelocityMode = EPathFollowingVelocityMode::Reset) override;
	virtual void ResumeMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest) override;
	virtual FVector GetMoveFocus(bool bAllowStrafe) const override;
	virtual void OnLanded() override;
	virtual void FinishUsingCustomLink(INavLinkCustomInterface* CustomNavLink) override;
	virtual void OnPathFinished(const FPathFollowingResult& Result) override;
	virtual void OnPathUpdated() override;
	virtual void OnPathfindingQuery(FPathFindingQuery& Query) override;
	virtual int32 GetCurrentPathElement() const override { return LastPathPolyIndex; }
	virtual void OnNavigationInitDone() override;
	// PathFollowingComponent END

	/** update params in crowd manager */
	void UpdateCrowdAgentParams() const;

	/** pass agent velocity to movement component */
	virtual void ApplyCrowdAgentVelocity(const FVector& NewVelocity, const FVector& DestPathCorner, bool bTraversingLink, bool bIsNearEndOfPath);
	
	/** pass desired position to movement component (after resolving collisions between crowd agents) */
	virtual void ApplyCrowdAgentPosition(const FVector& NewPosition);

	/** master switch for crowd steering & avoidance */
	UFUNCTION(BlueprintCallable, Category = "Crowd")
	virtual void SuspendCrowdSteering(bool bSuspend);

	/** switch between crowd simulation and parent implementation (following path segments) */
	virtual void SetCrowdSimulationState(ECrowdSimulationState NewState);

	/** called when agent moved to next nav node (poly) */
	virtual void OnNavNodeChanged(NavNodeRef NewPolyRef, NavNodeRef PrevPolyRef, int32 CorridorSize);

	void SetCrowdAnticipateTurns(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdObstacleAvoidance(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdSeparation(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdOptimizeVisibility(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdOptimizeTopology(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdPathOffset(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdSlowdownAtGoal(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdSeparationWeight(float Weight, bool bUpdateAgent = true);
	void SetCrowdCollisionQueryRange(float Range, bool bUpdateAgent = true);
	void SetCrowdPathOptimizationRange(float Range, bool bUpdateAgent = true);
	void SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Type Quality, bool bUpdateAgent = true);
	void SetCrowdAvoidanceRangeMultiplier(float Multiplier, bool bUpdateAgent = true);
	void SetCrowdAffectFallingVelocity(bool bEnable);
	void SetCrowdRotateToVelocity(bool bEnable);
	void SetAvoidanceGroup(int32 GroupFlags, bool bUpdateAgent = true);
	void SetGroupsToAvoid(int32 GroupFlags, bool bUpdateAgent = true);
	void SetGroupsToIgnore(int32 GroupFlags, bool bUpdateAgent = true);

	FORCEINLINE bool IsCrowdSimulationEnabled() const { return SimulationState == ECrowdSimulationState::Enabled; }
	FORCEINLINE bool IsCrowdSimulatioSuspended() const { return bSuspendCrowdSimulation; }
	FORCEINLINE bool IsCrowdAnticipateTurnsEnabled() const { return bEnableAnticipateTurns; }
	FORCEINLINE bool IsCrowdObstacleAvoidanceEnabled() const { return bEnableObstacleAvoidance; }
	FORCEINLINE bool IsCrowdSeparationEnabled() const { return bEnableSeparation; }
	FORCEINLINE bool IsCrowdOptimizeVisibilityEnabled() const { return bEnableOptimizeVisibility; /** don't check suspend here! */ }
	FORCEINLINE bool IsCrowdOptimizeTopologyEnabled() const { return bEnableOptimizeTopology; }
	FORCEINLINE bool IsCrowdPathOffsetEnabled() const { return bEnablePathOffset; }
	FORCEINLINE bool IsCrowdSlowdownAtGoalEnabled() const { return bEnableSlowdownAtGoal; }
	FORCEINLINE bool IsCrowdAffectFallingVelocityEnabled() const { return bAffectFallingVelocity; }
	FORCEINLINE bool IsCrowdRotateToVelocityEnabled() const { return bRotateToVelocity; }

	FORCEINLINE ECrowdSimulationState GetCrowdSimulationState() const { return SimulationState; }
	FORCEINLINE bool IsCrowdSimulationActive() const { return IsCrowdSimulationEnabled() && !IsCrowdSimulatioSuspended(); }
	/** checks if bEnableAnticipateTurns is set to true, and if crowd simulation is not suspended */
	FORCEINLINE bool IsCrowdAnticipateTurnsActive() const { return IsCrowdAnticipateTurnsEnabled() && !IsCrowdSimulatioSuspended(); }
	/** checks if bEnableObstacleAvoidance is set to true, and if crowd simulation is not suspended */
	FORCEINLINE bool IsCrowdObstacleAvoidanceActive() const { return IsCrowdObstacleAvoidanceEnabled() && !IsCrowdSimulatioSuspended(); }
	/** checks if bEnableSeparation is set to true, and if crowd simulation is not suspended */
	FORCEINLINE bool IsCrowdSeparationActive() const { return IsCrowdSeparationEnabled() && !IsCrowdSimulatioSuspended(); }
	/** checks if bEnableOptimizeTopology is set to true, and if crowd simulation is not suspended */
	FORCEINLINE bool IsCrowdOptimizeTopologyActive() const { return IsCrowdOptimizeTopologyEnabled() && !IsCrowdSimulatioSuspended(); }

	FORCEINLINE float GetCrowdSeparationWeight() const { return SeparationWeight; }
	FORCEINLINE float GetCrowdCollisionQueryRange() const { return CollisionQueryRange; }
	FORCEINLINE float GetCrowdPathOptimizationRange() const { return PathOptimizationRange; }
	FORCEINLINE ECrowdAvoidanceQuality::Type GetCrowdAvoidanceQuality() const { return AvoidanceQuality; }
	FORCEINLINE float GetCrowdAvoidanceRangeMultiplier() const { return AvoidanceRangeMultiplier; }
	int32 GetAvoidanceGroup() const;
	int32 GetGroupsToAvoid() const;
	int32 GetGroupsToIgnore() const;

	virtual void GetDebugStringTokens(TArray<FString>& Tokens, TArray<EPathFollowingDebugTokens::Type>& Flags) const override;
#if ENABLE_VISUAL_LOG
	virtual void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const override;
#endif // ENABLE_VISUAL_LOG

	DEPRECATED(4.11, "Use SetCrowdSimulationState function instead.")
	virtual void SetCrowdSimulation(bool bEnable);

	DEPRECATED_FORGAME(4.16, "Use ApplyCrowdAgentVelocity function with bIsNearEndOfPath param instead.")
	virtual void ApplyCrowdAgentVelocity(const FVector& NewVelocity, const FVector& DestPathCorner, bool bTraversingLink) {}

	void UpdateDestinationForMovingGoal(const FVector& NewDestination);

protected:

	UPROPERTY(transient)
	UCharacterMovementComponent* CharacterMovement;

	/** DEPRECATED: Group mask for this agent - use property from CharacterMovementComponent instead */
	UPROPERTY()
	FNavAvoidanceMask AvoidanceGroup_DEPRECATED;

	/** DEPRECATED: Will avoid other agents if they are in one of specified groups - use property from CharacterMovementComponent instead */
	UPROPERTY()
	FNavAvoidanceMask GroupsToAvoid_DEPRECATED;

	/** DEPRECATED: Will NOT avoid other agents if they are in one of specified groups, higher priority than GroupsToAvoid - use property from CharacterMovementComponent instead */
	UPROPERTY()
	FNavAvoidanceMask GroupsToIgnore_DEPRECATED;

	/** if set, velocity will be updated even if agent is falling */
	uint32 bAffectFallingVelocity : 1;

	/** if set, move focus will match velocity direction */
	uint32 bRotateToVelocity : 1;

	/** if set, move velocity will be updated in every tick */
	uint32 bUpdateDirectMoveVelocity : 1;

	DEPRECATED(4.11, "Please use IsCrowdSimulationEnabled(), SetCrowdSimulationState() and SimulationState member for initialization.")
	uint32 bEnableCrowdSimulation : 1;

	/** set when agent is registered in crowd simulation (either controlled or an obstacle) */
	uint32 bRegisteredWithCrowdSimulation : 1;

	/** if set, avoidance and steering will be suspended (used for direct move requests) */
	uint32 bSuspendCrowdSimulation : 1;

	uint32 bEnableAnticipateTurns : 1;
	uint32 bEnableObstacleAvoidance : 1;
	uint32 bEnableSeparation : 1;
	uint32 bEnableOptimizeVisibility : 1;
	uint32 bEnableOptimizeTopology : 1;
	uint32 bEnablePathOffset : 1;
	uint32 bEnableSlowdownAtGoal : 1;

	/** if set, agent if moving on final path part, skip further updates (runtime flag) */
	uint32 bFinalPathPart : 1;

	/** if set, destination overshot can be tested */
	uint32 bCanCheckMovingTooFar : 1;

	/** if set, movement will be finished when velocity is opposite to path direction (runtime flag) */
	uint32 bCheckMovementAngle : 1;

	uint32 bEnableSimulationReplanOnResume : 1;

	float SeparationWeight;
	float CollisionQueryRange;
	float PathOptimizationRange;

	/** multiplier for avoidance samples during detection, doesn't affect actual velocity */
	float AvoidanceRangeMultiplier;

	/** start index of current path part */
	int32 PathStartIndex;

	/** last visited poly on path */
	int32 LastPathPolyIndex;

	TEnumAsByte<ECrowdAvoidanceQuality::Type> AvoidanceQuality;
	ECrowdSimulationState SimulationState;

	// PathFollowingComponent BEGIN
	virtual int32 DetermineStartingPathPoint(const FNavigationPath* ConsideredPath) const override;
	virtual void SetMoveSegment(int32 SegmentStartIndex) override;
	virtual void UpdatePathSegment() override;
	virtual void FollowPathSegment(float DeltaTime) override;
	virtual bool ShouldCheckPathOnResume() const override;
	virtual bool IsOnPath() const override;
	virtual bool UpdateMovementComponent(bool bForce) override;
	virtual void Reset() override;
	// PathFollowingComponent END

	void SwitchToNextPathPart();
	bool ShouldSwitchPathPart(int32 CorridorSize) const;
	bool HasMovedDuringPause() const;
	void UpdateCachedDirections(const FVector& NewVelocity, const FVector& NextPathCorner, bool bTraversingLink);

	DEPRECATED(4.12, "This function is now deprecated and was renamed to ShouldTrackMovingGoal.")
	virtual bool UpdateCachedGoal(FVector& NewGoalPos);
	virtual bool ShouldTrackMovingGoal(FVector& OutGoalLocation) const;
	
	void RegisterCrowdAgent();

	friend UCrowdManager;
};
