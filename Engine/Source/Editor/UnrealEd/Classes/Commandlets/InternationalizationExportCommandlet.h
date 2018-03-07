// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/GatherTextCommandletBase.h"
#include "InternationalizationExportCommandlet.generated.h"

/**
 *	UInternationalizationExportCommandlet: Commandlet used to export internationalization data to various standard formats. 
 */
UCLASS()
class UInternationalizationExportCommandlet : public UGatherTextCommandletBase
{
    GENERATED_BODY()

public:
	UInternationalizationExportCommandlet(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{}

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface
};
