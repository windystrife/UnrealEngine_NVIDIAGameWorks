// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "EngineDefines.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AITypes.h"
#include "AIResourceInterface.h"
#include "AI/Navigation/NavigationData.h"
#include "GameFramework/NavMovementComponent.h"
#include "PathFollowingComponent.generated.h"

class Error;
class FDebugDisplayInfo;
class INavLinkCustomInterface;
class UCanvas;

AIMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogPathFollowing, Warning, All);

class UCanvas;
class AActor;
class INavLinkCustomInterface;
class INavAgentInterface;
class UNavigationComponent;

UENUM(BlueprintType)
namespace EPathFollowingStatus
{
	enum Type
	{
		/** No requests */
		Idle,

		/** Request with incomplete path, will start after UpdateMove() */
		Waiting,

		/** Request paused, will continue after ResumeMove() */
		Paused,

		/** Following path */
		Moving,
	};
}

UENUM(BlueprintType)
namespace EPathFollowingResult
{
	enum Type
	{
		/** Reached destination */
		Success,

		/** Movement was blocked */
		Blocked,

		/** Agent is not on path */
		OffPath,

		/** Aborted and stopped (failure) */
		Aborted,

		/** DEPRECATED, use Aborted result instead */
		Skipped_DEPRECATED UMETA(Hidden),

		/** Request was invalid */
		Invalid,
	};
}

namespace FPathFollowingResultFlags
{
	typedef uint16 Type;

	const Type None = 0;

	/** Reached destination (EPathFollowingResult::Success) */
	const Type Success = (1 << 0);

	/** Movement was blocked (EPathFollowingResult::Blocked) */
	const Type Blocked = (1 << 1);

	/** Agent is not on path (EPathFollowingResult::OffPath) */
	const Type OffPath = (1 << 2);

	/** Aborted (EPathFollowingResult::Aborted) */
	const Type UserAbort = (1 << 3);

	/** Abort details: owner no longer wants to move */
	const Type OwnerFinished = (1 << 4);

	/** Abort details: path is no longer valid */
	const Type InvalidPath = (1 << 5);

	/** Abort details: unable to move */
	const Type MovementStop = (1 << 6);

	/** Abort details: new movement request was received */
	const Type NewRequest = (1 << 7);

	/** Abort details: blueprint MoveTo function was called */
	const Type ForcedScript = (1 << 8);

	/** Finish details: never started, agent was already at goal */
	const Type AlreadyAtGoal = (1 << 9);

	/** Can be used to create project specific reasons */
	const Type FirstGameplayFlagShift = 10;

	const Type UserAbortFlagMask = ~(Success | Blocked | OffPath);

	FString ToString(uint16 Value);
}

struct AIMODULE_API FPathFollowingResult
{
	FPathFollowingResultFlags::Type Flags;
	TEnumAsByte<EPathFollowingResult::Type> Code;

	FPathFollowingResult() : Flags(0), Code(EPathFollowingResult::Invalid)  {}
	FPathFollowingResult(FPathFollowingResultFlags::Type InFlags);
	FPathFollowingResult(EPathFollowingResult::Type ResultCode, FPathFollowingResultFlags::Type ExtraFlags);

	bool HasFlag(FPathFollowingResultFlags::Type Flag) const { return (Flags & Flag) != 0; }

	bool IsSuccess() const { return HasFlag(FPathFollowingResultFlags::Success); }
	bool IsFailure() const { return !HasFlag(FPathFollowingResultFlags::Success); }
	bool IsInterrupted() const { return HasFlag(FPathFollowingResultFlags::UserAbort | FPathFollowingResultFlags::NewRequest); }
	
	FString ToString() const;
};

// DEPRECATED, will be removed with GetPathActionType function
UENUM(BlueprintType)
namespace EPathFollowingAction
{
	enum Type
	{
		Error,
		NoMove,
		DirectMove,
		PartialPath,
		PathToGoal,
	};
}

UENUM(BlueprintType)
namespace EPathFollowingRequestResult
{
	enum Type
	{
		Failed,
		AlreadyAtGoal,
		RequestSuccessful
	};
}

