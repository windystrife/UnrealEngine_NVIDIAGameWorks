// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQuery/Generators/EnvQueryGenerator_SimpleGrid.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryGenerator_SimpleGrid::UEnvQueryGenerator_SimpleGrid(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	GenerateAround = UEnvQueryContext_Querier::StaticClass();
	GridSize.DefaultValue = 500.0f;
	SpaceBetween.DefaultValue = 100.0f;
}

void UEnvQueryGenerator_SimpleGrid::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	UObject* BindOwner = QueryInstance.Owner.Get();
	GridSize.BindData(BindOwner, QueryInstance.QueryID);
	SpaceBetween.BindData(BindOwner, QueryInstance.QueryID);

	float RadiusValue = GridSize.GetValue();
	float DensityValue = SpaceBetween.GetValue();

	const int32 ItemCount = FPlatformMath::TruncToInt((RadiusValue * 2.0f / DensityValue) + 1);
	const int32 ItemCountHalf = ItemCount / 2;

	TArray<FVector> ContextLocations;
	QueryInstance.PrepareContext(GenerateAround, ContextLocations);

	TArray<FNavLocation> GridPoints;
	GridPoints.Reserve(ItemCount * ItemCount * ContextLocations.Num());

	for (int32 ContextIndex = 0; ContextIndex < ContextLocations.Num(); ContextIndex++)
	{
		for (int32 IndexX = 0; IndexX < ItemCount; ++IndexX)
		{
			for (int32 IndexY = 0; IndexY < ItemCount; ++IndexY)
			{
				const FNavLocation TestPoint = FNavLocation(ContextLocations[ContextIndex] - FVector(DensityValue * (IndexX - ItemCountHalf), DensityValue * (IndexY - ItemCountHalf), 0));
				GridPoints.Add(TestPoint);
			}
		}
	}

	ProjectAndFilterNavPoints(GridPoints, QueryInstance);
	StoreNavPoints(GridPoints, QueryInstance);
}

FText UEnvQueryGenerator_SimpleGrid::GetDescriptionTitle() const
{
	return FText::Format(LOCTEXT("SimpleGridDescriptionGenerateAroundContext", "{0}: generate around {1}"),
		Super::GetDescriptionTitle(), UEnvQueryTypes::DescribeContext(GenerateAround));
};

FText UEnvQueryGenerator_SimpleGrid::GetDescriptionDetails() const
{
	FText Desc = FText::Format(LOCTEXT("SimpleGridDescription", "radius: {0}, space between: {1}"),
		FText::FromString(GridSize.ToString()), FText::FromString(SpaceBetween.ToString()));

	FText ProjDesc = ProjectionData.ToText(FEnvTraceData::Brief);
	if (!ProjDesc.IsEmpty())
	{
		FFormatNamedArguments ProjArgs;
		ProjArgs.Add(TEXT("Description"), Desc);
		ProjArgs.Add(TEXT("ProjectionDescription"), ProjDesc);
		Desc = FText::Format(LOCTEXT("SimpleGridDescriptionWithProjection", "{Description}, {ProjectionDescription}"), ProjArgs);
	}

	return Desc;
}

#undef LOCTEXT_NAMESPACE
