// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "AITypes.h"
#include "BrainComponent.h"
#include "PawnAction.generated.h"

class AController;
class APawn;
class UPawnAction;
class UPawnActionsComponent;
struct FPawnActionStack;

UENUM()
namespace EPawnSubActionTriggeringPolicy
{
	enum Type
	{
		CopyBeforeTriggering,
		ReuseInstances,
	};
}

AIMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogPawnAction, Warning, All);
DECLARE_DELEGATE_TwoParams(FPawnActionEventDelegate, UPawnAction&, EPawnActionEventType::Type);

UENUM()
namespace EPawnActionFailHandling
{
	enum Type
	{
		RequireSuccess,
		IgnoreFailure
	};
}

/** 
 *	Things to remember:
 *	* Actions are created paused
 */
UCLASS(abstract, EditInlineNew)
class AIMODULE_API UPawnAction : public UObject
{
	GENERATED_UCLASS_BODY()

	friend UPawnActionsComponent;
	friend FPawnActionStack;

private:
	/** Current child node executing on top of this Action */
	UPROPERTY(Transient)
	UPawnAction* ChildAction;

	UPROPERTY(Transient)
	UPawnAction* ParentAction;

	/** Extra reference to the component this action is being governed by */
	UPROPERTY(Transient)
	UPawnActionsComponent* OwnerComponent;
	
	/** indicates an object that caused this action. Used for mass removal of actions 
	 *	by specific object */
	UPROPERTY(Transient)
	UObject* Instigator;

protected:
	/** @Note: THIS IS HERE _ONLY_ BECAUSE OF THE WAY AI MESSAGING IS CURRENTLY IMPLEMENTED. WILL GO AWAY! */
	UPROPERTY(Transient)
	UBrainComponent* BrainComp;

private:
	/** stores registered message observers */
	TArray<FAIMessageObserverHandle> MessageHandlers;

	EAIRequestPriority::Type ExecutionPriority;

	FPawnActionEventDelegate ActionObserver;

protected:

	FAIRequestID RequestID;

	/** specifies which resources will be locked by this action. */
	FAIResourcesSet RequiredResources;

	/** if this is FALSE and we're trying to push a new instance of a given class,
	 *	but the top of the stack is already an instance of that class ignore the attempted push */
	UPROPERTY(Category = PawnAction, EditDefaultsOnly, BlueprintReadOnly)
	uint32 bAllowNewSameClassInstance : 1;

	/** if this is TRUE, when we try to push a new instance of an action who has the
	 *	same class as the action on the top of the stack, pop the one on the stack, and push the new one
	 *	NOTE: This trumps bAllowNewClassInstance (e.g. if this is true and bAllowNewClassInstance
	 *	is false the active instance will still be replaced) */
	UPROPERTY(Category = PawnAction, EditDefaultsOnly, BlueprintReadWrite)
	uint32 bReplaceActiveSameClassInstance : 1;

	/** this is a temporary solution to allow having movement action running in background while there's 
	 *	another action on top doing its thing
	 *	@note should go away once AI resource locking comes on-line */
	UPROPERTY(Category = PawnAction, EditDefaultsOnly, BlueprintReadWrite)
	uint32 bShouldPauseMovement : 1;

	/** if set, action will call OnFinished notify even when ending as FailedToStart */
	UPROPERTY(Category = PawnAction, EditDefaultsOnly, BlueprintReadWrite, AdvancedDisplay)
	uint32 bAlwaysNotifyOnFinished : 1;

private:
	/** indicates whether action is in the process of abortion, and if so on what state */
	EPawnActionAbortState::Type AbortState;
	
	EPawnActionResult::Type FinishResult;
	
	/** Used exclusively for action events sorting */
	int32 IndexOnStack;
	
	/** Indicates the action has been paused */
	uint32 bPaused : 1;

	uint32 bHasBeenStarted : 1;

	/** set to true when action fails the initial Start call */
	uint32 bFailedToStart : 1;

protected:
	/** TickAction will get called only if this flag is set. To be set in derived action's
	 *	constructor. 
	 *	@NOTE Toggling at runtime is not supported */
	uint32 bWantsTick : 1;

public:

	// Begin UObject
	virtual UWorld* GetWorld() const override;
	// End UObject

	FORCEINLINE const UPawnAction* GetParentAction() const { return ParentAction; }
	FORCEINLINE const UPawnAction* GetChildAction() const { return ChildAction; }
	FORCEINLINE UPawnAction* GetChildAction() { return ChildAction; }
	FORCEINLINE bool IsPaused() const { return !!bPaused; }
	FORCEINLINE bool IsActive() const { return FinishResult == EPawnActionResult::InProgress && IsPaused() == false && AbortState == EPawnActionAbortState::NotBeingAborted; }
	FORCEINLINE bool IsBeingAborted() const { return AbortState != EPawnActionAbortState::NotBeingAborted; }
	FORCEINLINE bool IsFinished() const { return FinishResult > EPawnActionResult::InProgress; }
	FORCEINLINE bool WantsTick() const { return bWantsTick; }

	FORCEINLINE bool ShouldPauseMovement() const { return bShouldPauseMovement; }

protected:
	FORCEINLINE void TickAction(float DeltaTime)
	{ 
		// tick ChildAction 
		if (ChildAction != NULL)
		{
			ChildAction->Tick(DeltaTime);
		}
		// or self if not paused
		else if (!!bWantsTick && IsPaused() == false)
		{
			Tick(DeltaTime);
		}
	}

