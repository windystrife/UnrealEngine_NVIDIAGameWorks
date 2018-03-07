// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQuery/Generators/EnvQueryGenerator_CurrentLocation.h"
#include "AI/Navigation/NavigationTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryGenerator_CurrentLocation::UEnvQueryGenerator_CurrentLocation(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	QueryContext = UEnvQueryContext_Querier::StaticClass();
	ItemType = UEnvQueryItemType_Point::StaticClass();
}

void UEnvQueryGenerator_CurrentLocation::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	TArray<FVector> ContextLocations;
	QueryInstance.PrepareContext(QueryContext, ContextLocations);

	for (const FVector& Location : ContextLocations)
	{
		FNavLocation NavLoc(Location);
		QueryInstance.AddItemData<UEnvQueryItemType_Point>(NavLoc);
	}
}

FText UEnvQueryGenerator_CurrentLocation::GetDescriptionTitle() const
{
	return FText::Format(LOCTEXT("CurrentLocationOn", "Current Location of {0}"), UEnvQueryTypes::DescribeContext(QueryContext));
};

FText UEnvQueryGenerator_CurrentLocation::GetDescriptionDetails() const
{
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
