// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQuery/EnvQueryManager.h"
#include "UObject/UObjectIterator.h"
#include "EngineGlobals.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "AISystem.h"
#include "VisualLogger/VisualLogger.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EQSTestingPawn.h"
#include "EnvironmentQuery/EnvQueryDebugHelpers.h"
#include "Engine/Engine.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "EngineUtils.h"

extern UNREALED_API UEditorEngine* GEditor;
#endif // WITH_EDITOR
#include "TimeGuard.h"

DEFINE_LOG_CATEGORY(LogEQS);

DEFINE_STAT(STAT_AI_EQS_Tick);
DEFINE_STAT(STAT_AI_EQS_TickWork);
DEFINE_STAT(STAT_AI_EQS_TickNotifies);
DEFINE_STAT(STAT_AI_EQS_TickQueryRemovals);
DEFINE_STAT(STAT_AI_EQS_LoadTime);
DEFINE_STAT(STAT_AI_EQS_ExecuteOneStep);
DEFINE_STAT(STAT_AI_EQS_GeneratorTime);
DEFINE_STAT(STAT_AI_EQS_TestTime);
DEFINE_STAT(STAT_AI_EQS_NumInstances);
DEFINE_STAT(STAT_AI_EQS_NumItems);
DEFINE_STAT(STAT_AI_EQS_InstanceMemory);
DEFINE_STAT(STAT_AI_EQS_AvgInstanceResponseTime);
DEFINE_STAT(STAT_AI_EQS_Debug_StoreQuery);
DEFINE_STAT(STAT_AI_EQS_Debug_StoreTickTime);
DEFINE_STAT(STAT_AI_EQS_Debug_StoreStats);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bool UEnvQueryManager::bAllowEQSTimeSlicing = true;
#endif

#if USE_EQS_DEBUGGER
	TMap<FName, FEQSDebugger::FStatsInfo> UEnvQueryManager::DebuggerStats;
#endif


//////////////////////////////////////////////////////////////////////////
// FEnvQueryRequest

FEnvQueryRequest& FEnvQueryRequest::SetNamedParams(const TArray<FEnvNamedValue>& Params)
{
	for (int32 ParamIndex = 0; ParamIndex < Params.Num(); ParamIndex++)
	{
		NamedParams.Add(Params[ParamIndex].ParamName, Params[ParamIndex].Value);
	}

	return *this;
}

int32 FEnvQueryRequest::Execute(EEnvQueryRunMode::Type RunMode, FQueryFinishedSignature const& FinishDelegate)
{
	if (Owner == NULL)
	{
		Owner = FinishDelegate.GetUObject();
		if (Owner == NULL)
		{
			UE_LOG(LogEQS, Warning, TEXT("Unknown owner of request: %s"), *GetNameSafe(QueryTemplate));
			return INDEX_NONE;
		}
	}

	if (World == NULL)
	{
		World = GEngine->GetWorldFromContextObject(Owner, EGetWorldErrorMode::ReturnNull);
		if (World == NULL)
		{
			UE_LOG(LogEQS, Warning, TEXT("Unable to access world with owner: %s"), *GetNameSafe(Owner));
			return INDEX_NONE;
		}
	}

	UEnvQueryManager* EnvQueryManager = UEnvQueryManager::GetCurrent(World);
	if (EnvQueryManager == NULL)
	{
		UE_LOG(LogEQS, Warning, TEXT("Missing EQS manager!"));
		return INDEX_NONE;
	}

	return EnvQueryManager->RunQuery(*this, RunMode, FinishDelegate);
}


//////////////////////////////////////////////////////////////////////////
// UEnvQueryManager

TArray<TSubclassOf<UEnvQueryItemType> > UEnvQueryManager::RegisteredItemTypes;

UEnvQueryManager::UEnvQueryManager(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NextQueryID = 0;
	MaxAllowedTestingTime = 0.01f;
	bTestQueriesUsingBreadth = true;
	NumRunningQueriesAbortedSinceLastUpdate = 0;

	QueryCountWarningThreshold = 0;
	QueryCountWarningInterval = 30.0;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	LastQueryCountWarningThresholdTime = -FLT_MAX;
#endif

#if USE_EQS_DEBUGGER
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UEnvQueryManager::DebuggerStats.Empty();
	}
#endif
}

UWorld* UEnvQueryManager::GetWorld() const
{
	return Cast<UWorld>(GetOuter());
}

void UEnvQueryManager::FinishDestroy()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	Super::FinishDestroy();
}

UEnvQueryManager* UEnvQueryManager::GetCurrent(UWorld* World)
{
	UAISystem* AISys = UAISystem::GetCurrentSafe(World);
	return AISys ? AISys->GetEnvironmentQueryManager() : nullptr;
}

UEnvQueryManager* UEnvQueryManager::GetCurrent(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	UAISystem* AISys = UAISystem::GetCurrentSafe(World);
	return AISys ? AISys->GetEnvironmentQueryManager() : nullptr;
}

#if USE_EQS_DEBUGGER
void UEnvQueryManager::NotifyAssetUpdate(UEnvQuery* Query)
{
#if WITH_EDITOR
	if (GEditor == NULL)
	{
		return;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (World)
	{
		UEnvQueryManager* EQS = UEnvQueryManager::GetCurrent(World);
		if (EQS)
		{
			EQS->InstanceCache.Reset();
		}

		// was as follows, but got broken with changes to actor iterator (FActorIteratorBase::SpawnedActorArray)
		// for (TActorIterator<AEQSTestingPawn> It(World); It; ++It)
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AEQSTestingPawn* EQSPawn = Cast<AEQSTestingPawn>(*It);
			if (EQSPawn == NULL)
			{
				continue;
			}

			if (EQSPawn->QueryTemplate == Query || Query == NULL)
			{
				EQSPawn->RunEQSQuery();
			}
		}
	}
#endif //WITH_EDITOR
}
#endif // USE_EQS_DEBUGGER

