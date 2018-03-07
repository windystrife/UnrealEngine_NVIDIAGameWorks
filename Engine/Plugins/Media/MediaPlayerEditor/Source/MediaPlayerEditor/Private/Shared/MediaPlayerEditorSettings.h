// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "MediaPlayerEditorSettings.generated.h"


/** Options for scaling the viewport's video texture. */
UENUM()
enum class EMediaPlayerEditorScale : uint8
{
	/** Stretch non-uniformly to fill the viewport. */
	Fill,

	/** Scale uniformly, preserving aspect ratio. */
	Fit,

	/** Do not stretch or scale. */
	Original
};


UCLASS(config=EditorPerProjectUserSettings)
class UMediaPlayerEditorSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** The name of the desired native media player to use for playback. */
	UPROPERTY(config, EditAnywhere, Category=Viewer)
	FName DesiredPlayerName;

	/** Whether to display overlay texts. */
	UPROPERTY(config, EditAnywhere, Category=Viewer)
	bool ShowTextOverlays;

	/** How the video viewport should be scaled. */
	UPROPERTY(config, EditAnywhere, Category=Viewer)
	EMediaPlayerEditorScale ViewportScale;
};
