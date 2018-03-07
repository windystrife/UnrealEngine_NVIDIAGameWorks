// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "ImgMediaSourceFactoryNew.generated.h"


/**
 * Implements a factory for UImgMediaSource objects.
 */
UCLASS(hidecategories=Object)
class UImgMediaSourceFactoryNew
	: public UFactory
{
	GENERATED_UCLASS_BODY()

public:

	//~ UFactory Interface

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	virtual bool ShouldShowInNewMenu() const override;
};