TStatId UEnvQueryManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEnvQueryManager, STATGROUP_Tickables);
}

int32 UEnvQueryManager::RunQuery(const FEnvQueryRequest& Request, EEnvQueryRunMode::Type RunMode, FQueryFinishedSignature const& FinishDelegate)
{
	TSharedPtr<FEnvQueryInstance> QueryInstance = PrepareQueryInstance(Request, RunMode);
	return RunQuery(QueryInstance, FinishDelegate);
}

int32 UEnvQueryManager::RunQuery(const TSharedPtr<FEnvQueryInstance>& QueryInstance, FQueryFinishedSignature const& FinishDelegate)
{
	if (QueryInstance.IsValid() == false)
	{
		return INDEX_NONE;
	}

	QueryInstance->FinishDelegate = FinishDelegate;
	RunningQueries.Add(QueryInstance);

	return QueryInstance->QueryID;
}

TSharedPtr<FEnvQueryResult> UEnvQueryManager::RunInstantQuery(const FEnvQueryRequest& Request, EEnvQueryRunMode::Type RunMode)
{
	TSharedPtr<FEnvQueryInstance> QueryInstance = PrepareQueryInstance(Request, RunMode);
	if (!QueryInstance.IsValid())
	{
		return NULL;
	}

	RunInstantQuery(QueryInstance);

	return QueryInstance;
}

void UEnvQueryManager::RunInstantQuery(const TSharedPtr<FEnvQueryInstance>& QueryInstance)
{
	if (! ensure(QueryInstance.IsValid()))
	{
		return;
	}

	RegisterExternalQuery(QueryInstance);
	while (QueryInstance->IsFinished() == false)
	{
		QueryInstance->ExecuteOneStep(UEnvQueryTypes::UnlimitedStepTime);
	}

	UnregisterExternalQuery(QueryInstance);

	UE_VLOG_EQS(*QueryInstance.Get(), LogEQS, All);

#if USE_EQS_DEBUGGER
	EQSDebugger.StoreQuery(QueryInstance);
#endif // USE_EQS_DEBUGGER
}

void UEnvQueryManager::RemoveAllQueriesByQuerier(const UObject& Querier, bool bExecuteFinishDelegate)
{
	for (int32 QueryIndex = RunningQueries.Num() - 1; QueryIndex >= 0; --QueryIndex)
	{
		const TSharedPtr<FEnvQueryInstance>& QueryInstance = RunningQueries[QueryIndex];
		if (QueryInstance.IsValid() == false || QueryInstance->Owner.IsValid() == false || QueryInstance->Owner.Get() == &Querier)
		{
			if (QueryInstance->IsFinished() == false)
			{
				QueryInstance->MarkAsAborted();

				if (bExecuteFinishDelegate)
				{
					QueryInstance->FinishDelegate.ExecuteIfBound(QueryInstance);
				}

				// We will remove the aborted query from the RunningQueries array on the next EQS update
				++NumRunningQueriesAbortedSinceLastUpdate;
			}
		}
	}
}

TSharedPtr<FEnvQueryInstance> UEnvQueryManager::PrepareQueryInstance(const FEnvQueryRequest& Request, EEnvQueryRunMode::Type RunMode)
{
	TSharedPtr<FEnvQueryInstance> QueryInstance = CreateQueryInstance(Request.QueryTemplate, RunMode);
	if (!QueryInstance.IsValid())
	{
		return NULL;
	}

	QueryInstance->World = Cast<UWorld>(GetOuter());
	QueryInstance->Owner = Request.Owner;
	QueryInstance->StartTime = FPlatformTime::Seconds();

	DEC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, QueryInstance->NamedParams.GetAllocatedSize());

	// @TODO: interface for providing default named params (like custom ranges in AI)
	QueryInstance->NamedParams = Request.NamedParams;

	INC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, QueryInstance->NamedParams.GetAllocatedSize());

	QueryInstance->QueryID = NextQueryID++;

	return QueryInstance;
}

bool UEnvQueryManager::AbortQuery(int32 RequestID)
{
	for (int32 QueryIndex = 0; QueryIndex < RunningQueries.Num(); QueryIndex++)
	{
		TSharedPtr<FEnvQueryInstance>& QueryInstance = RunningQueries[QueryIndex];
		if (QueryInstance->QueryID == RequestID &&
			QueryInstance->IsFinished() == false)
		{
			QueryInstance->MarkAsAborted();
			QueryInstance->FinishDelegate.ExecuteIfBound(QueryInstance);
			
			// We will remove the aborted query from the RunningQueries array on the next EQS update
			++NumRunningQueriesAbortedSinceLastUpdate;

			return true;
		}
	}

	return false;
}

