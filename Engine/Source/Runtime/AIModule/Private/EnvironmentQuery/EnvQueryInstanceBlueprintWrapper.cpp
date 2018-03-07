// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQuery/EnvQueryInstanceBlueprintWrapper.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_ActorBase.h"
#include "EnvironmentQuery/EnvQueryManager.h"

UEnvQueryInstanceBlueprintWrapper::UEnvQueryInstanceBlueprintWrapper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, QueryID(INDEX_NONE)
{

}

void UEnvQueryInstanceBlueprintWrapper::OnQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	check(Result.IsValid());
	QueryResult = Result;
	ItemType = Result->ItemType;
	OptionIndex = Result->OptionIndex;

	OnQueryFinishedEvent.Broadcast(this, Result->GetRawStatus());

	// remove our reference to the query instance
	QueryInstance = nullptr;

	// unregister self, no longer shielded from GC
	UEnvQueryManager* EnvQueryManager = Cast<UEnvQueryManager>(GetOuter());
	if (ensure(EnvQueryManager))
	{
		EnvQueryManager->UnregisterActiveWrapper(*this);
	}
}

float UEnvQueryInstanceBlueprintWrapper::GetItemScore(int32 ItemIndex)
{
	return QueryResult.IsValid() ? QueryResult->GetItemScore(ItemIndex) : -1.f;
}

TArray<AActor*> UEnvQueryInstanceBlueprintWrapper::GetResultsAsActors()
{
	TArray<AActor*> Results;

	if (QueryResult.IsValid() && ItemType->IsChildOf(UEnvQueryItemType_ActorBase::StaticClass()))
	{
		if (RunMode != EEnvQueryRunMode::AllMatching)
		{
			Results.Add(QueryResult->GetItemAsActor(0));
		}
		else
		{
			QueryResult->GetAllAsActors(Results);
		}
	}

	return Results;
}

TArray<FVector> UEnvQueryInstanceBlueprintWrapper::GetResultsAsLocations()
{
	TArray<FVector> Results;

	if (QueryResult.IsValid())
	{
		if (RunMode != EEnvQueryRunMode::AllMatching)
		{
			Results.Add(QueryResult->GetItemAsLocation(0));
		}
		else
		{
			QueryResult->GetAllAsLocations(Results);
		}
	}

	return Results;
}

void UEnvQueryInstanceBlueprintWrapper::RunQuery(const EEnvQueryRunMode::Type InRunMode, FEnvQueryRequest& QueryRequest)
{
	RunMode = InRunMode;
	QueryID = QueryRequest.Execute(RunMode, this, &UEnvQueryInstanceBlueprintWrapper::OnQueryFinished);
	if (QueryID != INDEX_NONE)
	{
		// register self as a wrapper needing shielding from GC
		UEnvQueryManager* EnvQueryManager = Cast<UEnvQueryManager>(GetOuter());
		if (ensure(EnvQueryManager))
		{
			EnvQueryManager->RegisterActiveWrapper(*this);
			QueryInstance = EnvQueryManager->FindQueryInstance(QueryID);
		}
	}
}

void UEnvQueryInstanceBlueprintWrapper::SetNamedParam(FName ParamName, float Value)
{
	FEnvQueryInstance* InstancePtr = QueryInstance.Get();
	if (InstancePtr != nullptr)
	{
		InstancePtr->NamedParams.Add(ParamName, Value);
	}
}

void UEnvQueryInstanceBlueprintWrapper::SetInstigator(const UObject* Object)
{
#if !UE_BUILD_SHIPPING
	Instigator = Object;
#endif // !UE_BUILD_SHIPPING
}

bool UEnvQueryInstanceBlueprintWrapper::IsSupportedForNetworking() const
{
	// this object can't be replicated, but there are dragons in the land of blueprint...
	FEnvQueryInstance* InstancePtr = QueryInstance.Get();
	FString InstigatorName = TEXT("not available in shipping");
#if !UE_BUILD_SHIPPING
	InstigatorName = *GetNameSafe(Instigator.Get());
#endif // !UE_BUILD_SHIPPING

	UE_LOG(LogEQS, Warning, TEXT("%s can't be replicated over network!\n- Query: %s\n- Querier:%s\n- Instigator:%s"),
		*GetName(),
		InstancePtr ? *InstancePtr->QueryName : TEXT("instance not found"),
		InstancePtr ? *GetNameSafe(InstancePtr->Owner.Get()) : TEXT("instance not found"),
		*InstigatorName);

	return false;
}