struct AIMODULE_API FPathFollowingRequestResult
{
	FAIRequestID MoveId;
	TEnumAsByte<EPathFollowingRequestResult::Type> Code;

	FPathFollowingRequestResult() : MoveId(FAIRequestID::InvalidRequest), Code(EPathFollowingRequestResult::Failed) {}
	operator EPathFollowingRequestResult::Type() const { return Code; }
};

namespace EPathFollowingDebugTokens
{
	enum Type
	{
		Description,
		ParamName,
		FailedValue,
		PassedValue,
	};
}

// DEPRECATED, please use EPathFollowingResultDetails instead, will be removed with deprecated override of AbortMove function
namespace EPathFollowingMessage
{
	enum Type
	{
		NoPath,
		OtherRequest,
	};
}

enum class EPathFollowingVelocityMode : uint8
{
	Reset,
	Keep,
};

enum class EPathFollowingReachMode : uint8
{
	/** reach test uses only AcceptanceRadius */
	ExactLocation,

	/** reach test uses AcceptanceRadius increased by modified agent radius */
	OverlapAgent,

	/** reach test uses AcceptanceRadius increased by goal actor radius */
	OverlapGoal,

	/** reach test uses AcceptanceRadius increased by modified agent radius AND goal actor radius */
	OverlapAgentAndGoal,
};

UCLASS(config=Engine)
class AIMODULE_API UPathFollowingComponent : public UActorComponent, public IAIResourceInterface
{
	GENERATED_UCLASS_BODY()

	DECLARE_DELEGATE_TwoParams(FPostProcessMoveSignature, UPathFollowingComponent* /*comp*/, FVector& /*velocity*/);
	DECLARE_DELEGATE_OneParam(FRequestCompletedSignature, EPathFollowingResult::Type /*Result*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FMoveCompletedSignature, FAIRequestID /*RequestID*/, EPathFollowingResult::Type /*Result*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FMoveComplete, FAIRequestID /*RequestID*/, const FPathFollowingResult& /*Result*/);

	/** delegate for modifying path following velocity */
	FPostProcessMoveSignature PostProcessMove;

	/** delegate for move completion notify */
	FMoveComplete OnRequestFinished;

	//~ Begin UActorComponent Interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	//~ End UActorComponent Interface

	/** initialize component to use */
	virtual void Initialize();

	/** cleanup component before destroying */
	virtual void Cleanup();

	/** updates cached pointers to relevant owner's components */
	virtual void UpdateCachedComponents();

	/** start movement along path
	  * @return MoveId of requested move
	  */
	virtual FAIRequestID RequestMove(const FAIMoveRequest& RequestData, FNavPathSharedPtr InPath);

	/** aborts following path */
	virtual void AbortMove(const UObject& Instigator, FPathFollowingResultFlags::Type AbortFlags, FAIRequestID RequestID = FAIRequestID::CurrentRequest, EPathFollowingVelocityMode VelocityMode = EPathFollowingVelocityMode::Reset);

	/** create new request and finish it immediately (e.g. already at goal)
	 *  @return MoveId of requested (and already finished) move
	 */
	FAIRequestID RequestMoveWithImmediateFinish(EPathFollowingResult::Type Result, EPathFollowingVelocityMode VelocityMode = EPathFollowingVelocityMode::Reset);

