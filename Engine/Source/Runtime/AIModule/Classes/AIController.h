// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Templates/SubclassOf.h"
#include "EngineDefines.h"
#include "AI/Navigation/NavFilters/NavigationQueryFilter.h"
#include "AITypes.h"
#include "GameplayTaskOwnerInterface.h"
#include "GameplayTask.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionListenerInterface.h"
#include "GenericTeamAgentInterface.h"
#include "VisualLogger/VisualLoggerDebugSnapshotInterface.h"
#include "AIController.generated.h"

class FDebugDisplayInfo;
class UAIPerceptionComponent;
class UBehaviorTree;
class UBlackboardComponent;
class UBlackboardData;
class UBrainComponent;
class UCanvas;
class UGameplayTaskResource;
class UGameplayTasksComponent;
class UPawnAction;
class UPawnActionsComponent;
struct FVisualLogEntry;

#if ENABLE_VISUAL_LOG
struct FVisualLogEntry;
#endif // ENABLE_VISUAL_LOG
struct FPathFindingQuery;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAIMoveCompletedSignature, FAIRequestID, RequestID, EPathFollowingResult::Type, Result);

// the reason for this being namespace instead of a regular enum is
// so that it can be expanded in game-specific code
// @todo this is a bit messy, needs to be refactored
namespace EAIFocusPriority
{
	typedef uint8 Type;

	const Type Default = 0;
	const Type Move = 1;
	const Type Gameplay = 2;

	const Type LastFocusPriority = Gameplay;
}

struct FFocusKnowledge
{
	struct FFocusItem
	{
		TWeakObjectPtr<AActor> Actor;
		FVector Position;

		FFocusItem()
		{
			Actor = nullptr;
			Position = FAISystem::InvalidLocation;
		}
	};
	
	TArray<FFocusItem> Priorities;
};

//~=============================================================================
/**
 * AIController is the base class of controllers for AI-controlled Pawns.
 * 
 * Controllers are non-physical actors that can be attached to a pawn to control its actions.
 * AIControllers manage the artificial intelligence for the pawns they control.
 * In networked games, they only exist on the server.
 *
 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Framework/Controller/
 */

UCLASS(ClassGroup = AI, BlueprintType, Blueprintable)
class AIMODULE_API AAIController : public AController, public IAIPerceptionListenerInterface, public IGameplayTaskOwnerInterface, public IGenericTeamAgentInterface, public IVisualLoggerDebugSnapshotInterface
{
	GENERATED_BODY()

	FGameplayResourceSet ScriptClaimedResources;
protected:
	FFocusKnowledge	FocusInformation;

	/** By default AI's logic gets stopped when controlled Pawn is unpossesed. Setting this flag to false
	 *	will make AI logic persist past loosing controll over a pawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	uint32 bStopAILogicOnUnposses : 1;

public:
	/** used for alternating LineOfSight traces */
	UPROPERTY()
	mutable uint32 bLOSflag : 1;

	/** Skip extra line of sight traces to extremities of target being checked. */
	UPROPERTY()
	uint32 bSkipExtraLOSChecks : 1;

	/** Is strafing allowed during movement? */
	UPROPERTY()
	uint32 bAllowStrafe : 1;

	/** Specifies if this AI wants its own PlayerState. */
	UPROPERTY()
	uint32 bWantsPlayerState : 1;

	/** Copy Pawn rotation to ControlRotation, if there is no focus point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	uint32 bSetControlRotationFromPawnOrientation:1;

private:

	/** Component used for moving along a path. */
	UPROPERTY(VisibleDefaultsOnly, Category = AI)
	UPathFollowingComponent* PathFollowingComponent;

public:

	/** Component responsible for behaviors. */
	UPROPERTY(BlueprintReadWrite, Category = AI)
	UBrainComponent* BrainComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = AI)
	UAIPerceptionComponent* PerceptionComponent;

private:
	UPROPERTY(BlueprintReadOnly, Category = AI, meta = (AllowPrivateAccess = "true"))
	UPawnActionsComponent* ActionsComp;

