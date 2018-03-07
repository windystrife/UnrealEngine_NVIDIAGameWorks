// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// ReimportSpeedTreeFactory
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EditorReimportHandler.h"
#include "SpeedTreeImportFactory.h"
#include "ReimportSpeedTreeFactory.generated.h"

UCLASS(collapsecategories)
class UReimportSpeedTreeFactory : public USpeedTreeImportFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	//~ Begin FReimportHandler interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;
	virtual const UObject* GetFactoryObject() const { return this; }
	//~ End FReimportHandler interface
};



