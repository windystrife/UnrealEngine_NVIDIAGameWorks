// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/GatherTextCommandletBase.h"
#include "GatherTextCommandlet.generated.h"

namespace EOutputJson
{
	enum Format { Manifest, Archive };
}

/**
 *	UGatherTextCommandlet: One commandlet to rule them all. This commandlet loads a config file and then calls other localization commandlets. Allows localization system to be easily extendable and flexible. 
 */
UCLASS()
class UGatherTextCommandlet : public UGatherTextCommandletBase
{
    GENERATED_UCLASS_BODY()
public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

	// Helpler function to generate a changelist description
	FText GetChangelistDescription( const FString& InConfigPath );

	static const FString UsageText;

};
