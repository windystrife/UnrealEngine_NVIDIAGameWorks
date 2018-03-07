// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "EnvironmentQuery/Items/EnvQueryItemType.h"
#include "EnvironmentQuery/EnvQueryNode.h"
#include "EnvQueryGenerator.generated.h"

namespace EnvQueryGeneratorVersion
{
	const int32 Initial = 0;
	const int32 DataProviders = 1;

	const int32 Latest = DataProviders;
}

UCLASS(EditInlineNew, Abstract, meta = (Category = "Generators"))
class AIMODULE_API UEnvQueryGenerator : public UEnvQueryNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, Category=Option)
	FString OptionName;

	/** type of generated items */
	UPROPERTY()
	TSubclassOf<UEnvQueryItemType> ItemType;

	/** if set, tests will be automatically sorted for best performance before running query */
	UPROPERTY(EditDefaultsOnly, Category = Option, AdvancedDisplay)
	uint32 bAutoSortTests : 1;

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const { checkNoEntry(); }

	virtual void PostLoad() override;
	void UpdateNodeVersion() override;
};
