// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"
#include "Components/ActorComponent.h"
#include "EngineDefines.h"
#include "AITypes.h"
#include "AIResourceInterface.h"
#include "BrainComponent.generated.h"

class AAIController;
class AController;
class APawn;
class UBlackboardComponent;
class UBrainComponent;
struct FAIMessage;
struct FAIMessageObserver;

DECLARE_DELEGATE_TwoParams(FOnAIMessage, UBrainComponent*, const FAIMessage&);

DECLARE_LOG_CATEGORY_EXTERN(LogBrain, Warning, All);

struct AIMODULE_API FAIMessage
{
	enum EStatus
	{
		Failure,
		Success,
	};

	/** type of message */
	FName MessageName;

	/** message source */
	FWeakObjectPtr Sender;

	/** message param: ID */
	FAIRequestID RequestID;

	/** message param: status */
	TEnumAsByte<EStatus> Status;

	/** message param: custom flags */
	uint8 MessageFlags;

	FAIMessage() : MessageName(NAME_None), Sender(NULL), RequestID(0), Status(FAIMessage::Success), MessageFlags(0) {}
	FAIMessage(FName InMessage, UObject* InSender) : MessageName(InMessage), Sender(InSender), RequestID(0), Status(FAIMessage::Success), MessageFlags(0) {}
	FAIMessage(FName InMessage, UObject* InSender, FAIRequestID InID, EStatus InStatus) : MessageName(InMessage), Sender(InSender), RequestID(InID), Status(InStatus), MessageFlags(0) {}
	FAIMessage(FName InMessage, UObject* InSender, FAIRequestID InID, bool bSuccess) : MessageName(InMessage), Sender(InSender), RequestID(InID), Status(bSuccess ? Success : Failure), MessageFlags(0) {}
	FAIMessage(FName InMessage, UObject* InSender, EStatus InStatus) : MessageName(InMessage), Sender(InSender), RequestID(0), Status(InStatus), MessageFlags(0) {}
	FAIMessage(FName InMessage, UObject* InSender, bool bSuccess) : MessageName(InMessage), Sender(InSender), RequestID(0), Status(bSuccess ? Success : Failure), MessageFlags(0) {}

	void SetFlags(uint8 Flags) { MessageFlags = Flags; }
	void SetFlag(uint8 Flag) { MessageFlags |= Flag; }
	void ClearFlag(uint8 Flag) { MessageFlags &= ~Flag; }
	bool HasFlag(uint8 Flag) const { return (MessageFlags & Flag) != 0; }

	static void Send(AController* Controller, const FAIMessage& Message);
	static void Send(APawn* Pawn, const FAIMessage& Message);
	static void Send(UBrainComponent* BrainComp, const FAIMessage& Message);

	static void Broadcast(UObject* WorldContextObject, const FAIMessage& Message);
};

typedef TSharedPtr<struct FAIMessageObserver, ESPMode::Fast> FAIMessageObserverHandle;

struct AIMODULE_API FAIMessageObserver : public TSharedFromThis<FAIMessageObserver>
{
public:
	FAIMessageObserver();

	static FAIMessageObserverHandle Create(AController* Controller, FName MessageType, FOnAIMessage const& Delegate);
	static FAIMessageObserverHandle Create(AController* Controller, FName MessageType, FAIRequestID MessageID, FOnAIMessage const& Delegate);

	static FAIMessageObserverHandle Create(APawn* Pawn, FName MessageType, FOnAIMessage const& Delegate);
	static FAIMessageObserverHandle Create(APawn* Pawn, FName MessageType, FAIRequestID MessageID, FOnAIMessage const& Delegate);

	static FAIMessageObserverHandle Create(UBrainComponent* BrainComp, FName MessageType, FOnAIMessage const& Delegate);
	static FAIMessageObserverHandle Create(UBrainComponent* BrainComp, FName MessageType, FAIRequestID MessageID, FOnAIMessage const& Delegate);

	~FAIMessageObserver();

	void OnMessage(const FAIMessage& Message);
	FString DescribeObservedMessage() const;
	
	FORCEINLINE FName GetObservedMessageType() const { return MessageType; }
	FORCEINLINE FAIRequestID GetObservedMessageID() const { return MessageID; }
	FORCEINLINE bool IsObservingMessageID() const { return bFilterByID; }

private:

	void Register(UBrainComponent* OwnerComp);
	void Unregister();

	/** observed message type */
	FName MessageType;

	/** filter: message ID */
	FAIRequestID MessageID;
	bool bFilterByID;

	/** delegate to call */
	FOnAIMessage ObserverDelegate;

	/** brain component owning this observer */
	TWeakObjectPtr<UBrainComponent> Owner;

	// Non-copyable
	FAIMessageObserver(const FAIMessageObserver&);
	FAIMessageObserver& operator=(const FAIMessageObserver&);
};

UCLASS(BlueprintType)
class AIMODULE_API UBrainComponent : public UActorComponent, public IAIResourceInterface
{
	GENERATED_UCLASS_BODY()

protected:
	/** blackboard component */
	UPROPERTY(transient)
	UBlackboardComponent* BlackboardComp;

	UPROPERTY(transient)
	AAIController* AIOwner;

	// @TODO this is a temp contraption to implement delayed messages delivering
	// until proper AI messaging is implemented
	TArray<FAIMessage> MessagesToProcess;

public:
	virtual FString GetDebugInfoString() const { return TEXT(""); }

	/** To be called in case we want to restart AI logic while it's still being locked.
	 *	On subsequent ResumeLogic instead RestartLogic will be called. 
	 *	@note this call does nothing if logic is not locked at the moment of call */
	void RequestLogicRestartOnUnlock();

	UFUNCTION(BlueprintCallable, Category = "AI|Logic")
	virtual void RestartLogic();

	UFUNCTION(BlueprintCallable, Category = "AI|Logic")
	virtual void StopLogic(const FString& Reason);

	/** AI logic won't be needed anymore, stop all activity and run cleanup */
	virtual void Cleanup() {}
	virtual void PauseLogic(const FString& Reason) {}
	/** MUST be called by child implementations!
	 *	@return indicates whether child class' ResumeLogic should be called (true) or has it been 
	 *	handled in a different way and no other actions are required (false)*/
	virtual EAILogicResuming::Type ResumeLogic(const FString& Reason);
public:

	UFUNCTION(BlueprintPure, Category = "AI|Logic")
	virtual bool IsRunning() const;

	UFUNCTION(BlueprintPure, Category = "AI|Logic")
	virtual bool IsPaused() const;

#if ENABLE_VISUAL_LOG
	virtual void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const;
#endif // ENABLE_VISUAL_LOG
	
	// IAIResourceInterface begin
	virtual void LockResource(EAIRequestPriority::Type LockSource) override;
	virtual void ClearResourceLock(EAIRequestPriority::Type LockSource) override;
	virtual void ForceUnlockResource() override;
	virtual bool IsResourceLocked() const override;
	// IAIResourceInterface end
	
	virtual void HandleMessage(const FAIMessage& Message);
	
	/** BEGIN UActorComponent overrides */
	virtual void InitializeComponent() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void OnRegister() override;
	/** END UActorComponent overrides */

	/** caches BlackboardComponent's pointer to be used with this brain component */
	void CacheBlackboardComponent(UBlackboardComponent* BBComp);

	/** @return blackboard used with this component */
	UBlackboardComponent* GetBlackboardComponent();

	/** @return blackboard used with this component */
	const UBlackboardComponent* GetBlackboardComponent() const;

	FORCEINLINE AAIController* GetAIOwner() const { return AIOwner; }

protected:

	/** active message observers */
	TArray<FAIMessageObserver*> MessageObservers;

	friend struct FAIMessageObserver;
	friend struct FAIMessage;

	/** used to keep track of which subsystem requested this AI resource be locked */
	FAIResourceLock ResourceLock;

private:
	uint32 bDoLogicRestartOnUnlock : 1;

public:
	// static names to be used with SendMessage. Fell free to define game-specific
	// messages anywhere you want
	static const FName AIMessage_MoveFinished;
	static const FName AIMessage_RepathFailed;
	static const FName AIMessage_QueryFinished;
};

//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE UBlackboardComponent* UBrainComponent::GetBlackboardComponent()
{
	return BlackboardComp;
}

FORCEINLINE const UBlackboardComponent* UBrainComponent::GetBlackboardComponent() const
{
	return BlackboardComp;
}