protected:
	/** blackboard */
	UPROPERTY(BlueprintReadOnly, Category = AI, meta = (AllowPrivateAccess = "true"))
	UBlackboardComponent* Blackboard;

	UPROPERTY()
	UGameplayTasksComponent* CachedGameplayTasksComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	TSubclassOf<UNavigationQueryFilter> DefaultNavigationFilterClass;

public:

	AAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Event called when PossessedPawn is possessed by this controller. */
	UFUNCTION(BlueprintImplementableEvent)
	void OnPossess(APawn* PossessedPawn);

	/** Gets triggered after given pawn has been unpossesed */
	UFUNCTION(BlueprintImplementableEvent)
	void OnUnpossess(APawn* UnpossessedPawn);

	virtual void SetPawn(APawn* InPawn) override;

	/** Makes AI go toward specified Goal actor (destination will be continuously updated), aborts any active path following
	 *  @param AcceptanceRadius - finish move if pawn gets close enough
	 *  @param bStopOnOverlap - add pawn's radius to AcceptanceRadius
	 *  @param bUsePathfinding - use navigation data to calculate path (otherwise it will go in straight line)
	 *  @param bCanStrafe - set focus related flag: bAllowStrafe
	 *  @param FilterClass - navigation filter for pathfinding adjustments. If none specified DefaultNavigationFilterClass will be used
	 *  @param bAllowPartialPath - use incomplete path when goal can't be reached
	 *	@note AcceptanceRadius has default value or -1 due to Header Parser not being able to recognize UPathFollowingComponent::DefaultAcceptanceRadius
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation", Meta = (AdvancedDisplay = "bStopOnOverlap,bCanStrafe,bAllowPartialPath"))
	EPathFollowingRequestResult::Type MoveToActor(AActor* Goal, float AcceptanceRadius = -1, bool bStopOnOverlap = true,
		bool bUsePathfinding = true, bool bCanStrafe = true,
		TSubclassOf<UNavigationQueryFilter> FilterClass = NULL, bool bAllowPartialPath = true);

	/** Makes AI go toward specified Dest location, aborts any active path following
	 *  @param AcceptanceRadius - finish move if pawn gets close enough
	 *  @param bStopOnOverlap - add pawn's radius to AcceptanceRadius
	 *  @param bUsePathfinding - use navigation data to calculate path (otherwise it will go in straight line)
	 *  @param bProjectDestinationToNavigation - project location on navigation data before using it
	 *  @param bCanStrafe - set focus related flag: bAllowStrafe
	 *  @param FilterClass - navigation filter for pathfinding adjustments. If none specified DefaultNavigationFilterClass will be used
	 *  @param bAllowPartialPath - use incomplete path when goal can't be reached
	 *	@note AcceptanceRadius has default value or -1 due to Header Parser not being able to recognize UPathFollowingComponent::DefaultAcceptanceRadius
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation", Meta = (AdvancedDisplay = "bStopOnOverlap,bCanStrafe,bAllowPartialPath"))
	EPathFollowingRequestResult::Type MoveToLocation(const FVector& Dest, float AcceptanceRadius = -1, bool bStopOnOverlap = true,
		bool bUsePathfinding = true, bool bProjectDestinationToNavigation = false, bool bCanStrafe = true,
		TSubclassOf<UNavigationQueryFilter> FilterClass = NULL, bool bAllowPartialPath = true);

	/** Makes AI go toward specified destination
	 *  @param MoveRequest - details about move
	 *  @param OutPath - optional output param, filled in with assigned path
	 *  @return struct holding MoveId and enum code
	 */
	virtual FPathFollowingRequestResult MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath = nullptr);

	/** Passes move request and path object to path following */
	virtual FAIRequestID RequestMove(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr Path);

	/** Finds path for given move request
 	 *  @param MoveRequest - details about move
	 *  @param Query - pathfinding query for navigation system
	 *  @param OutPath - generated path
	 */
	virtual void FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query, FNavPathSharedPtr& OutPath) const;

	/** Helper function for creating pathfinding query for this agent from move request data */
	bool BuildPathfindingQuery(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query) const;

	DEPRECATED_FORGAME(4.13, "This function is now deprecated, please use FindPathForMoveRequest() for adjusting Query or BuildPathfindingQuery() for getting one.")
	virtual bool PreparePathfinding(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query);

	DEPRECATED_FORGAME(4.13, "This function is now deprecated, please use FindPathForMoveRequest() for adjusting pathfinding or path postprocess.")
	virtual FAIRequestID RequestPathAndMove(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query);

	/** if AI is currently moving due to request given by RequestToPause, then the move will be paused */
	bool PauseMove(FAIRequestID RequestToPause);

	/** resumes last AI-performed, paused request provided it's ID was equivalent to RequestToResume */
	bool ResumeMove(FAIRequestID RequestToResume);

	/** Aborts the move the controller is currently performing */
	virtual void StopMovement() override;

	/** Called on completing current movement request */
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);

	DEPRECATED_FORGAME(4.13, "This function is now deprecated, please use version with EPathFollowingResultDetails parameter.")
	virtual void OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

	/** Returns the Move Request ID for the current move */
	FORCEINLINE FAIRequestID GetCurrentMoveRequestID() const { return GetPathFollowingComponent() ? GetPathFollowingComponent()->GetCurrentRequestId() : FAIRequestID::InvalidRequest; }

	/** Blueprint notification that we've completed the current movement request */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "MoveCompleted"))
	FAIMoveCompletedSignature ReceiveMoveCompleted;

	TSubclassOf<UNavigationQueryFilter> GetDefaultNavigationFilterClass() const { return DefaultNavigationFilterClass; }

	/** Returns status of path following */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	EPathFollowingStatus::Type GetMoveStatus() const;

	/** Returns true if the current PathFollowingComponent's path is partial (does not reach desired destination). */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	bool HasPartialPath() const;

	/** Returns position of current path segment's end. */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	FVector GetImmediateMoveDestination() const;

	/** Updates state of movement block detection. */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void SetMoveBlockDetection(bool bEnable);

	/** Prepares path finding and path following components. */
	virtual void InitNavigationControl(UPathFollowingComponent*& PathFollowingComp) override;

	/** Starts executing behavior tree. */
	UFUNCTION(BlueprintCallable, Category = "AI")
	virtual bool RunBehaviorTree(UBehaviorTree* BTAsset);

	/**
	 * Makes AI use the specified Blackboard asset & creates a Blackboard Component if one does not already exist.
	 * @param	BlackboardAsset			The Blackboard asset to use.
	 * @param	BlackboardComponent		The Blackboard component that was used or created to work with the passed-in Blackboard Asset.
	 * @return true if we successfully linked the blackboard asset to the blackboard component.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI")
	bool UseBlackboard(UBlackboardData* BlackboardAsset, UBlackboardComponent*& BlackboardComponent);

	/** does this AIController allow given UBlackboardComponent sync data with it */
	virtual bool ShouldSyncBlackboardWith(const UBlackboardComponent& OtherBlackboardComponent) const;

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks")
	void ClaimTaskResource(TSubclassOf<UGameplayTaskResource> ResourceClass);
	
	UFUNCTION(BlueprintCallable, Category = "AI|Tasks")
	void UnclaimTaskResource(TSubclassOf<UGameplayTaskResource> ResourceClass);

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void OnUsingBlackBoard(UBlackboardComponent* BlackboardComp, UBlackboardData* BlackboardAsset);

	virtual bool InitializeBlackboard(UBlackboardComponent& BlackboardComp, UBlackboardData& BlackboardAsset);

