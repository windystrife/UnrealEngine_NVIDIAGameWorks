// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/World.h"
#include "AI/AISystemBase.h"
#include "Math/RandomStream.h"
#include "AISystem.generated.h"

class UAIAsyncTaskBlueprintProxy;
class UAIHotSpotManager;
class UAIPerceptionSystem;
class UAISystem;
class UBehaviorTreeManager;
class UBlackboardComponent;
class UBlackboardData;
class UEnvQueryManager;
class UNavLocalGridManager;

#define GET_AI_CONFIG_VAR(a) (GetDefault<UAISystem>()->a)

UCLASS(config=Engine, defaultconfig)
class AIMODULE_API UAISystem : public UAISystemBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(globalconfig, EditAnywhere, Category = "AISystem", meta = (MetaClass = "AIPerceptionSystem", DisplayName = "Perception System Class"))
	FSoftClassPath PerceptionSystemClassName;

	UPROPERTY(globalconfig, EditAnywhere, Category = "AISystem", meta = (MetaClass = "AIHotSpotManager", DisplayName = "AIHotSpotManager Class"))
	FSoftClassPath HotSpotManagerClassName;

public:
	/** Default AI movement's acceptance radius used to determine whether 
 	 * AI reached path's end */
	UPROPERTY(globalconfig, EditDefaultsOnly, Category = "Movement")
	float AcceptanceRadius; 

	/** Value used for pathfollowing's internal code to determine whether AI reached path's point. 
	 *	@note this value is not used for path's last point. @see AcceptanceRadius*/
	UPROPERTY(globalconfig, EditDefaultsOnly, Category = "Movement")
	float PathfollowingRegularPathPointAcceptanceRadius;
	
	/** Similarly to PathfollowingRegularPathPointAcceptanceRadius used by pathfollowing's internals
	 *	but gets applied only when next point on a path represents a begining of navigation link */
	UPROPERTY(globalconfig, EditDefaultsOnly, Category = "Movement")
	float PathfollowingNavLinkAcceptanceRadius;
	
	UPROPERTY(globalconfig, EditDefaultsOnly, Category = "Movement")
	bool bFinishMoveOnGoalOverlap;

	UPROPERTY(globalconfig, EditDefaultsOnly, Category = "Movement")
	bool bAcceptPartialPaths;

	UPROPERTY(globalconfig, EditDefaultsOnly, Category = "Movement")
	bool bAllowStrafing;

	/** this property is just a transition-time flag - in the end we're going to switch over to Gameplay Tasks anyway, that's the goal. */
	UPROPERTY(globalconfig, EditDefaultsOnly, Category = "Gameplay Tasks")
	bool bEnableBTAITasks;

	/** if enable will make EQS not complaint about using Controllers as queriers. Default behavior (false) will 
	 *	in places automatically convert controllers to pawns, and complain if code user bypasses the conversion or uses
	 *	pawn-less controller */
	UPROPERTY(globalconfig, EditDefaultsOnly, Category = "EQS")
	bool bAllowControllersAsEQSQuerier;

	/** if set, GameplayDebuggerPlugin will be loaded on module's startup */
	UPROPERTY(globalconfig, EditDefaultsOnly, Category = "AISystem")
	bool bEnableDebuggerPlugin;

	UPROPERTY(globalconfig, EditAnywhere, Category = "PerceptionSystem")
	TEnumAsByte<ECollisionChannel> DefaultSightCollisionChannel;

protected:
	/** Behavior tree manager used by game */
	UPROPERTY(Transient)
	UBehaviorTreeManager* BehaviorTreeManager;

	/** Environment query manager used by game */
	UPROPERTY(Transient)
	UEnvQueryManager* EnvironmentQueryManager;

	UPROPERTY(Transient)
	UAIPerceptionSystem* PerceptionSystem;

	UPROPERTY(Transient)
	TArray<UAIAsyncTaskBlueprintProxy*> AllProxyObjects;

	UPROPERTY(Transient)
	UAIHotSpotManager* HotSpotManager;

	UPROPERTY(Transient)
	UNavLocalGridManager* NavLocalGrids;

	typedef TMultiMap<TWeakObjectPtr<UBlackboardData>, TWeakObjectPtr<UBlackboardComponent> > FBlackboardDataToComponentsMap;

	/** UBlackboardComponent instances that reference the blackboard data definition */
	FBlackboardDataToComponentsMap BlackboardDataToComponentsMap;

	FDelegateHandle ActorSpawnedDelegateHandle;

	/** random number stream to be used by all things AI. WIP */
	static FRandomStream RandomStream;
	
