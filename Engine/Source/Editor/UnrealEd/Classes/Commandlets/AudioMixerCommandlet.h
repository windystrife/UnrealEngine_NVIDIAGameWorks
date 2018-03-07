// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "AudioMixerCommandlet.generated.h"

/**
* Commandlet used to test various aspects of audio mixer without loading UI.
*/
UCLASS()
class UAudioMixerCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

	void PrintUsage() const;
};
