// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/PlatformMediaSourceFactoryNew.h"
#include "AssetTypeCategories.h"
#include "PlatformMediaSource.h"


/* UPlatformMediaSourceFactoryNew structors
 *****************************************************************************/

UPlatformMediaSourceFactoryNew::UPlatformMediaSourceFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPlatformMediaSource::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory interface
 *****************************************************************************/

UObject* UPlatformMediaSourceFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UPlatformMediaSource>(InParent, InClass, InName, Flags);
}


uint32 UPlatformMediaSourceFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}


bool UPlatformMediaSourceFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
