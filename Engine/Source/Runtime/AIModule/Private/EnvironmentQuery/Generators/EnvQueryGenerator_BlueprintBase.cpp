// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQuery/Generators/EnvQueryGenerator_BlueprintBase.h"
#include "GameFramework/Actor.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_ActorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryGenerator_BlueprintBase::UEnvQueryGenerator_BlueprintBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Context = UEnvQueryContext_Querier::StaticClass();
	ItemType = UEnvQueryItemType_Actor::StaticClass();
	GeneratedItemType = UEnvQueryItemType_Actor::StaticClass();
}

void UEnvQueryGenerator_BlueprintBase::PostInitProperties()
{
	Super::PostInitProperties();
	ItemType = GeneratedItemType;
}

UWorld* UEnvQueryGenerator_BlueprintBase::GetWorld() const
{
	return CachedQueryInstance ? CachedQueryInstance->World : NULL;
}

void UEnvQueryGenerator_BlueprintBase::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	TArray<FVector> ContextLocations;
	QueryInstance.PrepareContext(Context, ContextLocations);

	CachedQueryInstance = &QueryInstance;
	const_cast<UEnvQueryGenerator_BlueprintBase*>(this)->DoItemGeneration(ContextLocations);
	CachedQueryInstance = NULL;
}

void UEnvQueryGenerator_BlueprintBase::AddGeneratedVector(FVector Vector) const
{
	check(CachedQueryInstance);
	if (ensure(ItemType->IsChildOf<UEnvQueryItemType_ActorBase>() == false))
	{
		CachedQueryInstance->AddItemData<UEnvQueryItemType_Point>(Vector);
	}
	else
	{
		UE_LOG(LogEQS, Error, TEXT("Trying to generate a Vector item while generator %s is configured to produce Actor items")
			, *GetName());
	}
}

void UEnvQueryGenerator_BlueprintBase::AddGeneratedActor(AActor* Actor) const
{
	check(CachedQueryInstance);
	if (ensure(ItemType->IsChildOf<UEnvQueryItemType_ActorBase>()))
	{
		CachedQueryInstance->AddItemData<UEnvQueryItemType_Actor>(Actor);
	}
	else
	{
		UE_LOG(LogEQS, Error, TEXT("Trying to generate an Actor item while generator %s is configured to produce Vector items. Will use Actor\'s location, but please update your BP code.")
			, *GetName());
		if (Actor)
		{
			CachedQueryInstance->AddItemData<UEnvQueryItemType_Point>(Actor->GetActorLocation());
		}
	}
}

UObject* UEnvQueryGenerator_BlueprintBase::GetQuerier() const
{
	check(CachedQueryInstance);
	return CachedQueryInstance->Owner.Get();
}

FText UEnvQueryGenerator_BlueprintBase::GetDescriptionTitle() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("DescriptionTitle"), Super::GetDescriptionTitle());
	Args.Add(TEXT("DescribeGeneratorAction"), GeneratorsActionDescription);
	Args.Add(TEXT("DescribeContext"), UEnvQueryTypes::DescribeContext(Context));

	return FText::Format(LOCTEXT("DescriptionBlueprintImplementedGenerator", "{DescriptionTitle}: {DescribeGeneratorAction} around {DescribeContext}"), Args);
}

FText UEnvQueryGenerator_BlueprintBase::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("None"));
}

#undef LOCTEXT_NAMESPACE