void UEnvQueryManager::Tick(float DeltaTime)
{
	SCOPE_TIME_GUARD_MS(TEXT("UEnvQueryManager::Tick"), 10);
	SCOPE_CYCLE_COUNTER(STAT_AI_EQS_Tick);
	SET_DWORD_STAT(STAT_AI_EQS_NumInstances, RunningQueries.Num());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	CheckQueryCount();
#endif

	const float ExecutionTimeWarningSeconds = 0.025f;

	float TimeLeft = MaxAllowedTestingTime;
	int32 QueriesFinishedDuringUpdate = 0;

	{
		SCOPE_CYCLE_COUNTER(STAT_AI_EQS_TickWork);
		
		const int32 NumRunningQueries = RunningQueries.Num();
		int32 Index = 0;

		while ((TimeLeft > 0.0) 
			&& (Index < NumRunningQueries) 
			// make sure we account for queries that have finished (been aborted)
			// before UEnvQueryManager::Tick has been called
			&& (QueriesFinishedDuringUpdate + NumRunningQueriesAbortedSinceLastUpdate < NumRunningQueries))
		{
			const double StepStartTime = FPlatformTime::Seconds();
			float ResultHandlingDuration = 0.0f;
#if USE_EQS_DEBUGGER
			bool bWorkHasBeenDone = false;
#endif // USE_EQS_DEBUGGER

			TSharedPtr<FEnvQueryInstance> QueryInstance = RunningQueries[Index];
			FEnvQueryInstance* QueryInstancePtr = QueryInstance.Get();
			if (QueryInstancePtr == nullptr || QueryInstancePtr->IsFinished())
			{
				// If this query is already finished, skip it.
				++Index;
			}
			else
			{
				QueryInstancePtr->ExecuteOneStep(TimeLeft);
#if USE_EQS_DEBUGGER
				bWorkHasBeenDone = true;
#endif // USE_EQS_DEBUGGER
				if (QueryInstancePtr->IsFinished())
				{
					// Always log that we executed total execution time at the end of the query.
					if (QueryInstancePtr->TotalExecutionTime > ExecutionTimeWarningSeconds)
					{
						UE_LOG(LogEQS, Warning, TEXT("Finished query %s over execution time warning. %s"), *QueryInstancePtr->QueryName, *QueryInstancePtr->GetExecutionTimeDescription());
						QueryInstancePtr->bHasLoggedTimeLimitWarning = true;
					}

					// Now, handle the response to the query finishing, but calculate the time from that to remove from
					// the time spent for time-slicing purposes, because that's NOT the EQS manager doing work.
					{
						SCOPE_CYCLE_COUNTER(STAT_AI_EQS_TickNotifies);
						const double ResultHandlingStartTime = FPlatformTime::Seconds();
	
						UE_VLOG_EQS(*QueryInstancePtr, LogEQS, All);

#if USE_EQS_DEBUGGER
						EQSDebugger.StoreStats(*QueryInstancePtr);
						EQSDebugger.StoreQuery(QueryInstance);
#endif // USE_EQS_DEBUGGER

						QueryInstancePtr->FinishDelegate.ExecuteIfBound(QueryInstance);

						ResultHandlingDuration = FPlatformTime::Seconds() - ResultHandlingStartTime;
					}

					++QueriesFinishedDuringUpdate;
					++Index;
				}
				// If we're testing queries using breadth, move on to the next query.
				// If we're testing queries using depth, we only move on to the next query when we finish the current one.
				else if (bTestQueriesUsingBreadth)
				{
					++Index;
				}

				if (QueryInstancePtr->TotalExecutionTime > ExecutionTimeWarningSeconds && !QueryInstancePtr->bHasLoggedTimeLimitWarning)
				{
					UE_LOG(LogEQS, Warning, TEXT("Query %s over execution time warning. %s"), *QueryInstancePtr->QueryName, *QueryInstancePtr->GetExecutionTimeDescription());
					QueryInstancePtr->bHasLoggedTimeLimitWarning = true;
				}
			}

			// Start over at the beginning if we are testing using breadth and we've reached the end of the list
			if (bTestQueriesUsingBreadth && (Index == NumRunningQueries))
			{
				Index = 0;
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (bAllowEQSTimeSlicing) // if Time slicing is enabled...
#endif
			{	// Don't include the querier handling as part of the total time spent by EQS for time-slicing purposes.
				const float StepProcessingTime = ((FPlatformTime::Seconds() - StepStartTime) - ResultHandlingDuration);
				TimeLeft -= StepProcessingTime;

#if USE_EQS_DEBUGGER
				// we want to do any kind of logging only if any work has been done
				if (QueryInstancePtr && bWorkHasBeenDone)
				{
					EQSDebugger.StoreTickTime(*QueryInstancePtr, StepProcessingTime, MaxAllowedTestingTime);
				}
#endif // USE_EQS_DEBUGGER
			}
		}
	}

	{
		const int32 NumQueriesFinished = QueriesFinishedDuringUpdate + NumRunningQueriesAbortedSinceLastUpdate;
		float FinishedQueriesTotalTime = 0.0;

		if (NumQueriesFinished > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_AI_EQS_TickQueryRemovals);

			// When using breadth testing we don't know when a particular query will finish,
			// or if we have queries that were aborted since the last update we don't know which ones were aborted,
			// so we have to go through all the queries.
			// When doing depth without any queries aborted since the last update we know how many to remove.
			// Or if we have finished all the queries.  In that case we don't need to check if the queries are finished)
			if ((NumQueriesFinished != RunningQueries.Num()) && (bTestQueriesUsingBreadth || (NumRunningQueriesAbortedSinceLastUpdate > 0)))
			{
				for (int32 Index = RunningQueries.Num() - 1, FinishedQueriesCounter = NumQueriesFinished; Index >= 0 && FinishedQueriesCounter > 0; --Index)
				{
					TSharedPtr<FEnvQueryInstance>& QueryInstance = RunningQueries[Index];
					if (!QueryInstance.IsValid())
					{
						RunningQueries.RemoveAt(Index, 1, /*bAllowShrinking=*/false);
						continue;
					}

					if (QueryInstance->IsFinished())
					{
						FinishedQueriesTotalTime += (FPlatformTime::Seconds() - QueryInstance->StartTime);
						RunningQueries.RemoveAt(Index, 1, /*bAllowShrinking=*/false);
						--FinishedQueriesCounter;
					}
				}
			}
			else // queries tested using depth without any aborted queries since our last update, or we're removing all queries
			{
				for (int32 Index = 0; Index < NumQueriesFinished; ++Index)
				{
					TSharedPtr<FEnvQueryInstance>& QueryInstance = RunningQueries[Index];
					ensure(QueryInstance->IsFinished());

					FinishedQueriesTotalTime += (FPlatformTime::Seconds() - QueryInstance->StartTime);
				}

				RunningQueries.RemoveAt(0, NumQueriesFinished, /*bAllowShrinking=*/false);
			}
		}

		// Reset the running queries aborted since last update counter
		NumRunningQueriesAbortedSinceLastUpdate = 0;

		const float InstanceAverageResponseTime = (NumQueriesFinished > 0) ? (1000.0f * FinishedQueriesTotalTime / NumQueriesFinished) : 0.0f;
		SET_FLOAT_STAT(STAT_AI_EQS_AvgInstanceResponseTime, InstanceAverageResponseTime);
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UEnvQueryManager::CheckQueryCount() const
{
	if ((QueryCountWarningThreshold > 0) && (RunningQueries.Num() >= QueryCountWarningThreshold))
	{
		const double CurrentTime = FPlatformTime::Seconds();

		if ((LastQueryCountWarningThresholdTime < 0.0) || ((LastQueryCountWarningThresholdTime + QueryCountWarningInterval) < CurrentTime))
		{
			LogQueryCountWarning();

			LastQueryCountWarningThresholdTime = CurrentTime;
		}
	}
}

void UEnvQueryManager::LogQueryCountWarning() const
{
	UE_LOG(LogEQS, Warning, TEXT("The number of EQS queries has reached (%d) the warning threshold (%d).  Logging queries."), RunningQueries.Num(), QueryCountWarningThreshold);

	for (const TSharedPtr<FEnvQueryInstance>& RunningQuery : RunningQueries)
	{
		if (RunningQuery.IsValid())
		{
			UE_LOG(LogEQS, Warning, TEXT("Query: %s - Owner: %s"), *RunningQuery->QueryName, RunningQuery->Owner.IsValid() ? *RunningQuery->Owner->GetName() : TEXT("Invalid"));
		}
		else
		{
			UE_LOG(LogEQS, Warning, TEXT("Invalid query found in list!"));
		}
	}
}
#endif

void UEnvQueryManager::OnWorldCleanup()
{
	if (RunningQueries.Num() > 0)
	{
		// @todo investigate if this is even needed. We should be fine with just removing all queries
		TArray<TSharedPtr<FEnvQueryInstance> > RunningQueriesCopy = RunningQueries;
		RunningQueries.Reset();

		for (int32 Index = 0; Index < RunningQueriesCopy.Num(); Index++)
		{
			TSharedPtr<FEnvQueryInstance>& QueryInstance = RunningQueriesCopy[Index];
			if (QueryInstance->IsFinished() == false)
			{
				QueryInstance->MarkAsFailed();
				QueryInstance->FinishDelegate.ExecuteIfBound(QueryInstance);
			}
		}
	}

	GCShieldedWrappers.Reset();
}

void UEnvQueryManager::RegisterExternalQuery(const TSharedPtr<FEnvQueryInstance>& QueryInstance)
{
	if (QueryInstance.IsValid())
	{
		ExternalQueries.Add(QueryInstance->QueryID, QueryInstance);
	}
}

void UEnvQueryManager::UnregisterExternalQuery(const TSharedPtr<FEnvQueryInstance>& QueryInstance)
{
	if (QueryInstance.IsValid())
	{
		ExternalQueries.Remove(QueryInstance->QueryID);
	}
}

namespace EnvQueryTestSort
{
	struct FAllMatching
	{
		FORCEINLINE bool operator()(const UEnvQueryTest& TestA, const UEnvQueryTest& TestB) const
		{
			// cheaper tests go first
			if (TestB.Cost > TestA.Cost)
			{
				return true;
			}

			// conditions go first
			const bool bConditionA = (TestA.TestPurpose != EEnvTestPurpose::Score); // Is Test A Filtering?
			const bool bConditionB = (TestB.TestPurpose != EEnvTestPurpose::Score); // Is Test B Filtering?
			if (bConditionA && !bConditionB)
			{
				return true;
			}

			// keep connection order (sort stability)
			return (TestB.TestOrder > TestA.TestOrder);
		}
	};

	struct FSingleResult
	{
		FSingleResult(EEnvTestCost::Type InHighestCost) : HighestCost(InHighestCost) {}

		FORCEINLINE bool operator()(const UEnvQueryTest& TestA, const UEnvQueryTest& TestB) const
		{
			// cheaper tests go first
			if (TestB.Cost > TestA.Cost)
			{
				return true;
			}

			const bool bConditionA = (TestA.TestPurpose != EEnvTestPurpose::Score); // Is Test A Filtering?
			const bool bConditionB = (TestB.TestPurpose != EEnvTestPurpose::Score); // Is Test B Filtering?
			if (TestA.Cost == HighestCost)
			{
				// highest cost: weights go first, conditions later (first match will return result)
				if (!bConditionA && bConditionB)
				{
					return true;
				}
			}
			else
			{
				// lower costs: conditions go first to reduce amount of items
				if (bConditionA && !bConditionB)
				{
					return true;
				}
			}

			// keep connection order (sort stability)
			return (TestB.TestOrder > TestA.TestOrder);
		}

	protected:
		TEnumAsByte<EEnvTestCost::Type> HighestCost;
	};
}

UEnvQuery* UEnvQueryManager::FindQueryTemplate(const FString& QueryName) const
{
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceCache.Num(); InstanceIndex++)
	{
		const UEnvQuery* QueryTemplate = InstanceCache[InstanceIndex].Template;

		if (QueryTemplate != NULL && QueryTemplate->GetName() == QueryName)
		{
			return const_cast<UEnvQuery*>(QueryTemplate);
		}
	}

	for (FObjectIterator It(UEnvQuery::StaticClass()); It; ++It)
	{
		if (It->GetName() == QueryName)
		{
			return Cast<UEnvQuery>(*It);
		}
	}

	return NULL;
}

TSharedPtr<FEnvQueryInstance> UEnvQueryManager::CreateQueryInstance(const UEnvQuery* Template, EEnvQueryRunMode::Type RunMode)
{
	if (Template == nullptr || Template->Options.Num() == 0)
	{
		UE_CLOG(Template != nullptr && Template->Options.Num() == 0, LogEQS, Warning, TEXT("Query [%s] doesn't have any valid options!"), *Template->GetName());
		return nullptr;
	}

	// try to find entry in cache
	FEnvQueryInstance* InstanceTemplate = NULL;
	for (int32 InstanceIndex = 0; InstanceIndex < InstanceCache.Num(); InstanceIndex++)
	{
		if (InstanceCache[InstanceIndex].AssetName == Template->GetFName() &&
			InstanceCache[InstanceIndex].Instance.Mode == RunMode)
		{
			InstanceTemplate = &InstanceCache[InstanceIndex].Instance;
			break;
		}
	}

	// and create one if can't be found
	if (InstanceTemplate == NULL)
	{
		SCOPE_CYCLE_COUNTER(STAT_AI_EQS_LoadTime);
		static const UEnum* RunModeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EEnvQueryRunMode"));

		// duplicate template in manager's world for BP based nodes
		const FString NewInstanceName = RunModeEnum
			? FString::Printf(TEXT("%s_%s"), *Template->GetFName().ToString(), *RunModeEnum->GetNameStringByValue(RunMode))
			: FString::Printf(TEXT("%s_%d"), *Template->GetFName().ToString(), uint8(RunMode));
		UEnvQuery* LocalTemplate = (UEnvQuery*)StaticDuplicateObject(Template, this, *NewInstanceName);

		{
			// memory stat tracking: temporary variable will exist only inside this section
			FEnvQueryInstanceCache NewCacheEntry;
			NewCacheEntry.AssetName = Template->GetFName();
			NewCacheEntry.Template = LocalTemplate;
			NewCacheEntry.Instance.UniqueName = LocalTemplate->GetFName();
			NewCacheEntry.Instance.QueryName = LocalTemplate->GetQueryName().ToString();
			NewCacheEntry.Instance.Mode = RunMode;

			const int32 Idx = InstanceCache.Add(NewCacheEntry);
			InstanceTemplate = &InstanceCache[Idx].Instance;
		}

		// NOTE: We must iterate over this from 0->Num because we are copying the options from the template into the
		// instance, and order matters!  Since we also may need to remove invalid or null options, we must decrement
		// the iteration pointer when doing so to avoid problems.
		int32 SourceOptionIndex = 0;
		for (int32 OptionIndex = 0; OptionIndex < LocalTemplate->Options.Num(); ++OptionIndex, ++SourceOptionIndex)
		{
			UEnvQueryOption* MyOption = LocalTemplate->Options[OptionIndex];
			if (MyOption == nullptr ||
				MyOption->Generator == nullptr ||
				MyOption->Generator->ItemType == nullptr)
			{
				UE_LOG(LogEQS, Error, TEXT("Trying to spawn a query with broken Template (generator:%s itemType:%s): %s, option %d"),
					MyOption ? (MyOption->Generator ? TEXT("ok") : TEXT("MISSING")) : TEXT("N/A"),
					(MyOption && MyOption->Generator) ? (MyOption->Generator->ItemType ? TEXT("ok") : TEXT("MISSING")) : TEXT("N/A"),
					*GetNameSafe(LocalTemplate), OptionIndex);

				LocalTemplate->Options.RemoveAt(OptionIndex, 1, false);
				--OptionIndex; // See note at top of for loop.  We cannot iterate backwards here.
				continue;
			}

			UEnvQueryOption* LocalOption = (UEnvQueryOption*)StaticDuplicateObject(MyOption, this);
			UEnvQueryGenerator* LocalGenerator = (UEnvQueryGenerator*)StaticDuplicateObject(MyOption->Generator, this);
			LocalTemplate->Options[OptionIndex] = LocalOption;
			LocalOption->Generator = LocalGenerator;

			// check if TestOrder property is set correctly in asset, try to recreate it if not
			// don't use editor graph, so any disabled tests in the middle will make it look weird
			if (MyOption->Tests.Num() > 1 && MyOption->Tests.Last() && MyOption->Tests.Last()->TestOrder == 0)
			{
				for (int32 TestIndex = 0; TestIndex < MyOption->Tests.Num(); TestIndex++)
				{
					if (MyOption->Tests[TestIndex])
					{
						MyOption->Tests[TestIndex]->TestOrder = TestIndex;
					}
				}
			}

			EEnvTestCost::Type HighestCost(EEnvTestCost::Low);
			TArray<UEnvQueryTest*> SortedTests = MyOption->Tests;
			TSubclassOf<UEnvQueryItemType> GeneratedType = MyOption->Generator->ItemType;
			for (int32 TestIndex = SortedTests.Num() - 1; TestIndex >= 0; TestIndex--)
			{
				UEnvQueryTest* TestOb = SortedTests[TestIndex];
				if (TestOb == NULL || !TestOb->IsSupportedItem(GeneratedType))
				{
					UE_LOG(LogEQS, Warning, TEXT("Query [%s] can't use test [%s] in option %d [%s], removing it"),
						*GetNameSafe(LocalTemplate), *GetNameSafe(TestOb), OptionIndex, *MyOption->Generator->OptionName);

					SortedTests.RemoveAt(TestIndex, 1, false);
				}
				else if (HighestCost < TestOb->Cost)
				{
					HighestCost = TestOb->Cost;
				}
			}

			LocalOption->Tests.Reset(SortedTests.Num());
			for (int32 TestIdx = 0; TestIdx < SortedTests.Num(); TestIdx++)
			{
				UEnvQueryTest* LocalTest = (UEnvQueryTest*)StaticDuplicateObject(SortedTests[TestIdx], this);
				LocalOption->Tests.Add(LocalTest);
			}

			// use locally referenced duplicates
			SortedTests = LocalOption->Tests;

			if (SortedTests.Num() && LocalGenerator->bAutoSortTests)
			{
				switch (RunMode)
				{
				case EEnvQueryRunMode::SingleResult:
					SortedTests.Sort(EnvQueryTestSort::FSingleResult(HighestCost));
					break;

				case EEnvQueryRunMode::RandomBest5Pct:
				case EEnvQueryRunMode::RandomBest25Pct:
				case EEnvQueryRunMode::AllMatching:
					SortedTests.Sort(EnvQueryTestSort::FAllMatching());
					break;

				default:
					{
						UE_LOG(LogEQS, Warning, TEXT("Query [%s] can't be sorted for RunMode: %d [%s]"),
							*GetNameSafe(LocalTemplate), (int32)RunMode, RunModeEnum ? *RunModeEnum->GetNameStringByValue(RunMode) : TEXT("??"));
					}
				}
			}

			CreateOptionInstance(LocalOption, SourceOptionIndex, SortedTests, *InstanceTemplate);
		}
	}

	if (InstanceTemplate->Options.Num() == 0)
	{
		return nullptr;
	}

	// create new instance
	TSharedPtr<FEnvQueryInstance> NewInstance(new FEnvQueryInstance(*InstanceTemplate));
	return NewInstance;
}

void UEnvQueryManager::CreateOptionInstance(UEnvQueryOption* OptionTemplate, int32 SourceOptionIndex, const TArray<UEnvQueryTest*>& SortedTests, FEnvQueryInstance& Instance)
{
	FEnvQueryOptionInstance OptionInstance;
	OptionInstance.Generator = OptionTemplate->Generator;
	OptionInstance.ItemType = OptionTemplate->Generator->ItemType;
	OptionInstance.SourceOptionIndex = SourceOptionIndex;

	OptionInstance.Tests.AddZeroed(SortedTests.Num());
	for (int32 TestIndex = 0; TestIndex < SortedTests.Num(); TestIndex++)
	{
		UEnvQueryTest* TestOb = SortedTests[TestIndex];
		OptionInstance.Tests[TestIndex] = TestOb;
	}

	DEC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, Instance.Options.GetAllocatedSize());

	const int32 AddedIdx = Instance.Options.Add(OptionInstance);

#if USE_EQS_DEBUGGER
	Instance.DebugData.OptionData.AddDefaulted(1);
	FEnvQueryDebugProfileData::FOptionData& OptionData = Instance.DebugData.OptionData.Last();
	
	OptionData.OptionIdx = SourceOptionIndex;
	for (int32 TestIndex = 0; TestIndex < SortedTests.Num(); TestIndex++)
	{
		UEnvQueryTest* TestOb = SortedTests[TestIndex];
		OptionData.TestIndices.Add(TestOb->TestOrder);
	}
#endif

	INC_MEMORY_STAT_BY(STAT_AI_EQS_InstanceMemory, Instance.Options.GetAllocatedSize() + Instance.Options[AddedIdx].GetAllocatedSize());
}

