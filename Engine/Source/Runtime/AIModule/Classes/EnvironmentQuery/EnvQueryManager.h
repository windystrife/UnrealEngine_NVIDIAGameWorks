// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "EnvironmentQuery/Items/EnvQueryItemType.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Tickable.h"
#include "EnvironmentQuery/EnvQueryInstanceBlueprintWrapper.h"
#include "EnvQueryManager.generated.h"

class UEnvQuery;
class UEnvQueryManager;
class UEnvQueryOption;
class UEnvQueryTest;

/** wrapper for easy query execution */
USTRUCT()
struct AIMODULE_API FEnvQueryRequest
{
	GENERATED_USTRUCT_BODY()

	FEnvQueryRequest() : QueryTemplate(NULL), Owner(NULL), World(NULL) {}

	// basic constructor: owner will be taken from finish delegate bindings
	FEnvQueryRequest(const UEnvQuery* Query) : QueryTemplate(Query), Owner(NULL), World(NULL) {}

	// use when owner is different from finish delegate binding
	FEnvQueryRequest(const UEnvQuery* Query, UObject* RequestOwner) : QueryTemplate(Query), Owner(RequestOwner), World(NULL) {}

	// set named params
	FORCEINLINE FEnvQueryRequest& SetFloatParam(FName ParamName, float Value) { NamedParams.Add(ParamName, Value); return *this; }
	FORCEINLINE FEnvQueryRequest& SetIntParam(FName ParamName, int32 Value) { NamedParams.Add(ParamName, *((float*)&Value)); return *this; }
	FORCEINLINE FEnvQueryRequest& SetBoolParam(FName ParamName, bool Value) { NamedParams.Add(ParamName, Value ? 1.0f : -1.0f); return *this; }
	FORCEINLINE FEnvQueryRequest& SetNamedParam(const FEnvNamedValue& ParamData) { NamedParams.Add(ParamData.ParamName, ParamData.Value); return *this; }
	FEnvQueryRequest& SetNamedParams(const TArray<FEnvNamedValue>& Params);

	// set world (for accessing query manager) when owner can't provide it
	FORCEINLINE FEnvQueryRequest& SetWorldOverride(UWorld* InWorld) { World = InWorld; return *this; }

	template< class UserClass >	
	FORCEINLINE int32 Execute(EEnvQueryRunMode::Type Mode, UserClass* InObj, typename FQueryFinishedSignature::TUObjectMethodDelegate< UserClass >::FMethodPtr InMethod)
	{
		return Execute(Mode, FQueryFinishedSignature::CreateUObject(InObj, InMethod));
	}
	template< class UserClass >	
	FORCEINLINE int32 Execute(EEnvQueryRunMode::Type Mode, UserClass* InObj, typename FQueryFinishedSignature::TUObjectMethodDelegate_Const< UserClass >::FMethodPtr InMethod)
	{
		return Execute(Mode, FQueryFinishedSignature::CreateUObject(InObj, InMethod));
	}
	int32 Execute(EEnvQueryRunMode::Type RunMode, FQueryFinishedSignature const& FinishDelegate);

protected:

	/** query to run */
	UPROPERTY()
	const UEnvQuery* QueryTemplate;

	/** querier */
	UPROPERTY()
	UObject* Owner;

	/** world */
	UPROPERTY()
	UWorld* World;

	/** list of named params */
	TMap<FName, float> NamedParams;

	friend UEnvQueryManager;
};

/** cache of instances with sorted tests */
USTRUCT()
struct FEnvQueryInstanceCache
{
	GENERATED_USTRUCT_BODY()

	/** query template, duplicated in manager's world */
	UPROPERTY()
	UEnvQuery* Template;

	/** instance to duplicate */
	FEnvQueryInstance Instance;

	/** the name of the source query */
	FName AssetName;
};

#if USE_EQS_DEBUGGER
struct AIMODULE_API FEQSDebugger
{
	struct FEnvQueryInfo
	{
		TSharedPtr<FEnvQueryInstance> Instance;
		float Timestamp;
	};

	struct FStatsInfo
	{
		// most expensive run
		FEnvQueryDebugProfileData MostExpensive;
		float MostExpensiveDuration;

		// average run (sum of all runs, divide by TotalAvgCount to get values)
		FEnvQueryDebugProfileData TotalAvgData;
		float TotalAvgDuration;
		int32 TotalAvgCount;

