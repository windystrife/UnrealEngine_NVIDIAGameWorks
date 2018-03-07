// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/BlueprintFactory.h"
#include "BlueprintMacroFactory.generated.h"

UCLASS(MinimalAPI, hidecategories=Object, collapsecategories)
class UBlueprintMacroFactory : public UBlueprintFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory interface
	virtual FText GetDisplayName() const override;
	virtual FName GetNewAssetThumbnailOverride() const override;
	virtual uint32 GetMenuCategories() const override;
	virtual FText GetToolTip() const override;
	virtual FString GetToolTipDocumentationExcerpt() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual FString GetDefaultNewAssetName() const override;
	// End of UFactory interface

protected:
	// UBlueprintFactory interface
	virtual bool IsMacroFactory() const override { return true; }
	// End of UBlueprintFactory interface
};
