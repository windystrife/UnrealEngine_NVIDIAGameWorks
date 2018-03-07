// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Pawn.h"
#include "FunctionalTest.h"
#include "GenericTeamAgentInterface.h"
#include "FunctionalAITest.generated.h"

class AAIController;
class AFunctionalAITest;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFunctionalTestAISpawned, AAIController*, Controller, APawn*, Pawn);

USTRUCT(BlueprintType)
struct FAITestSpawnInfo
{
	GENERATED_USTRUCT_BODY()

	/** Determines AI to be spawned */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	TSubclassOf<class APawn>  PawnClass;
	
	/** class to override default pawn's controller class. If None the default will be used*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	TSubclassOf<class AAIController>  ControllerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	FGenericTeamId TeamID;

	/** if set will be applied to spawned AI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	class UBehaviorTree* BehaviorTree;

	/** Where should AI be spawned */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	AActor* SpawnLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn, meta=(UIMin=1, ClampMin=1))
	int32 NumberToSpawn;

	/** delay between consecutive spawn attempts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AISpawn, meta = (UIMin = 0, ClampMin = 0))
	float SpawnDelay;

	/** delay before attempting first spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AISpawn, meta = (UIMin = 0, ClampMin = 0))
	float PreSpawnDelay;

	/** Gets filled owning spawn set upon game start */
	FName SpawnSetName;

	FAITestSpawnInfo() : NumberToSpawn(1)
	{}

	FORCEINLINE bool IsValid() const { return PawnClass != NULL && SpawnLocation != NULL; }

	bool Spawn(AFunctionalAITest* AITest) const;
};

USTRUCT()
struct FPendingDelayedSpawn : public FAITestSpawnInfo
{
	GENERATED_USTRUCT_BODY()

	uint32 NumberToSpawnLeft;
	float TimeToNextSpawn;
	bool bFinished;

	FPendingDelayedSpawn()
		: NumberToSpawnLeft(uint32(-1)), TimeToNextSpawn(FLT_MAX), bFinished(true)
	{}
	FPendingDelayedSpawn(const FAITestSpawnInfo& Source);

	void Tick(float TimeDelta, AFunctionalAITest* AITest);
};

USTRUCT(BlueprintType)
struct FAITestSpawnSet
{
	GENERATED_USTRUCT_BODY()

	/** what to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	TArray<FAITestSpawnInfo> SpawnInfoContainer;

	/** give the set a name to help identify it if need be */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AISpawn)
	uint32 bEnabled:1;

	/** location used for spawning if spawn info doesn't define one */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AISpawn)
	AActor* FallbackSpawnLocation;

	FAITestSpawnSet() : bEnabled(true)
	{}
};

UCLASS(Blueprintable, MinimalAPI)
class AFunctionalAITest : public AFunctionalTest
{
	GENERATED_UCLASS_BODY()
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AITest)
	TArray<FAITestSpawnSet> SpawnSets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AITest, meta = (UIMin = "0.0"))
	float SpawnLocationRandomizationRange;

	UPROPERTY(BlueprintReadOnly, Category=AITest)
	TArray<APawn*> SpawnedPawns;

	UPROPERTY(BlueprintReadOnly, Category = AITest)
	TArray<FPendingDelayedSpawn> PendingDelayedSpawns;
	
	int32 CurrentSpawnSetIndex;
	FString CurrentSpawnSetName;

	/** Called when a single AI finished spawning */
	UPROPERTY(BlueprintAssignable)
	FFunctionalTestAISpawned OnAISpawned;

	/** Called when a all AI finished spawning */
	UPROPERTY(BlueprintAssignable)
	FFunctionalTestEventSignature OnAllAISPawned;

	/** navmesh debug: log navoctree modifiers around this point */
	UPROPERTY(EditAnywhere, Category = NavMeshDebug, meta = (MakeEditWidget = ""))
	FVector NavMeshDebugOrigin;

	/** navmesh debug: extent around NavMeshDebugOrigin */
	UPROPERTY(EditAnywhere, Category = NavMeshDebug)
	FVector NavMeshDebugExtent;

	/** if set, ftest will postpone start until navmesh is fully generated */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AITest)
	uint32 bWaitForNavMesh : 1;

	/** if set, ftest will postpone start until navmesh is fully generated */
	UPROPERTY(EditAnywhere, Category = NavMeshDebug)
	uint32 bDebugNavMeshOnTimeout : 1;

	uint32 bSingleSetRun:1;

public:
	UFUNCTION(BlueprintCallable, Category = "Development")
	virtual bool IsOneOfSpawnedPawns(AActor* Actor);

	// AActor interface begin
protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaSeconds) override;
	// AActor interface end

	virtual bool RunTest(const TArray<FString>& Params = TArray<FString>()) override;
	virtual void StartTest() override;
	virtual void OnTimeout() override;
	virtual bool IsReady_Implementation() override;
	virtual bool WantsToRunAgain() const override;
	virtual void GatherRelevantActors(TArray<AActor*>& OutActors) const override;
	virtual void CleanUp() override;
	virtual FString GetAdditionalTestFinishedMessage(EFunctionalTestResult TestResult) const override;
	virtual FString GetReproString() const override;

	void AddSpawnedPawn(APawn& SpawnedPawn);

	FVector GetRandomizedLocation(const FVector& Location) const;

protected:

	void KillOffSpawnedPawns();
	void ClearPendingDelayedSpawns();
	void StartSpawning();
	void OnSpawningFailure();
	bool IsNavMeshReady() const;

	FTimerHandle NavmeshDelayTimer;
};
