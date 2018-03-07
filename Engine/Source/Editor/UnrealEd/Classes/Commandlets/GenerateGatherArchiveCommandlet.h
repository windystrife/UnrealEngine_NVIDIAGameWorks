// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/GatherTextCommandletBase.h"
#include "GenerateGatherArchiveCommandlet.generated.h"

/**
 *	UGenerateGatherArchiveCommandlet: Generates a localisation archive; generally used as a gather step.
 */
UCLASS()
class UGenerateGatherArchiveCommandlet : public UGatherTextCommandletBase
{
    GENERATED_UCLASS_BODY()
#if CPP || UE_BUILD_DOCS
public:
	//~ Begin UCommandlet Interface
	virtual int32 Main( const FString& Params ) override;
	//~ End UCommandlet Interface
#endif
};
