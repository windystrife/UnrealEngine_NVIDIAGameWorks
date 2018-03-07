// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "AnimSequenceFactory.generated.h"

struct FAssetData;
class SWindow;

UCLASS(HideCategories=Object)
class UNREALED_API UAnimSequenceFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class USkeleton* TargetSkeleton;

	/** The preview mesh to use with this animation */
	UPROPERTY()
	class USkeletalMesh* PreviewSkeletalMesh;

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override { return false; } 
	//~ Begin UFactory Interface	

private:
	void OnTargetSkeletonSelected(const FAssetData& SelectedAsset);

private:
	TSharedPtr<SWindow> PickerWindow;
};