UEnvQueryContext* UEnvQueryManager::PrepareLocalContext(TSubclassOf<UEnvQueryContext> ContextClass)
{
	UEnvQueryContext* LocalContext = LocalContextMap.FindRef(ContextClass->GetFName());
	if (LocalContext == NULL)
	{
		LocalContext = (UEnvQueryContext*)StaticDuplicateObject(ContextClass.GetDefaultObject(), this);
		LocalContexts.Add(LocalContext);
		LocalContextMap.Add(ContextClass->GetFName(), LocalContext);
	}

	return LocalContext;
}

float UEnvQueryManager::FindNamedParam(int32 QueryId, FName ParamName) const
{
	float ParamValue = 0.0f;

	const TWeakPtr<FEnvQueryInstance>* QueryInstancePtr = ExternalQueries.Find(QueryId);
	if (QueryInstancePtr)
	{
		TSharedPtr<FEnvQueryInstance> QueryInstance = (*QueryInstancePtr).Pin();
		if (QueryInstance.IsValid())
		{
			ParamValue = QueryInstance->NamedParams.FindRef(ParamName);
		}
	}
	else
	{
		for (int32 QueryIndex = 0; QueryIndex < RunningQueries.Num(); QueryIndex++)
		{
			const TSharedPtr<FEnvQueryInstance>& QueryInstance = RunningQueries[QueryIndex];
			if (QueryInstance->QueryID == QueryId)
			{
				ParamValue = QueryInstance->NamedParams.FindRef(ParamName);
				break;
			}
		}
	}

	return ParamValue;
}

