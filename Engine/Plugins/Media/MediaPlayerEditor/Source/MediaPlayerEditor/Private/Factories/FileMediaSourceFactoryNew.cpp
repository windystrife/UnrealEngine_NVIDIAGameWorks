// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/FileMediaSourceFactoryNew.h"
#include "AssetTypeCategories.h"
#include "FileMediaSource.h"


/* UFileMediaSourceFactoryNew structors
 *****************************************************************************/

UFileMediaSourceFactoryNew::UFileMediaSourceFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UFileMediaSource::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory interface
 *****************************************************************************/

UObject* UFileMediaSourceFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UFileMediaSource>(InParent, InClass, InName, Flags);
}


uint32 UFileMediaSourceFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}


bool UFileMediaSourceFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
