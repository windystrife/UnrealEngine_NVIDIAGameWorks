// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaperTileSetFactory.h"
#include "PaperTileSet.h"
#include "TileSetEditor/TileSetEditorSettings.h"

#define LOCTEXT_NAMESPACE "Paper2D"

/////////////////////////////////////////////////////
// UPaperTileSetFactory

UPaperTileSetFactory::UPaperTileSetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UPaperTileSet::StaticClass();
}

UObject* UPaperTileSetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UPaperTileSet* NewTileSet = NewObject<UPaperTileSet>(InParent, Class, Name, Flags | RF_Transactional);
	NewTileSet->SetTileSheetTexture(InitialTexture);
	NewTileSet->SetBackgroundColor(GetDefault<UTileSetEditorSettings>()->DefaultBackgroundColor);

	NewTileSet->PostEditChange();

	return NewTileSet;
}

#undef LOCTEXT_NAMESPACE