//----------------------------------------------------------------------//
// BP functions and related functionality 
//----------------------------------------------------------------------//
UEnvQueryInstanceBlueprintWrapper* UEnvQueryManager::RunEQSQuery(UObject* WorldContextObject, UEnvQuery* QueryTemplate, UObject* Querier, TEnumAsByte<EEnvQueryRunMode::Type> RunMode, TSubclassOf<UEnvQueryInstanceBlueprintWrapper> WrapperClass)
{ 
	if (QueryTemplate == nullptr || Querier == nullptr)
	{
		return nullptr;
	}

	UEnvQueryManager* EQSManager = GetCurrent(WorldContextObject);
	UEnvQueryInstanceBlueprintWrapper* QueryInstanceWrapper = nullptr;

	if (EQSManager)
	{
		bool bValidQuerier = true;

		// convert controller-owners to pawns, unless specifically configured not to do so
		if (GET_AI_CONFIG_VAR(bAllowControllersAsEQSQuerier) == false && Cast<AController>(Querier))
		{
			AController* Controller = Cast<AController>(Querier);
			if (Controller->GetPawn())
			{
				Querier = Controller->GetPawn();
			}
			else
			{
				UE_VLOG(Controller, LogEQS, Error, TEXT("Trying to run EQS query while not having a pawn! Aborting."));
				bValidQuerier = false;
			}
		}

		if (bValidQuerier)
		{
			QueryInstanceWrapper = NewObject<UEnvQueryInstanceBlueprintWrapper>(EQSManager, (UClass*)(WrapperClass) ? (UClass*)WrapperClass : UEnvQueryInstanceBlueprintWrapper::StaticClass());
			check(QueryInstanceWrapper);

			FEnvQueryRequest QueryRequest(QueryTemplate, Querier);
			// @todo named params still missing support
			//QueryRequest.SetNamedParams(QueryParams);
			QueryInstanceWrapper->SetInstigator(WorldContextObject);
			QueryInstanceWrapper->RunQuery(RunMode, QueryRequest);
		}
	}
	
	return QueryInstanceWrapper;
}

