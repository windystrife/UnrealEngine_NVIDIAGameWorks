// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ExtractSprites/PaperExtractSpritesSettings.h"

UPaperExtractSpritesSettings::UPaperExtractSpritesSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OutlineColor = FLinearColor::Yellow;
	ViewportTextureTint = FLinearColor::Gray;
	BackgroundColor = FLinearColor(0.1f, 0.1f, 0.1f);
}

UPaperExtractSpriteGridSettings::UPaperExtractSpriteGridSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
