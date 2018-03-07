// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "SkeletonFactory.generated.h"

struct FAssetData;
class SWindow;

UCLASS(HideCategories=Object,MinimalAPI)
class USkeletonFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// The skeletal mesh with which to initialize this skeleton.
	UPROPERTY()
	class USkeletalMesh* TargetSkeletalMesh;

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	

private:
	void OnTargetSkeletalMeshSelected(const FAssetData& SelectedAsset);

private:
	TSharedPtr<SWindow> PickerWindow;
};