void UEnvQueryManager::RegisterActiveWrapper(UEnvQueryInstanceBlueprintWrapper& Wrapper)
{
	GCShieldedWrappers.AddUnique(&Wrapper);
}

void UEnvQueryManager::UnregisterActiveWrapper(UEnvQueryInstanceBlueprintWrapper& Wrapper)
{
	GCShieldedWrappers.RemoveSingleSwap(&Wrapper, /*bAllowShrinking=*/false);
}

TSharedPtr<FEnvQueryInstance> UEnvQueryManager::FindQueryInstance(const int32 QueryID)
{
	if (QueryID != INDEX_NONE)
	{
		// going from the back since it's most probably there
		for (int32 QueryIndex = RunningQueries.Num() - 1; QueryIndex >= 0; --QueryIndex)
		{
			if (RunningQueries[QueryIndex]->QueryID == QueryID)
			{
				return RunningQueries[QueryIndex];
			}
		}
	}

	return nullptr;
}

//----------------------------------------------------------------------//
// Exec functions (i.e. console commands)
//----------------------------------------------------------------------//
void UEnvQueryManager::SetAllowTimeSlicing(bool bAllowTimeSlicing)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bAllowEQSTimeSlicing = bAllowTimeSlicing;

	UE_LOG(LogEQS, Log, TEXT("Set allow time slicing to %s."),
			bAllowEQSTimeSlicing ? TEXT("true") : TEXT("false"));