public:
	/** Retrieve the final position that controller should be looking at. */
	UFUNCTION(BlueprintCallable, Category = "AI")
	FVector GetFocalPoint() const;

	FVector GetFocalPointForPriority(EAIFocusPriority::Type InPriority) const;

	/** Retrieve the focal point this controller should focus to on given actor. */
	UFUNCTION(BlueprintCallable, Category = "AI")
	virtual FVector GetFocalPointOnActor(const AActor *Actor) const;

	/** Set the position that controller should be looking at. */
	UFUNCTION(BlueprintCallable, Category = "AI", meta = (DisplayName = "SetFocalPoint", Keywords = "focus"))
	void K2_SetFocalPoint(FVector FP);

	/** Set Focus for actor, will set FocalPoint as a result. */
	UFUNCTION(BlueprintCallable, Category = "AI", meta = (DisplayName = "SetFocus"))
	void K2_SetFocus(AActor* NewFocus);

	/** Get the focused actor. */
	UFUNCTION(BlueprintCallable, Category = "AI")
	AActor* GetFocusActor() const;

	FORCEINLINE AActor* GetFocusActorForPriority(EAIFocusPriority::Type InPriority) const { return FocusInformation.Priorities.IsValidIndex(InPriority) ? FocusInformation.Priorities[InPriority].Actor.Get() : nullptr; }

	/** Clears Focus, will also clear FocalPoint as a result */
	UFUNCTION(BlueprintCallable, Category = "AI", meta = (DisplayName = "ClearFocus"))
	void K2_ClearFocus();


	/**
	 * Computes a launch velocity vector to toss a projectile and hit the given destination.
	 * Performance note: Potentially expensive. Nonzero CollisionRadius and bOnlyTraceUp=false are the more expensive options.
	 *
	 * @param OutTossVelocity - out param stuffed with the computed velocity to use
	 * @param Start - desired start point of arc
	 * @param End - desired end point of arc
	 * @param TossSpeed - Initial speed of the theoretical projectile. Assumed to only change due to gravity for the entire lifetime of the projectile
	 * @param CollisionSize (optional) - is the size of bounding box of the tossed actor (defaults to (0,0,0)
	 * @param bOnlyTraceUp  (optional) - when true collision checks verifying the arc will only be done along the upward portion of the arc
	 * @return - true if a valid arc was computed, false if no valid solution could be found
	 */
	bool SuggestTossVelocity(FVector& OutTossVelocity, FVector Start, FVector End, float TossSpeed, bool bPreferHighArc, float CollisionRadius = 0, bool bOnlyTraceUp = false);

	//~ Begin AActor Interface
	virtual void Tick(float DeltaTime) override;
	virtual void PostInitializeComponents() override;
	virtual void PostRegisterAllComponents() override;
	//~ End AActor Interface

	//~ Begin AController Interface
	virtual void Possess(APawn* InPawn) override;
	virtual void UnPossess() override;
	virtual bool ShouldPostponePathUpdates() const override;
	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;

