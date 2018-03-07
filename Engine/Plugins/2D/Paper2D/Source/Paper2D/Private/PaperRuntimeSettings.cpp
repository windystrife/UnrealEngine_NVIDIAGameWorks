// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaperRuntimeSettings.h"

//////////////////////////////////////////////////////////////////////////
// UPaperRuntimeSettings

UPaperRuntimeSettings::UPaperRuntimeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bEnableSpriteAtlasGroups(false)
	, bEnableTerrainSplineEditing(false)
	, bResizeSpriteDataToMatchTextures(true)
{
}