#else
	UE_LOG(LogEQS, Log, TEXT("Time slicing cannot be disabled in Test or Shipping builds.  SetAllowTimeSlicing does nothing."));
#endif
}

bool UEnvQueryManager::Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar)
{
#if USE_EQS_DEBUGGER
	if (FParse::Command(&Cmd, TEXT("DumpEnvQueryStats")))
	{
		const FString FileName = FPaths::CreateTempFilename(*FPaths::ProjectLogDir(), TEXT("EnvQueryStats"), TEXT(".ue4eqs"));

		FEQSDebugger::SaveStats(FileName);
		return true;
	}
#endif

	return false;
}


//----------------------------------------------------------------------//
// FEQSDebugger
//----------------------------------------------------------------------//
#if USE_EQS_DEBUGGER

void FEQSDebugger::StoreStats(const FEnvQueryInstance& QueryInstance)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_EQS_Debug_StoreStats);

	FStatsInfo& UpdateInfo = UEnvQueryManager::DebuggerStats.FindOrAdd(FName(*QueryInstance.QueryName));

	const FEnvQueryDebugProfileData& QueryStats = QueryInstance.DebugData;
	const float ExecutionTime = QueryInstance.TotalExecutionTime;
	
	if (ExecutionTime > UpdateInfo.MostExpensiveDuration)
	{
		UpdateInfo.MostExpensiveDuration = ExecutionTime;
		UpdateInfo.MostExpensive = QueryStats;
	}

	// better solution for counting average?
	if (UpdateInfo.TotalAvgCount >= 100000)
	{
		UpdateInfo.TotalAvgData = QueryStats;
		UpdateInfo.TotalAvgDuration = ExecutionTime;
		UpdateInfo.TotalAvgCount = 1;
	}
	else
	{
		UpdateInfo.TotalAvgData.Add(QueryStats);
		UpdateInfo.TotalAvgDuration += ExecutionTime;
		UpdateInfo.TotalAvgCount++;
	}
}

