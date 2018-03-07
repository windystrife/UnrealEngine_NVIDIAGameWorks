// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// ReimportSoundSurroundFactory
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EditorReimportHandler.h"
#include "Factories/SoundSurroundFactory.h"
#include "ReimportSoundSurroundFactory.generated.h"

UCLASS(hidecategories=Object, collapsecategories)
class UReimportSoundSurroundFactory : public USoundSurroundFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FString> ReimportPaths;

	//~ Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;
	//~ End FReimportHandler Interface
};
