// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTask.h"
#include "AITask.generated.h"

class AActor;
class AAIController;

UENUM()
enum class EAITaskPriority : uint8
{
	Lowest = 0,
	Low = 64, //FGameplayTasks::DefaultPriority / 2,
	AutonomousAI = 127, //FGameplayTasks::DefaultPriority,
	High = 192, //(1.5 * FGameplayTasks::DefaultPriority),
	Ultimate = 254,
};

UCLASS(Abstract, BlueprintType)
class AIMODULE_API UAITask : public UGameplayTask
{
	GENERATED_BODY()
protected:

	UPROPERTY(BlueprintReadOnly, Category="AI|Tasks")
	AAIController* OwnerController;

	virtual void Activate() override;

public:
	UAITask(const FObjectInitializer& ObjectInitializer);

	static AAIController* GetAIControllerForActor(AActor* Actor);
	AAIController* GetAIController() const { return OwnerController; };

	void InitAITask(AAIController& AIOwner, IGameplayTaskOwnerInterface& InTaskOwner, uint8 InPriority);
	void InitAITask(AAIController& AIOwner, IGameplayTaskOwnerInterface& InTaskOwner);

	/** effectively adds UAIResource_Logic to the set of Claimed resources */
	void RequestAILogicLocking();

	template <class T>
	static T* NewAITask(AAIController& AIOwner, IGameplayTaskOwnerInterface& InTaskOwner, FName InstanceName = FName())
	{
		T* TaskInstance = NewObject<T>();
		TaskInstance->InstanceName = InstanceName;
		TaskInstance->InitAITask(AIOwner, InTaskOwner);
		return TaskInstance;
	}

	template <class T>
	static T* NewAITask(AAIController& AIOwner, IGameplayTaskOwnerInterface& InTaskOwner, EAITaskPriority InPriority, FName InstanceName = FName())
	{
		T* TaskInstance = NewObject<T>();
		TaskInstance->InstanceName = InstanceName;
		TaskInstance->InitAITask(AIOwner, InTaskOwner, (uint8)InPriority);
		return TaskInstance;
	}

	template <class T>
	static T* NewAITask(AAIController& AIOwner, FName InstanceName = FName())
	{
		T* TaskInstance = NewObject<T>();
		TaskInstance->InstanceName = InstanceName;
		TaskInstance->InitAITask(AIOwner, AIOwner);
		return TaskInstance;
	}

	template <class T>
	static T* NewAITask(AAIController& AIOwner, EAITaskPriority InPriority, FName InstanceName = FName())
	{
		T* TaskInstance = NewObject<T>();
		TaskInstance->InstanceName = InstanceName;
		TaskInstance->InitAITask(AIOwner, AIOwner, (uint8)InPriority);
		return TaskInstance;
	}
};