void FEQSDebugger::StoreQuery(const TSharedPtr<FEnvQueryInstance>& QueryInstance)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_EQS_Debug_StoreQuery);

	StoredQueries.Remove(nullptr);
	if (!QueryInstance.IsValid() || QueryInstance->World == nullptr)
	{
		return;
	}

	TArray<FEnvQueryInfo>& AllQueries = StoredQueries.FindOrAdd(QueryInstance->Owner.Get());
	int32 QueryIdx = INDEX_NONE;

	for (int32 Idx = 0; Idx < AllQueries.Num(); Idx++)
	{
		const FEnvQueryInfo& TestInfo = AllQueries[Idx];
		if (TestInfo.Instance.IsValid() && QueryInstance->QueryName == TestInfo.Instance->QueryName)
		{
			QueryIdx = Idx;
			break;
		}
	}

	if (QueryIdx == INDEX_NONE)
	{
		QueryIdx = AllQueries.AddDefaulted(1);
	}

	FEnvQueryInfo& UpdateInfo = AllQueries[QueryIdx];
	UpdateInfo.Instance = QueryInstance;
	UpdateInfo.Timestamp = QueryInstance->World->GetTimeSeconds();
}

void FEQSDebugger::StoreTickTime(const FEnvQueryInstance& QueryInstance, float TickTime, float MaxTickTime)
{
#if USE_EQS_TICKLOADDATA
	SCOPE_CYCLE_COUNTER(STAT_AI_EQS_Debug_StoreTickTime);

	const int32 NumRecordedTicks = 0x4000;

	FStatsInfo& UpdateInfo = UEnvQueryManager::DebuggerStats.FindOrAdd(FName(*QueryInstance.QueryName));
	if (UpdateInfo.TickPct.Num() != NumRecordedTicks)
	{
		UpdateInfo.TickPct.Reset();
		UpdateInfo.TickPct.AddZeroed(NumRecordedTicks);
	}

	if (UpdateInfo.LastTickFrame != GFrameCounter)
	{
		UpdateInfo.LastTickFrame = GFrameCounter;
		UpdateInfo.LastTickTime = 0.0f;
	}

	const uint16 TickIdx = GFrameCounter & (NumRecordedTicks - 1);
	UpdateInfo.FirstTickEntry = (TickIdx < UpdateInfo.FirstTickEntry) ? TickIdx : UpdateInfo.FirstTickEntry;
	UpdateInfo.LastTickEntry = (TickIdx > UpdateInfo.LastTickEntry) ? TickIdx : UpdateInfo.LastTickEntry;

	UpdateInfo.LastTickTime += TickTime;
	UpdateInfo.TickPct[TickIdx] = FMath::Min(255, FMath::TruncToInt(255 * UpdateInfo.LastTickTime / MaxTickTime));
#endif // USE_EQS_TICKLOADDATA
}

const TArray<FEQSDebugger::FEnvQueryInfo>& FEQSDebugger::GetAllQueriesForOwner(const UObject* Owner)
{
	TArray<FEnvQueryInfo>& AllQueries = StoredQueries.FindOrAdd(Owner);
	return AllQueries;
}

FArchive& operator<<(FArchive& Ar, FEQSDebugger::FStatsInfo& Data)
{
	int32 VersionNum = 1;
	Ar << VersionNum;

	Ar << Data.MostExpensive;
	Ar << Data.MostExpensiveDuration;
	Ar << Data.TotalAvgData;
	Ar << Data.TotalAvgDuration;
	Ar << Data.TotalAvgCount;
	Ar << Data.TickPct;
	Ar << Data.FirstTickEntry;
	Ar << Data.LastTickEntry;
	return Ar;
}

void FEQSDebugger::SaveStats(const FString& FileName)
{
	FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*FileName);
	if (FileWriter)
	{
		int32 NumRecords = UEnvQueryManager::DebuggerStats.Num();
		(*FileWriter) << NumRecords;

		for (auto It : UEnvQueryManager::DebuggerStats)
		{
			FString QueryName = It.Key.ToString();
			(*FileWriter) << QueryName;
			(*FileWriter) << It.Value;
		}

		FileWriter->Close();
		delete FileWriter;

		UE_LOG(LogEQS, Display, TEXT("EQS debugger data saved! File: %s"), *FileName);
	}
}

void FEQSDebugger::LoadStats(const FString& FileName)
{
	FArchive* FileReader = IFileManager::Get().CreateFileReader(*FileName);
	if (FileReader)
	{
		UEnvQueryManager::DebuggerStats.Reset();

		int32 NumRecords = UEnvQueryManager::DebuggerStats.Num();
		(*FileReader) << NumRecords;

		for (int32 Idx = 0; Idx < NumRecords; Idx++)
		{
			FString QueryName;
			FEQSDebugger::FStatsInfo Stats;

			(*FileReader) << QueryName;
			(*FileReader) << Stats;

			UEnvQueryManager::DebuggerStats.Add(*QueryName, Stats);
		}

		FileReader->Close();
		delete FileReader;
	}
}

#endif // USE_EQS_DEBUGGER