		// EQS tick load
		TArray<uint8> TickPct;
		float LastTickTime;
		uint64 LastTickFrame;
		uint16 FirstTickEntry;
		uint16 LastTickEntry;

		FStatsInfo() :
			MostExpensiveDuration(0.0f),
			TotalAvgDuration(0.0f), TotalAvgCount(0),
			LastTickTime(0.0f), LastTickFrame(0),
			FirstTickEntry(MAX_uint16), LastTickEntry(0)
		{}
	};

	void StoreStats(const FEnvQueryInstance& QueryInstance);
	void StoreTickTime(const FEnvQueryInstance& QueryInstance, float TickTime, float MaxTickTime);
	void StoreQuery(const TSharedPtr<FEnvQueryInstance>& QueryInstance);
	
	static void SaveStats(const FString& FileName);
	static void LoadStats(const FString& FileName);

	const TArray<FEnvQueryInfo>& GetAllQueriesForOwner(const UObject* Owner);

	// map query name with profiler data
	TMap<FName, FStatsInfo> StoredStats;

protected:
	// maps owner to performed queries
	TMap<const UObject*, TArray<FEnvQueryInfo> > StoredQueries;
};

FArchive& operator<<(FArchive& Ar, FEQSDebugger::FStatsInfo& Data);

FORCEINLINE bool operator== (const FEQSDebugger::FEnvQueryInfo & Left, const FEQSDebugger::FEnvQueryInfo & Right)
{
	return Left.Instance == Right.Instance;
}
#endif // USE_EQS_DEBUGGER

UCLASS(config = Game, defaultconfig)
class AIMODULE_API UEnvQueryManager : public UObject, public FTickableGameObject, public FSelfRegisteringExec
{
	GENERATED_UCLASS_BODY()

	// We need to implement GetWorld() so that any EQS-related blueprints (such as blueprint contexts) can implement
	// GetWorld() and so provide access to blueprint nodes using hidden WorldContextObject parameters.
	virtual UWorld* GetWorld() const override;

	/** [FTickableGameObject] get world function */
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

	/** [FTickableGameObject] tick function */
	virtual void Tick(float DeltaTime) override;

	/** [FTickableGameObject] always tick, unless it's the default object */
	virtual bool IsTickable() const override { return HasAnyFlags(RF_ClassDefaultObject) == false; }

	/** [FTickableGameObject] tick stats */
	virtual TStatId GetStatId() const override;

	/** execute query */
	int32 RunQuery(const FEnvQueryRequest& Request, EEnvQueryRunMode::Type RunMode, FQueryFinishedSignature const& FinishDelegate);
	int32 RunQuery(const TSharedPtr<FEnvQueryInstance>& QueryInstance, FQueryFinishedSignature const& FinishDelegate);

	/** Removed all active queries asked by Querier. No "on finished" notifications are being sent, call this function when
	 *	you no longer care about Querier's queries, like when he's "dead" */
	void SilentlyRemoveAllQueriesByQuerier(const UObject& Querier)
	{
		RemoveAllQueriesByQuerier(Querier, /*bExecuteFinishDelegate=*/false);
	}

	void RemoveAllQueriesByQuerier(const UObject& Querier, bool bExecuteFinishDelegate);

	/** alternative way to run queries. Do not use for anything other than testing
	*  or when you know exactly what you're doing! Bypasses all EQS perf controlling
	*  and time slicing mechanics. */
	TSharedPtr<FEnvQueryResult> RunInstantQuery(const FEnvQueryRequest& Request, EEnvQueryRunMode::Type RunMode);
	void RunInstantQuery(const TSharedPtr<FEnvQueryInstance>& QueryInstance);

	/** Creates a query instance configured for execution */
	TSharedPtr<FEnvQueryInstance> PrepareQueryInstance(const FEnvQueryRequest& Request, EEnvQueryRunMode::Type RunMode);

	/** finds UEnvQuery matching QueryName by first looking at instantiated queries (from InstanceCache)
	 *	falling back to looking up all UEnvQuery and testing their name */
	UEnvQuery* FindQueryTemplate(const FString& QueryName) const;

	/** creates local context object */
	UEnvQueryContext* PrepareLocalContext(TSubclassOf<UEnvQueryContext> ContextClass);

	/** find value of named param stored with active query */
	float FindNamedParam(int32 QueryId, FName ParamName) const;

	/** execute query */
	bool AbortQuery(int32 RequestID);

