// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SpriteEditorSettings.generated.h"

// Settings for the Paper2D sprite editor
UCLASS(config=EditorPerProjectUserSettings)
class USpriteEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	USpriteEditorSettings();

	/** Background color in the sprite editor */
	UPROPERTY(config, EditAnywhere, Category=Background, meta=(HideAlphaChannel))
	FColor BackgroundColor;

	/** Should the grid be shown by default when the editor is opened? */
	UPROPERTY(config, EditAnywhere, Category=Background)
	bool bShowGridByDefault;
};
