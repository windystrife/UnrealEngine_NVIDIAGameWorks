// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SourceCodeAccessSettings.generated.h"

UCLASS(config=EditorSettings)
class USourceCodeAccessSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** The source code editor we prefer to use. */
	UPROPERTY(Config, EditAnywhere, Category="Source Code Editor", meta=(DisplayName="Source Code Editor"))
	FString PreferredAccessor;
};
