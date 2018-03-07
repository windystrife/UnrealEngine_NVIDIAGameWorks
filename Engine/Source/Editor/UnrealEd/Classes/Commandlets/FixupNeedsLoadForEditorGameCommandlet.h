// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/ResavePackagesCommandlet.h"
#include "FixupNeedsLoadForEditorGameCommandlet.generated.h"

UCLASS()
class UFixupNeedsLoadForEditorGameCommandlet : public UResavePackagesCommandlet
{
	GENERATED_BODY()

	/** Map of class names and their default NeedsLoadForEditorGame() return values. */
	TMap<FName, bool> ResaveClassNeedsLoadForEditorGameValues;

protected:

	//~ Begin UResavePackagesCommandlet Interface
	virtual int32 InitializeResaveParameters(const TArray<FString>& Tokens, TArray<FString>& MapPathNames) override;
	virtual void PerformPreloadOperations(FLinkerLoad* PackageLinker, bool& bSavePackage) override;
	//~ End UCommandlet Interface
};