#if ENABLE_VISUAL_LOG
	virtual void GrabDebugSnapshot(FVisualLogEntry* Snapshot) const override;
#endif

	virtual void Reset() override;

	/**
	 * Checks line to center and top of other actor
	 * @param Other is the actor whose visibility is being checked.
	 * @param ViewPoint is eye position visibility is being checked from.  If vect(0,0,0) passed in, uses current viewtarget's eye position.
	 * @param bAlternateChecks used only in AIController implementation
	 * @return true if controller's pawn can see Other actor.
	 */
	virtual bool LineOfSightTo(const AActor* Other, FVector ViewPoint = FVector(ForceInit), bool bAlternateChecks = false) const override;
	//~ End AController Interface

	/** Notifies AIController of changes in given actors' perception */
	virtual void ActorsPerceptionUpdated(const TArray<AActor*>& UpdatedActors);

	/** Update direction AI is looking based on FocalPoint */
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true);

	/** Set FocalPoint for given priority as absolute position or offset from base. */
	virtual void SetFocalPoint(FVector NewFocus, EAIFocusPriority::Type InPriority = EAIFocusPriority::Gameplay);

	/* Set Focus actor for given priority, will set FocalPoint as a result. */
	virtual void SetFocus(AActor* NewFocus, EAIFocusPriority::Type InPriority = EAIFocusPriority::Gameplay);

	/** Clears Focus for given priority, will also clear FocalPoint as a result
	 *	@param InPriority focus priority to clear. If you don't know what to use you probably mean EAIFocusPriority::Gameplay*/
	virtual void ClearFocus(EAIFocusPriority::Type InPriority);

	void SetPerceptionComponent(UAIPerceptionComponent& InPerceptionComponent);
	//----------------------------------------------------------------------//
	// IAIPerceptionListenerInterface
	//----------------------------------------------------------------------//
	virtual UAIPerceptionComponent* GetPerceptionComponent() override { return GetAIPerceptionComponent(); }

	//----------------------------------------------------------------------//
	// INavAgentInterface
	//----------------------------------------------------------------------//
	virtual bool IsFollowingAPath() const override;

	//----------------------------------------------------------------------//
	// IGenericTeamAgentInterface
	//----------------------------------------------------------------------//
