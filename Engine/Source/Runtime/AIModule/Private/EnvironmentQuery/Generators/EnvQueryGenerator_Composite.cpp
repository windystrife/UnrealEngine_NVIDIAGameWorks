// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQuery/Generators/EnvQueryGenerator_Composite.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryGenerator_Composite::UEnvQueryGenerator_Composite(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ItemType = UEnvQueryItemType_Point::StaticClass();
	bHasMatchingItemType = true;
}

void UEnvQueryGenerator_Composite::GenerateItems(FEnvQueryInstance& QueryInstance) const 
{
	if (bHasMatchingItemType)
	{
		for (int32 Idx = 0; Idx < Generators.Num(); Idx++)
		{
			if (Generators[Idx])
			{
				FScopeCycleCounterUObject GeneratorScope(Generators[Idx]);
				Generators[Idx]->GenerateItems(QueryInstance);
			}
		}
	}
}

FText UEnvQueryGenerator_Composite::GetDescriptionTitle() const
{
	FText Desc = Super::GetDescriptionTitle();
	for (int32 Idx = 0; Idx < Generators.Num(); Idx++)
	{
		if (Generators[Idx])
		{
			Desc = FText::Format(LOCTEXT("DescTitleExtention", "{0}\n  {1}"), Desc, Generators[Idx]->GetDescriptionTitle());
		}
	}

	return Desc;
};

void UEnvQueryGenerator_Composite::VerifyItemTypes()
{
	TSubclassOf<UEnvQueryItemType> CommonItemType = nullptr;
	bHasMatchingItemType = true;

	if (bAllowDifferentItemTypes)
	{
		// ignore safety and force user specified item type
		// REQUIRES proper memory layout between forced type and ALL item types used by child generators
		// this is advanced option and will NOT be validated!

		CommonItemType = ForcedItemType;
		bHasMatchingItemType = (ForcedItemType != nullptr);
	}
	else
	{
		for (int32 Idx = 0; Idx < Generators.Num(); Idx++)
		{
			if (Generators[Idx])
			{
				if (CommonItemType)
				{
					if (CommonItemType != Generators[Idx]->ItemType)
					{
						bHasMatchingItemType = false;
						break;
					}
				}
				else
				{
					CommonItemType = Generators[Idx]->ItemType;
				}
			}
		}
	}

	if (bHasMatchingItemType)
	{
		ItemType = CommonItemType;
	}
	else
	{
		// any type will do, generator is not allowed to create items anyway
		ItemType = UEnvQueryItemType_Point::StaticClass();
	}
}

#undef LOCTEXT_NAMESPACE