	/** pause path following
	*  @param RequestID - request to pause, FAIRequestID::CurrentRequest means pause current request, regardless of its ID */
	virtual void PauseMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest, EPathFollowingVelocityMode VelocityMode = EPathFollowingVelocityMode::Reset);

	/** resume path following
	*  @param RequestID - request to resume, FAIRequestID::CurrentRequest means restor current request, regardless of its ID */
	virtual void ResumeMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest);

	/** notify about finished movement */
	virtual void OnPathFinished(const FPathFollowingResult& Result);

	FORCEINLINE void OnPathFinished(EPathFollowingResult::Type ResultCode, uint16 ExtraResultFlags) { OnPathFinished(FPathFollowingResult(ResultCode, ExtraResultFlags)); }

	/** notify about finishing move along current path segment */
	virtual void OnSegmentFinished();

	/** notify about changing current path: new pointer or update from path event */
	virtual void OnPathUpdated();

	/** set associated movement component */
	virtual void SetMovementComponent(UNavMovementComponent* MoveComp);

	/** get current focal point of movement */
	virtual FVector GetMoveFocus(bool bAllowStrafe) const;

	/** simple test for stationary agent (used as early finish condition), check if reached given point
	 *  @param TestPoint - point to test
	 *  @param AcceptanceRadius - allowed 2D distance
	 *  @param ReachMode - modifiers for AcceptanceRadius
	 */
	bool HasReached(const FVector& TestPoint, EPathFollowingReachMode ReachMode, float AcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius) const;

	/** simple test for stationary agent (used as early finish condition), check if reached given goal
	 *  @param TestGoal - actor to test
	 *  @param AcceptanceRadius - allowed 2D distance
	 *  @param ReachMode - modifiers for AcceptanceRadius
	 *  @param bUseNavAgentGoalLocation - true: if the goal is a nav agent, we will use their nav agent location rather than their actual location
	 */
	bool HasReached(const AActor& TestGoal, EPathFollowingReachMode ReachMode, float AcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius, bool bUseNavAgentGoalLocation = true) const;

	/** simple test for stationary agent (used as early finish condition), check if reached target specified in move request */
	bool HasReached(const FAIMoveRequest& MoveRequest) const;

	/** update state of block detection */
	void SetBlockDetectionState(bool bEnable);

	/** @returns state of block detection */
	bool IsBlockDetectionActive() const { return bUseBlockDetection; }

	/** set block detection params */
	void SetBlockDetection(float DistanceThreshold, float Interval, int32 NumSamples);

	/** Returns true if pathfollowing is doing deceleration at the end of the path. */
	bool IsDecelerating() const { return bIsDecelerating; };

	/** @returns state of movement stopping on finish */
	FORCEINLINE bool IsStopMovementOnFinishActive() const { return bStopMovementOnFinish; }
	
	/** set whether movement is stopped on finish of move. */
	FORCEINLINE void SetStopMovementOnFinish(bool bEnable) { bStopMovementOnFinish = bEnable; }

	/** set threshold for precise reach tests in intermediate goals (minimal test radius)  */
	void SetPreciseReachThreshold(float AgentRadiusMultiplier, float AgentHalfHeightMultiplier);

	/** set status of last requested move, works only in Idle state */
	void SetLastMoveAtGoal(bool bFinishedAtGoal);

	/** @returns estimated cost of unprocessed path segments
	 *	@NOTE 0 means, that component is following final path segment or doesn't move */
	float GetRemainingPathCost() const;
	
	/** Returns current location on navigation data */
	FNavLocation GetCurrentNavLocation() const;

	FORCEINLINE EPathFollowingStatus::Type GetStatus() const { return Status; }
	FORCEINLINE float GetAcceptanceRadius() const { return AcceptanceRadius; }
	FORCEINLINE float GetDefaultAcceptanceRadius() const { return MyDefaultAcceptanceRadius; }
	void SetAcceptanceRadius(const float InAcceptanceRadius);
	FORCEINLINE AActor* GetMoveGoal() const { return DestinationActor.Get(); }
	FORCEINLINE bool HasPartialPath() const { return Path.IsValid() && Path->IsPartial(); }
	FORCEINLINE bool DidMoveReachGoal() const { return bLastMoveReachedGoal && (Status == EPathFollowingStatus::Idle); }

	FORCEINLINE FAIRequestID GetCurrentRequestId() const { return CurrentRequestId; }
	FORCEINLINE uint32 GetCurrentPathIndex() const { return MoveSegmentStartIndex; }
	FORCEINLINE uint32 GetNextPathIndex() const { return MoveSegmentEndIndex; }
	FORCEINLINE UObject* GetCurrentCustomLinkOb() const { return CurrentCustomLinkOb.Get(); }
	FORCEINLINE FVector GetCurrentTargetLocation() const { return *CurrentDestination; }
	FORCEINLINE FBasedPosition GetCurrentTargetLocationBased() const { return CurrentDestination; }
	FORCEINLINE FVector GetMoveGoalLocationOffset() const { return MoveOffset; }
	bool HasStartedNavLinkMove() const { return bWalkingNavLinkStart; }
	bool IsCurrentSegmentNavigationLink() const;
	FVector GetCurrentDirection() const;
	/** note that CurrentMoveInput is only valid if MovementComp->UseAccelerationForPathFollowing() == true */
	FVector GetCurrentMoveInput() const { return CurrentMoveInput; }

	/** check if path following has authority over movement (e.g. not falling) and can update own state */
	FORCEINLINE bool HasMovementAuthority() const { return (MovementComp == nullptr) || MovementComp->CanStopPathFollowing(); }

	FORCEINLINE const FNavPathSharedPtr GetPath() const { return Path; }
	FORCEINLINE bool HasValidPath() const { return Path.IsValid() && Path->IsValid(); }
	bool HasDirectPath() const;

	/** readable name of current status */
	FString GetStatusDesc() const;
	/** readable name of result enum */
	FString GetResultDesc(EPathFollowingResult::Type Result) const;

	void SetDestinationActor(const AActor* InDestinationActor);

	/** returns index of the currently followed element of path. Depending on the actual 
	 *	path it may represent different things, like a path point or navigation corridor index */
	virtual int32 GetCurrentPathElement() const { return MoveSegmentEndIndex; }

	virtual void GetDebugStringTokens(TArray<FString>& Tokens, TArray<EPathFollowingDebugTokens::Type>& Flags) const;
	virtual FString GetDebugString() const;

	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) const;