	/** fail all running queries on cleaning the world */
	virtual void OnWorldCleanup();

	/** cleanup hooks for map loading */
	virtual void FinishDestroy() override;

	/** add information for data providers about query instance run independently */
	void RegisterExternalQuery(const TSharedPtr<FEnvQueryInstance>& QueryInstance);

	/** clear information about query instance run independently */
	void UnregisterExternalQuery(const TSharedPtr<FEnvQueryInstance>& QueryInstance);

	/** list of all known item types */
	static TArray<TSubclassOf<UEnvQueryItemType> > RegisteredItemTypes;

	static UEnvQueryManager* GetCurrent(UWorld* World);
	static UEnvQueryManager* GetCurrent(const UObject* WorldContextObject);
	
	UFUNCTION(BlueprintCallable, Category = "AI|EQS", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "WrapperClass"))
	static UEnvQueryInstanceBlueprintWrapper* RunEQSQuery(UObject* WorldContextObject, UEnvQuery* QueryTemplate, UObject* Querier, TEnumAsByte<EEnvQueryRunMode::Type> RunMode, TSubclassOf<UEnvQueryInstanceBlueprintWrapper> WrapperClass);

	void RegisterActiveWrapper(UEnvQueryInstanceBlueprintWrapper& Wrapper);
	void UnregisterActiveWrapper(UEnvQueryInstanceBlueprintWrapper& Wrapper);

	static void SetAllowTimeSlicing(bool bAllowTimeSlicing);

	//~ Begin FExec Interface
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	//~ End FExec Interface

protected:
	friend UEnvQueryInstanceBlueprintWrapper;
	TSharedPtr<FEnvQueryInstance> FindQueryInstance(const int32 QueryID);

#if USE_EQS_DEBUGGER
public:
	static void NotifyAssetUpdate(UEnvQuery* Query);
	static TMap<FName, FEQSDebugger::FStatsInfo> DebuggerStats;

	FEQSDebugger& GetDebugger() { return EQSDebugger; }

protected:
	FEQSDebugger EQSDebugger;
#endif // USE_EQS_DEBUGGER

protected:

	/** currently running queries */
	TArray<TSharedPtr<FEnvQueryInstance> > RunningQueries;

	/** count of queries aborted since last update, to be removed. */
	int32 NumRunningQueriesAbortedSinceLastUpdate;

	/** queries run independently from manager, mapped here for data providers */
	TMap<int32, TWeakPtr<FEnvQueryInstance>> ExternalQueries;

	/** cache of instances */
	UPROPERTY(transient)
	TArray<FEnvQueryInstanceCache> InstanceCache;

	/** local cache of context objects for managing BP based objects */
	UPROPERTY(transient)
	TArray<UEnvQueryContext*> LocalContexts;

	UPROPERTY()
	TArray<UEnvQueryInstanceBlueprintWrapper*> GCShieldedWrappers;

	/** local contexts mapped by class names */
	TMap<FName, UEnvQueryContext*> LocalContextMap;

	/** next ID for running query */
	int32 NextQueryID;

	/** create new instance, using cached data is possible */
	TSharedPtr<FEnvQueryInstance> CreateQueryInstance(const UEnvQuery* Template, EEnvQueryRunMode::Type RunMode);

	/** how long are we allowed to test per update, in seconds. */
	UPROPERTY(config)
	float MaxAllowedTestingTime;

	/** whether we update EQS queries based on:
	    running a test on one query and move to the next (breadth) - default behavior,
	    or test an entire query before moving to the next one (depth). */
	UPROPERTY(config)
	bool bTestQueriesUsingBreadth;

	/** if greater than zero, we will warn once when the number of queries is greater than or equal to this number, and log the queries out */
	UPROPERTY(config)
	int32 QueryCountWarningThreshold;

	/** how often (in seconds) we will warn about the number of queries (allows us to catch multiple occurrences in a session) */
	UPROPERTY(config)
	double QueryCountWarningInterval;

private:

	/** create and bind delegates in instance */
	void CreateOptionInstance(UEnvQueryOption* OptionTemplate, int32 SourceOptionIndex, const TArray<UEnvQueryTest*>& SortedTests, FEnvQueryInstance& Instance);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static bool bAllowEQSTimeSlicing;

	mutable double LastQueryCountWarningThresholdTime;

	void CheckQueryCount() const;
	void LogQueryCountWarning() const;
#endif
};
