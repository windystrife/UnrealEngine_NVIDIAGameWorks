// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// FontFactory: Creates a Font Factory
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "EditorReimportHandler.h"
#include "FontFileImportFactory.generated.h"

UCLASS()
class UNREALED_API UFontFileImportFactory : public UFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateBinary(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface

	//~ Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;
	//~ End FReimportHandler Interface

private:
	enum class EBatchCreateFontAsset : uint8
	{
		Unknown,
		Cancel,
		Yes,
		No,
	};
	EBatchCreateFontAsset BatchCreateFontAsset;
};
