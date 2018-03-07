// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 * A configuration class used by the UMaterial Editor to save editor
 * settings across sessions.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "MaterialEditorOptions.generated.h"

UCLASS(hidecategories=Object, config=EditorPerProjectUserSettings)
class UNREALED_API UMaterialEditorOptions : public UObject
{
	GENERATED_UCLASS_BODY()

	/** If true, render grid the preview scene. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bShowGrid:1;

	/** If true, render background object in the preview scene. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bShowBackground:1;

	/** If true, don't render connectors that are not connected to anything. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bHideUnusedConnectors:1;

	/** If true, the 3D material preview viewport updates in realtime. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bRealtimeMaterialViewport:1;

	/** If true, the linked object viewport updates in realtime. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bRealtimeExpressionViewport:1;

	/** If true, always refresh the material preview. */
	UPROPERTY(EditAnywhere, config, Category = Options)
	uint32 bLivePreviewUpdate : 1;

	/** If true, always refresh all expression previews. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bAlwaysRefreshAllPreviews:1;

	/** If false, use expression categorized menus. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bUseUnsortedMenus:1;

	/** If true, show mobile statis and errors. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	uint32 bShowMobileStats:1;

	/** The users favorite material expressions. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	TArray<FString> FavoriteExpressions;

};