#if ENABLE_VISUAL_LOG
	virtual void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const;
#endif // ENABLE_VISUAL_LOG

	/** called when moving agent collides with another actor */
	UFUNCTION()
	virtual void OnActorBump(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	/** Called when movement is blocked by a collision with another actor.  */
	virtual void OnMoveBlockedBy(const FHitResult& BlockingImpact) {}

	/** Called when falling movement starts. */
	virtual void OnStartedFalling();

	/** Called when falling movement ends. */
	virtual void OnLanded() {}

	/** Check if path following can be activated */
	virtual bool IsPathFollowingAllowed() const;

	/** call when moving agent finishes using custom nav link, returns control back to path following */
	virtual void FinishUsingCustomLink(INavLinkCustomInterface* CustomNavLink);

	/** called when owner is preparing new pathfinding request */
	virtual void OnPathfindingQuery(FPathFindingQuery& Query) {}

	// IAIResourceInterface begin
	virtual void LockResource(EAIRequestPriority::Type LockSource) override;
	virtual void ClearResourceLock(EAIRequestPriority::Type LockSource) override;
	virtual void ForceUnlockResource() override;
	virtual bool IsResourceLocked() const override;
	// IAIResourceInterface end

	/** path observer */
	void OnPathEvent(FNavigationPath* InPath, ENavPathEvent::Type Event);

	/** helper function for sending a path for visual log */
	static void LogPathHelper(const AActor* LogOwner, FNavPathSharedPtr InLogPath, const AActor* LogGoalActor);
	static void LogPathHelper(const AActor* LogOwner, FNavigationPath* InLogPath, const AActor* LogGoalActor);

	DEPRECATED(4.12, "This function is now deprecated and replaced with HandlePathUpdateEvent. Receiving new path pointer for the same move request is no longer supported, please either update data within current path and call FNavigationPath::DoneUpdating or start new move request.")
	virtual bool UpdateMove(FNavPathSharedPtr Path, FAIRequestID RequestID = FAIRequestID::CurrentRequest);

	DEPRECATED(4.13, "This function is now deprecated, please use version with FAIMoveRequest parameter instead. Any observers needs to register with OnRequestFinished mutlicast delegate now.")
	virtual FAIRequestID RequestMove(FNavPathSharedPtr Path, FRequestCompletedSignature OnComplete, const AActor* DestinationActor = NULL, float AcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius, bool bStopOnOverlap = true, FCustomMoveSharedPtr GameData = NULL);

	DEPRECATED(4.13, "This function is now deprecated, please use version with FAIMoveRequest parameter instead.")
	FAIRequestID RequestMove(FNavPathSharedPtr InPath, const AActor* InDestinationActor = NULL, float InAcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius, bool InStopOnOverlap = true, FCustomMoveSharedPtr InGameData = NULL);

	DEPRECATED(4.13, "This function is now deprecated, please use version with EPathFollowingResultDetails parameter instead.")
	virtual void AbortMove(const FString& Reason, FAIRequestID RequestID = FAIRequestID::CurrentRequest, bool bResetVelocity = true, bool bSilent = false, uint8 MessageFlags = 0);

	DEPRECATED(4.13, "This function is now deprecated, please use version with EPathFollowingVelocityMode parameter instead.")
	virtual void PauseMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest, bool bResetVelocity = true);

	UFUNCTION(BlueprintCallable, Category="AI|Components|PathFollowing", meta = (DeprecatedFunction, DeprecationMessage = "This function is now deprecated, please use AIController.GetMoveStatus instead"))
	EPathFollowingAction::Type GetPathActionType() const;

	UFUNCTION(BlueprintCallable, Category="AI|Components|PathFollowing", meta = (DeprecatedFunction, DeprecationMessage = "This function is now deprecated, please use AIController.GetImmediateMoveDestination instead"))
	FVector GetPathDestination() const;

	DEPRECATED(4.13, "This function is now deprecated, please version with FPathFollowingResult parameter instead.")
	virtual void OnPathFinished(EPathFollowingResult::Type Result);

	DEPRECATED(4.13, "This function is now deprecated and no longer supported.")
	int32 OptimizeSegmentVisibility(int32 StartIndex) { return StartIndex + 1; }

	DEPRECATED(4.13, "This function is now deprecated, please use version with EPathFollowingReachMode parameter instead.")
	bool HasReached(const FVector& TestPoint, float AcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius, bool bExactSpot = false) const;

	DEPRECATED(4.13, "This function is now deprecated, please use version with EPathFollowingReachMode parameter instead.")
	bool HasReached(const AActor& TestGoal, float AcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius, bool bExactSpot = false, bool bUseNavAgentGoalLocation = true) const;

	// This delegate is now deprecated, please use OnRequestFinished instead
	FMoveCompletedSignature OnMoveFinished_DEPRECATED;

