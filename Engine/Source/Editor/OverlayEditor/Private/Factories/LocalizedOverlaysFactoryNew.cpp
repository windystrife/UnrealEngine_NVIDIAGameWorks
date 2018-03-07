// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/LocalizedOverlaysFactoryNew.h"
#include "LocalizedOverlays.h"
#include "AssetTypeCategories.h"

/* ULocalizedOverlaysFactoryNew structors
*****************************************************************************/

ULocalizedOverlaysFactoryNew::ULocalizedOverlaysFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = ULocalizedOverlays::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory interface
*****************************************************************************/

UObject* ULocalizedOverlaysFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<ULocalizedOverlays>(InParent, InClass, InName, Flags);
}


uint32 ULocalizedOverlaysFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}


bool ULocalizedOverlaysFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}