	/** triggers aborting of an Action
	 *	@param bForce
	 *	@return current state of task abort
	 *	@NOTE do not make this virtual! Contains some essential logic. */
	EPawnActionAbortState::Type Abort(EAIForceParam::Type ShouldForce = EAIForceParam::DoNotForce);
	
	FORCEINLINE UPawnActionsComponent* GetOwnerComponent() { return OwnerComponent; }
public:
	FORCEINLINE EAIRequestPriority::Type GetPriority() const { return ExecutionPriority; }
	FORCEINLINE EPawnActionResult::Type GetResult() const { return FinishResult; }
	FORCEINLINE EPawnActionAbortState::Type GetAbortState() const { return AbortState; }
	FORCEINLINE UPawnActionsComponent* GetOwnerComponent() const { return OwnerComponent; }
	FORCEINLINE UObject* GetInstigator() const { return Instigator; }
	APawn* GetPawn() const;
	AController* GetController() const;

	template<class TActionClass>
	static TActionClass* CreateActionInstance(UWorld& World)
	{
		TSubclassOf<UPawnAction> ActionClass = TActionClass::StaticClass();
		return NewObject<TActionClass>(&World, ActionClass);
	}

	//----------------------------------------------------------------------//
	// messaging
	//----------------------------------------------------------------------//
	void WaitForMessage(FName MessageType, FAIRequestID RequestID = FAIRequestID::AnyRequest);
	// @note this function will change its signature once AI messaging is rewritten @todo
	virtual void HandleAIMessage(UBrainComponent*, const FAIMessage&){};

	void SetActionObserver(const FPawnActionEventDelegate& InActionObserver) { ActionObserver = InActionObserver; }
	bool HasActionObserver() const { return ActionObserver.IsBound(); }

	//----------------------------------------------------------------------//
	// Blueprint interface
	//----------------------------------------------------------------------//
	UFUNCTION(BlueprintPure, Category = "AI|PawnActions")
	TEnumAsByte<EAIRequestPriority::Type> GetActionPriority();

	UFUNCTION(BlueprintCallable, Category = "AI|PawnActions", meta = (WorldContext="WorldContextObject"))
	static UPawnAction* CreateActionInstance(UObject* WorldContextObject, TSubclassOf<UPawnAction> ActionClass);

	//----------------------------------------------------------------------//
	// debug
	//----------------------------------------------------------------------//
	FString GetStateDescription() const;
	FString GetPriorityName() const;
	virtual FString GetDisplayName() const;

protected:

	/** starts or resumes action, depending on internal state */
	bool Activate();
	void OnPopped();

	UFUNCTION(BlueprintCallable, Category = "AI|PawnActions")
	virtual void Finish(TEnumAsByte<EPawnActionResult::Type> WithResult);

	void SendEvent(EPawnActionEventType::Type Event);

	void StopWaitingForMessages();

	void SetOwnerComponent(UPawnActionsComponent* Component);

	void SetInstigator(UObject* const InInstigator);

	virtual void Tick(float DeltaTime);

	/** called to start off the Action
	 *	@return 'true' if actions successfully started. 
	 *	@NOTE if action fails to start no finishing or aborting mechanics will be triggered */
	virtual bool Start();
	/** called to pause action when higher priority or child action kicks in */
	virtual bool Pause(const UPawnAction* PausedBy);
	/** called to resume action after being paused */
	virtual bool Resume();
	/** called when this action is being removed from action stacks */
	virtual void OnFinished(EPawnActionResult::Type WithResult);
	/** called to give Action chance to react to child action finishing.
	 *	@NOTE gets called _AFTER_ child's OnFinished to give child action chance 
	 *		to prepare "finishing data" for parent to read. 
	 *	@NOTE clears parent-child binding */
	virtual void OnChildFinished(UPawnAction& Action, EPawnActionResult::Type WithResult);

	/** apart from doing regular push request copies additional values from Parent, like Priority and Instigator */
	bool PushChildAction(UPawnAction& Action);
	
	/** performs actual work on aborting Action. Should be called exclusively by Abort function
	 *	@return only valid return values here are LatendAbortInProgress and AbortDone */
	virtual EPawnActionAbortState::Type PerformAbort(EAIForceParam::Type ShouldForce) { return EPawnActionAbortState::AbortDone; }

	FORCEINLINE bool HasBeenStarted() const { return AbortState != EPawnActionAbortState::NeverStarted; }

private:
	/** called when this action is put on a stack. Does not indicate action will be started soon
	 *	(it depends on other actions on other action stacks. Called before Start() call */
	void OnPushed();

	/** Sets final result for this Action. To be called only once upon Action's finish */
	void SetFinishResult(EPawnActionResult::Type Result);

	// do not un-private. Internal logic only!
	void SetAbortState(EPawnActionAbortState::Type NewAbortState);
};


//----------------------------------------------------------------------//
// Blueprint inlines
//----------------------------------------------------------------------//
FORCEINLINE TEnumAsByte<EAIRequestPriority::Type> UPawnAction::GetActionPriority()
{
	return ExecutionPriority;
}
