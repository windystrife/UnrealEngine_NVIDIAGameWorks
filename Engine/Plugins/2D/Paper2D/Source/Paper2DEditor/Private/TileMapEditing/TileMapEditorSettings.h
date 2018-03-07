// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "TileMapEditorSettings.generated.h"

// Settings for the Paper2D tile map editor
UCLASS(config=EditorPerProjectUserSettings)
class UTileMapEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UTileMapEditorSettings();

	/** Default background color for new tile map assets */
	UPROPERTY(config, EditAnywhere, Category=Background, meta=(HideAlphaChannel))
	FColor DefaultBackgroundColor;

	/** Should the grid be shown by default when the editor is opened? */
	UPROPERTY(config, EditAnywhere, Category=Background)
	bool bShowGridByDefault;
};