public:
	UAISystem(const FObjectInitializer& ObjectInitializer);

	virtual void BeginDestroy() override;
	
	virtual void PostInitProperties() override;

	// UAISystemBase begin		
	virtual void InitializeActorsForPlay(bool bTimeGotReset) override;
	virtual void WorldOriginLocationChanged(FIntVector OldOriginLocation, FIntVector NewOriginLocation) override;
	virtual void CleanupWorld(bool bSessionEnded = true, bool bCleanupResources = true, UWorld* NewWorld = NULL) override;
	virtual void StartPlay() override;
	// UAISystemBase end

	/** Behavior tree manager getter */
	FORCEINLINE UBehaviorTreeManager* GetBehaviorTreeManager() { return BehaviorTreeManager; }
	/** Behavior tree manager const getter */
	FORCEINLINE const UBehaviorTreeManager* GetBehaviorTreeManager() const { return BehaviorTreeManager; }

	/** Environment Query manager getter */
	FORCEINLINE UEnvQueryManager* GetEnvironmentQueryManager() { return EnvironmentQueryManager; }
	/** Environment Query manager const getter */
	FORCEINLINE const UEnvQueryManager* GetEnvironmentQueryManager() const { return EnvironmentQueryManager; }

	FORCEINLINE UAIPerceptionSystem* GetPerceptionSystem() { return PerceptionSystem; }
	FORCEINLINE const UAIPerceptionSystem* GetPerceptionSystem() const { return PerceptionSystem; }

	FORCEINLINE UAIHotSpotManager* GetHotSpotManager() { return HotSpotManager; }
	FORCEINLINE const UAIHotSpotManager* GetHotSpotManager() const { return HotSpotManager; }

	FORCEINLINE UNavLocalGridManager* GetNavLocalGridManager() { return NavLocalGrids; }
	FORCEINLINE const UNavLocalGridManager* GetNavLocalGridManager() const { return NavLocalGrids; }

	FORCEINLINE static UAISystem* GetCurrentSafe(UWorld* World) 
	{ 
		return World != nullptr ? Cast<UAISystem>(World->GetAISystem()) : NULL;
	}

	FORCEINLINE static UAISystem* GetCurrent(UWorld& World)
	{
		return Cast<UAISystem>(World.GetAISystem());
	}

	FORCEINLINE UWorld* GetOuterWorld() const { return Cast<UWorld>(GetOuter()); }

	virtual UWorld* GetWorld() const override { return GetOuterWorld(); }
	
	FORCEINLINE void AddReferenceFromProxyObject(UAIAsyncTaskBlueprintProxy* BlueprintProxy) { AllProxyObjects.AddUnique(BlueprintProxy); }

	FORCEINLINE void RemoveReferenceToProxyObject(UAIAsyncTaskBlueprintProxy* BlueprintProxy) { AllProxyObjects.RemoveSwap(BlueprintProxy); }

	//----------------------------------------------------------------------//
	// cheats
	//----------------------------------------------------------------------//
	UFUNCTION(exec)
	virtual void AIIgnorePlayers();

	UFUNCTION(exec)
	virtual void AILoggingVerbose();

	/** insta-runs EQS query for given Target */
	void RunEQS(const FString& QueryName, UObject* Target);

	/**
	* Iterator for traversing all UBlackboardComponent instances associated
	* with this blackboard data asset. This is a forward only iterator.
	*/
	struct FBlackboardDataToComponentsIterator
	{
	public:
		FBlackboardDataToComponentsIterator(FBlackboardDataToComponentsMap& BlackboardDataToComponentsMap, class UBlackboardData* BlackboardAsset);

		FORCEINLINE FBlackboardDataToComponentsIterator& operator++()
		{
			++GetCurrentIteratorRef();
			TryMoveIteratorToParentBlackboard();
			return *this;
		}
		FORCEINLINE FBlackboardDataToComponentsIterator operator++(int)
		{
			FBlackboardDataToComponentsIterator Tmp(*this);
			++GetCurrentIteratorRef();
			TryMoveIteratorToParentBlackboard();
			return Tmp;
		}

		FORCEINLINE explicit operator bool() const { return CurrentIteratorIndex < Iterators.Num() && (bool)GetCurrentIteratorRef(); }
		FORCEINLINE bool operator !() const { return !(bool)*this; }

		FORCEINLINE UBlackboardData* Key() const { return GetCurrentIteratorRef().Key().Get(); }
		FORCEINLINE UBlackboardComponent* Value() const { return GetCurrentIteratorRef().Value().Get(); }

	private:
		FORCEINLINE const FBlackboardDataToComponentsMap::TConstKeyIterator& GetCurrentIteratorRef() const { return Iterators[CurrentIteratorIndex]; }
		FORCEINLINE FBlackboardDataToComponentsMap::TConstKeyIterator& GetCurrentIteratorRef() { return Iterators[CurrentIteratorIndex]; }

		void TryMoveIteratorToParentBlackboard()
		{
			if (!GetCurrentIteratorRef() && CurrentIteratorIndex < Iterators.Num() - 1)
			{
				++CurrentIteratorIndex;
				TryMoveIteratorToParentBlackboard(); // keep incrementing until we find a valid iterator.
			}
		}

		int32 CurrentIteratorIndex;

		static const int32 InlineSize = 8;
		TArray<FBlackboardDataToComponentsMap::TConstKeyIterator, TInlineAllocator<InlineSize>> Iterators;
	};

	/**
	* Registers a UBlackboardComponent instance with this blackboard data asset.
	* This will also register the component for each parent UBlackboardData
	* asset. This should be called after the component has been initialized
	* (i.e. InitializeComponent). The user is responsible for calling
	* UnregisterBlackboardComponent (i.e. UninitializeComponent).
	*/
	void RegisterBlackboardComponent(class UBlackboardData& BlackboardAsset, class UBlackboardComponent& BlackboardComp);

	/**
	* Unregisters a UBlackboardComponent instance with this blackboard data
	* asset. This should be called before the component has been uninitialized
	* (i.e. UninitializeComponent).
	*/
	void UnregisterBlackboardComponent(class UBlackboardData& BlackboardAsset, class UBlackboardComponent& BlackboardComp);

	/**
	* Creates a forward only iterator for that will iterate all
	* UBlackboardComponent instances that reference the specified
	* BlackboardAsset and it's parents.
	*/
	FBlackboardDataToComponentsIterator CreateBlackboardDataToComponentsIterator(class UBlackboardData& BlackboardAsset);

	virtual void ConditionalLoadDebuggerPlugin();

	static const FRandomStream& GetRandomStream() { return RandomStream; }
	static void SeedRandomStream(const int32 Seed) { return RandomStream.Initialize(Seed); }

protected:
	virtual void OnActorSpawned(AActor* SpawnedActor);
	void LoadDebuggerPlugin();
};