private:
	FGenericTeamId TeamID;
public:
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	virtual FGenericTeamId GetGenericTeamId() const override { return TeamID; }

	//----------------------------------------------------------------------//
	// IGameplayTaskOwnerInterface
	//----------------------------------------------------------------------//
	virtual UGameplayTasksComponent* GetGameplayTasksComponent(const UGameplayTask& Task) const override { return GetGameplayTasksComponent(); }
	virtual AActor* GetGameplayTaskOwner(const UGameplayTask* Task) const override { return const_cast<AAIController*>(this); }
	virtual AActor* GetGameplayTaskAvatar(const UGameplayTask* Task) const override { return GetPawn(); }
	virtual uint8 GetGameplayTaskDefaultPriority() const { return FGameplayTasks::DefaultPriority - 1; }

	FORCEINLINE UGameplayTasksComponent* GetGameplayTasksComponent() const { return CachedGameplayTasksComponent; }

	// add empty overrides to fix linker errors if project implements a child class without adding GameplayTasks module dependency
	virtual void OnGameplayTaskInitialized(UGameplayTask& Task) override {}
	virtual void OnGameplayTaskActivated(UGameplayTask& Task) override {}
	virtual void OnGameplayTaskDeactivated(UGameplayTask& Task) override {}

	UFUNCTION()
	virtual void OnGameplayTaskResourcesClaimed(FGameplayResourceSet NewlyClaimed, FGameplayResourceSet FreshlyReleased);

	//----------------------------------------------------------------------//
	// Actions
	//----------------------------------------------------------------------//
	bool PerformAction(UPawnAction& Action, EAIRequestPriority::Type Priority, UObject* const Instigator = NULL);

	//----------------------------------------------------------------------//
	// debug/dev-time 
	//----------------------------------------------------------------------//
	virtual FString GetDebugIcon() const;
	
	// Cheat/debugging functions
	static void ToggleAIIgnorePlayers() { bAIIgnorePlayers = !bAIIgnorePlayers; }
	static bool AreAIIgnoringPlayers() { return bAIIgnorePlayers; }

	/** If true, AI controllers will ignore players. */
	static bool bAIIgnorePlayers;

public:
	/** Returns PathFollowingComponent subobject **/
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	UPathFollowingComponent* GetPathFollowingComponent() const { return PathFollowingComponent; }
	/** Returns ActionsComp subobject **/
	UPawnActionsComponent* GetActionsComp() const { return ActionsComp; }
	UFUNCTION(BlueprintPure, Category = "AI|Perception")
	UAIPerceptionComponent* GetAIPerceptionComponent() { return PerceptionComponent; }

	const UAIPerceptionComponent* GetAIPerceptionComponent() const { return PerceptionComponent; }

	UBrainComponent* GetBrainComponent() const { return BrainComponent; }
	const UBlackboardComponent* GetBlackboardComponent() const { return Blackboard; }
	UBlackboardComponent* GetBlackboardComponent() { return Blackboard; }
};

//----------------------------------------------------------------------//
// forceinlines
//----------------------------------------------------------------------//
namespace FAISystem
{
	FORCEINLINE bool IsValidControllerAndHasValidPawn(const AController* Controller)
	{
		return Controller != nullptr && Controller->IsPendingKillPending() == false
			&& Controller->GetPawn() != nullptr && Controller->GetPawn()->IsPendingKillPending() == false;
	}
}
