// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GroundTruthDataFactory.h"
#include "GroundTruthData.h"
#include "AssetTypeCategories.h"

#define LOCTEXT_NAMESPACE "UGroundTruthDataFactory"

UGroundTruthDataFactory::UGroundTruthDataFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = false;
	bEditorImport = false;
	SupportedClass = UGroundTruthData::StaticClass();
}

UObject* UGroundTruthDataFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* Context, FFeedbackContext* Warn)
{
	UGroundTruthData* GroundTruthData = NewObject<UGroundTruthData>(InParent, SupportedClass, InName, InFlags | RF_Transactional);
	return GroundTruthData;
}

uint32 UGroundTruthDataFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Misc;
}

FText UGroundTruthDataFactory::GetDisplayName() const
{
	return LOCTEXT("MenuEntry", "Ground Truth Data");
}

#undef LOCTEXT_NAMESPACE
