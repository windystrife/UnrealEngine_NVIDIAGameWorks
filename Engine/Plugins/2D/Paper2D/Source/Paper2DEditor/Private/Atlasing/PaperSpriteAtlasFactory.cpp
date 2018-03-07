// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Atlasing/PaperSpriteAtlasFactory.h"
#include "PaperSpriteAtlas.h"
#include "PaperRuntimeSettings.h"

//////////////////////////////////////////////////////////////////////////
// UPaperSpriteAtlasFactory

UPaperSpriteAtlasFactory::UPaperSpriteAtlasFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPaperSpriteAtlas::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UPaperSpriteAtlasFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UObject>(InParent, SupportedClass, InName, Flags | RF_Transactional);
}

bool UPaperSpriteAtlasFactory::CanCreateNew() const
{
	return GetDefault<UPaperRuntimeSettings>()->bEnableSpriteAtlasGroups;
}
