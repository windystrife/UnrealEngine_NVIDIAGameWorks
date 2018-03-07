// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "UObject/CoreNet.h"
#include "UObject/ScriptInterface.h"
#include "Components/ActorComponent.h"
#include "EngineDefines.h"
#include "GameplayTaskOwnerInterface.h"
#include "GameplayTask.h"
#include "GameplayTaskResource.h"
#include "GameplayTasksComponent.generated.h"

class AActor;
class Error;
class FOutBunch;
class UActorChannel;

enum class EGameplayTaskEvent : uint8
{
	Add,
	Remove,
};

UENUM()
enum class EGameplayTaskRunResult : uint8
{
	/** When tried running a null-task*/
	Error,
	Failed,
	/** Successfully registered for running, but currently paused due to higher priority tasks running */
	Success_Paused,
	/** Successfully activated */
	Success_Active,
	/** Successfully activated, but finished instantly */
	Success_Finished,
};

struct FGameplayTaskEventData
{
	EGameplayTaskEvent Event;
	UGameplayTask& RelatedTask;

	FGameplayTaskEventData(EGameplayTaskEvent InEvent, UGameplayTask& InRelatedTask)
		: Event(InEvent), RelatedTask(InRelatedTask)
	{

	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnClaimedResourcesChangeSignature, FGameplayResourceSet, NewlyClaimed, FGameplayResourceSet, FreshlyReleased);

typedef TArray<UGameplayTask*>::TConstIterator FConstGameplayTaskIterator;

/**
*	The core ActorComponent for interfacing with the GameplayAbilities System
*/
UCLASS(ClassGroup = GameplayTasks, hidecategories = (Object, LOD, Lighting, Transform, Sockets, TextureStreaming), editinlinenew, meta = (BlueprintSpawnableComponent))
class GAMEPLAYTASKS_API UGameplayTasksComponent : public UActorComponent, public IGameplayTaskOwnerInterface
{
	GENERATED_BODY()

protected:
	/** Tasks that run on simulated proxies */
	UPROPERTY(ReplicatedUsing = OnRep_SimulatedTasks)
	TArray<UGameplayTask*> SimulatedTasks;

	UPROPERTY()
	TArray<UGameplayTask*> TaskPriorityQueue;
	
	/** Transient array of events whose main role is to avoid
	 *	long chain of recurrent calls if an activated/paused/removed task 
	 *	wants to push/pause/kill other tasks.
	 *	Note: this TaskEvents is assumed to be used in a single thread */
	TArray<FGameplayTaskEventData> TaskEvents;

	/** Array of currently active UGameplayTask that require ticking */
	UPROPERTY()
	TArray<UGameplayTask*> TickingTasks;

	/** All known tasks (processed by this component) referenced for GC */
	UPROPERTY(transient)
	TArray<UGameplayTask*> KnownTasks;

	/** Indicates what's the highest priority among currently running tasks */
	uint8 TopActivePriority;

	/** Resources used by currently active tasks */
	FGameplayResourceSet CurrentlyClaimedResources;

public:
	UPROPERTY(BlueprintReadWrite, Category = "Gameplay Tasks")
	FOnClaimedResourcesChangeSignature OnClaimedResourcesChange;

	UGameplayTasksComponent(const FObjectInitializer& ObjectInitializer);
	
	UFUNCTION()
	void OnRep_SimulatedTasks();

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel *Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void UpdateShouldTick();

	/** retrieves information whether this component should be ticking taken current
	*	activity into consideration*/
	virtual bool GetShouldTick() const;
	
	/** processes the task and figures out if it should get triggered instantly or wait
	 *	based on task's RequiredResources, Priority and ResourceOverlapPolicy */
	void AddTaskReadyForActivation(UGameplayTask& NewTask);

	void RemoveResourceConsumingTask(UGameplayTask& Task);
	void EndAllResourceConsumingTasksOwnedBy(const IGameplayTaskOwnerInterface& TaskOwner);

	bool FindAllResourceConsumingTasksOwnedBy(const IGameplayTaskOwnerInterface& TaskOwner, TArray<UGameplayTask*>& FoundTasks) const;
	
	/** finds first resource-consuming task of given name */
	UGameplayTask* FindResourceConsumingTaskByName(const FName TaskInstanceName) const;

	bool HasActiveTasks(UClass* TaskClass) const;

	FORCEINLINE FGameplayResourceSet GetCurrentlyUsedResources() const { return CurrentlyClaimedResources; }

	// BEGIN IGameplayTaskOwnerInterface
	virtual UGameplayTasksComponent* GetGameplayTasksComponent(const UGameplayTask& Task) const { return const_cast<UGameplayTasksComponent*>(this); }
	virtual AActor* GetGameplayTaskOwner(const UGameplayTask* Task) const override { return GetOwner(); }
	virtual AActor* GetGameplayTaskAvatar(const UGameplayTask* Task) const override { return GetOwner(); }
	virtual void OnGameplayTaskActivated(UGameplayTask& Task) override;
	virtual void OnGameplayTaskDeactivated(UGameplayTask& Task) override;
	// END IGameplayTaskOwnerInterface

	UFUNCTION(BlueprintCallable, DisplayName="Run Gameplay Task", Category = "Gameplay Tasks", meta = (AutoCreateRefTerm = "AdditionalRequiredResources, AdditionalClaimedResources", AdvancedDisplay = "AdditionalRequiredResources, AdditionalClaimedResources"))
	static EGameplayTaskRunResult K2_RunGameplayTask(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, UGameplayTask* Task, uint8 Priority, TArray<TSubclassOf<UGameplayTaskResource> > AdditionalRequiredResources, TArray<TSubclassOf<UGameplayTaskResource> > AdditionalClaimedResources);

	static EGameplayTaskRunResult RunGameplayTask(IGameplayTaskOwnerInterface& TaskOwner, UGameplayTask& Task, uint8 Priority, FGameplayResourceSet AdditionalRequiredResources, FGameplayResourceSet AdditionalClaimedResources);
	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FString GetTickingTasksDescription() const;
	FString GetKnownTasksDescription() const;
	FString GetTasksPriorityQueueDescription() const;
	static FString GetTaskStateName(EGameplayTaskState Value);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FConstGameplayTaskIterator GetTickingTaskIterator() const;
	FConstGameplayTaskIterator GetKnownTaskIterator() const;
	FConstGameplayTaskIterator GetPriorityQueueIterator() const;

#if ENABLE_VISUAL_LOG
	void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const;
#endif // ENABLE_VISUAL_LOG

protected:
	struct FEventLock
	{
		FEventLock(UGameplayTasksComponent* InOwner);
		~FEventLock();

	protected:
		UGameplayTasksComponent* Owner;
	};

	void RequestTicking();
	void ProcessTaskEvents();
	void UpdateTaskActivations();

	void SetCurrentlyClaimedResources(FGameplayResourceSet NewClaimedSet);

private:
	/** called when a task gets ended with an external call, meaning not coming from UGameplayTasksComponent mechanics */
	void OnTaskEnded(UGameplayTask& Task);

	void AddTaskToPriorityQueue(UGameplayTask& NewTask);
	void RemoveTaskFromPriorityQueue(UGameplayTask& Task);

	friend struct FEventLock;
	int32 EventLockCounter;
	uint32 bInEventProcessingInProgress : 1;

	FORCEINLINE bool CanProcessEvents() const { return !bInEventProcessingInProgress && (EventLockCounter == 0); }
};
