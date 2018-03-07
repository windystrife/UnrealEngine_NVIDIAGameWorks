// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "SmokeTestCommandlet.generated.h"

UCLASS()
class USmokeTestCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()


	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface
};

