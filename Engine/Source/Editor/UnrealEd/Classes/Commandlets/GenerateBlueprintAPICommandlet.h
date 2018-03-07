// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "GenerateBlueprintAPICommandlet.generated.h"

UCLASS()
class UGenerateBlueprintAPICommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:		
	//~ Begin UCommandlet Interface
	virtual int32 Main(FString const& Params) override;
	//~ End UCommandlet Interface
};