protected:

	/** associated movement component */
	UPROPERTY(transient)
	UNavMovementComponent* MovementComp;

	/** currently traversed custom nav link */
	FWeakObjectPtr CurrentCustomLinkOb;

	/** navigation data for agent described in movement component */
	UPROPERTY(transient)
	ANavigationData* MyNavData;

	/** current status */
	EPathFollowingStatus::Type Status;

	/** requested path */
	FNavPathSharedPtr Path;

	/** value based on navigation agent's properties that's used for AcceptanceRadius when DefaultAcceptanceRadius is requested */
	float MyDefaultAcceptanceRadius;

	/** min distance to destination to consider request successful.
	 *	If following a partial path movement request will finish
	 *	when the original goal gets within AcceptanceRadius or 
	 *	pathfollowing agent gets within MyDefaultAcceptanceRadius 
	 *	of the end of the path*/
	float AcceptanceRadius;

	/** min distance to end of current path segment to consider segment finished */
	float CurrentAcceptanceRadius;

	/** part of agent radius used as min acceptance radius */
	float MinAgentRadiusPct;

	/** part of agent height used as min acceptable height difference */
	float MinAgentHalfHeightPct;

	/** game specific data */
	FCustomMoveSharedPtr GameData;

	/** destination actor. Use SetDestinationActor to set this */
	TWeakObjectPtr<AActor> DestinationActor;

	/** cached DestinationActor cast to INavAgentInterface. Use SetDestinationActor to set this */
	const INavAgentInterface* DestinationAgent;

	/** destination for current path segment */
	FBasedPosition CurrentDestination;

	/** last MoveInput calculated and passed over to MovementComponent. Valid only if MovementComp->UseAccelerationForPathFollowing() == true */
	FVector CurrentMoveInput;

	/** relative offset from goal actor's location to end of path */
	FVector MoveOffset;

	/** agent location when movement was paused */
	FVector LocationWhenPaused;

	/** This is needed for partial paths when trying to figure out if following a path should finish
	 *	before reaching path end, due to reaching requested acceptance radius away from original
	 *	move goal
	 *	Is being set for non-partial paths as well */
	FVector OriginalMoveRequestGoalLocation;

	/** timestamp of path update when movement was paused */
	float PathTimeWhenPaused;

	/** Indicates a path node index at which precise "is at goal"
	 *	tests are going to be performed every frame, in regards
	 *	to acceptance radius */
	int32 PreciseAcceptanceRadiusCheckStartNodeIndex;

	/** increase acceptance radius with agent's radius */
	uint32 bReachTestIncludesAgentRadius : 1;

	/** increase acceptance radius with goal's radius */
	uint32 bReachTestIncludesGoalRadius : 1;

	/** if set, target location will be constantly updated to match goal actor while following last segment of full path */
	uint32 bMoveToGoalOnLastSegment : 1;

	/** if set, movement block detection will be used */
	uint32 bUseBlockDetection : 1;

	/** set when agent collides with goal actor */
	uint32 bCollidedWithGoal : 1;

	/** set when last move request was finished at goal */
	uint32 bLastMoveReachedGoal : 1;

	/** if set, movement will be stopped on finishing path */
	uint32 bStopMovementOnFinish : 1;

	/** if set, path following is using FMetaNavMeshPath */
	uint32 bIsUsingMetaPath : 1;

	/** gets set when agent starts following a navigation link. Cleared after agent starts falling or changes segment to a non-link one */
	uint32 bWalkingNavLinkStart : 1;

	/** True if pathfollowing is doing deceleration at the end of the path. @see FollowPathSegment(). */
	uint32 bIsDecelerating : 1;

	/** timeout for Waiting state, negative value = infinite */
	float WaitingTimeout;

	/** detect blocked movement when distance between center of location samples and furthest one (centroid radius) is below threshold */
	float BlockDetectionDistance;

	/** interval for collecting location samples */
	float BlockDetectionInterval;

	/** number of samples required for block detection */
	int32 BlockDetectionSampleCount;

	/** timestamp of last location sample */
	float LastSampleTime;

	/** index of next location sample in array */
	int32 NextSampleIdx;

	/** location samples for stuck detection */
	TArray<FBasedPosition> LocationSamples;

	/** index of path point being current move beginning */
	int32 MoveSegmentStartIndex;

	/** index of path point being current move target */
	int32 MoveSegmentEndIndex;

	/** reference of node at segment start */
	NavNodeRef MoveSegmentStartRef;

	/** reference of node at segment end */
	NavNodeRef MoveSegmentEndRef;

	/** direction of current move segment */
	FVector MoveSegmentDirection;

	/** braking distance for acceleration driven path following */
	float CachedBrakingDistance;

	/** max speed used for CachedBrakingDistance */
	float CachedBrakingMaxSpeed;

	/** index of path point for starting deceleration */
	int32 DecelerationSegmentIndex;

	/** reset path following data */
	virtual void Reset();

	/** should verify if agent if still on path ater movement has been resumed? */
	virtual bool ShouldCheckPathOnResume() const;

	/** sets variables related to current move segment */
	virtual void SetMoveSegment(int32 SegmentStartIndex);
	
	/** follow current path segment */
	virtual void FollowPathSegment(float DeltaTime);

	/** check state of path following, update move segment if needed */
	virtual void UpdatePathSegment();

	/** next path segment if custom nav link, try passing control to it */
	virtual void StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint);

	/** update blocked movement detection, @returns true if new sample was added */
	virtual bool UpdateBlockDetection();

	/** updates braking distance and deceleration segment */
	virtual void UpdateDecelerationData();

	/** check if move is completed */
	bool HasReachedDestination(const FVector& CurrentLocation) const;

	/** check if segment is completed */
	virtual bool HasReachedCurrentTarget(const FVector& CurrentLocation) const;

	/** check if moving agent has reached goal defined by cylinder */
	bool HasReachedInternal(const FVector& GoalLocation, float GoalRadius, float GoalHalfHeight, const FVector& AgentLocation, float RadiusThreshold, float AgentRadiusMultiplier) const;

	/** check if agent is on path */
	virtual bool IsOnPath() const;

	/** check if movement is blocked */
	bool IsBlocked() const;

	/** switch to next segment on path */
	FORCEINLINE void SetNextMoveSegment() { SetMoveSegment(GetNextPathIndex()); }

	/** assign new request Id */
	FORCEINLINE void StoreRequestId() { CurrentRequestId = UPathFollowingComponent::GetNextRequestId(); }

	FORCEINLINE static uint32 GetNextRequestId() { return NextRequestId++; }

	/** Checks if this PathFollowingComponent is already on path, and
	*	if so determines index of next path point
	*	@return what PathFollowingComponent thinks should be next path point. INDEX_NONE if given path is invalid
	*	@note this function does not set MoveSegmentEndIndex */
	virtual int32 DetermineStartingPathPoint(const FNavigationPath* ConsideredPath) const;

	/** @return index of path point, that should be target of current move segment */
	virtual int32 DetermineCurrentTargetPathPoint(int32 StartIndex);

	/** check if movement component is valid or tries to grab one from owner 
	 *	@param bForce results in looking for owner's movement component even if pointer to one is already cached */
	virtual bool UpdateMovementComponent(bool bForce = false);

	/** called after receiving update event from current path
	 *  @return false if path was not accepted and move request needs to be aborted */
	virtual bool HandlePathUpdateEvent();

	/** called from timer if component spends too much time in Waiting state */
	virtual void OnWaitingPathTimeout();

	/** clears Block Detection stored data effectively resetting the mechanism */
	void ResetBlockDetectionData();

	/** force creating new location sample for block detection */
	void ForceBlockDetectionUpdate();

	/** set move focus in AI owner */
	void UpdateMoveFocus();

	/** For given path finds a path node at which
	 *	PathfollowingComponent should start doing 
	 *	precise is-goal-in-acceptance-radius  tests */
	int32 FindPreciseAcceptanceRadiusTestsStartNodeIndex(const FNavigationPath& PathInstance, const FVector& GoalLocation) const;

	/** Based on Path's properties, original move goal location and requested AcceptanceRadius
	 *	this function calculates actual acceptance radius to apply when testing if the agent
	 *	has successfully reached requested goal's vicinity */
	float GetFinalAcceptanceRadius(const FNavigationPath& PathInstance, const FVector OriginalGoalLocation, const FVector* PathEndOverride = nullptr) const;

	/** debug point reach test values */
	void DebugReachTest(float& CurrentDot, float& CurrentDistance, float& CurrentHeight, uint8& bDotFailed, uint8& bDistanceFailed, uint8& bHeightFailed) const;

	/** called when NavigationSystem finishes initial navigation data registration.
	 *	This is usually required by AI agents hand-placed on levels to find MyNavData */
	virtual void OnNavigationInitDone();

	/** called when NavigationSystem registers new navigation data type while this component
	 *	instance has empty MyNavData. This is usually the case for AI agents hand-placed
	 *	on levels. */
	UFUNCTION()
	void OnNavDataRegistered(ANavigationData* NavData);


	/** used to keep track of which subsystem requested this AI resource be locked */
	FAIResourceLock ResourceLock;

	/** timer handle for OnWaitingPathTimeout function */
	FTimerHandle WaitingForPathTimer;

private:

	/** used for debugging purposes to be able to identify which logged information
	 *	results from which request, if there was multiple ones during one frame */
	static uint32 NextRequestId;
	FAIRequestID CurrentRequestId;

	/** Current location on navigation data.  Lazy-updated, so read this via GetCurrentNavLocation(). 
	 *	Since it makes conceptual sense for GetCurrentNavLocation() to be const but we may 
	 *	need to update the cached value, CurrentNavLocation is mutable. */
	mutable FNavLocation CurrentNavLocation;

	/** DEPRECATED, use bReachTestIncludesAgentRadius instead */
	uint32 bStopOnOverlap : 1;

public:
	/** special float constant to symbolize "use default value". This does not contain 
	 *	value to be used, it's used to detect the fact that it's requested, and 
	 *	appropriate value from querier/doer will be pulled */
	static const float DefaultAcceptanceRadius;
};
