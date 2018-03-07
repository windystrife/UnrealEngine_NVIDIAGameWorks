// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TileSetEditor/TileSetEditorSettings.h"

//////////////////////////////////////////////////////////////////////////
// UTileSetEditorSettings

UTileSetEditorSettings::UTileSetEditorSettings()
	: DefaultBackgroundColor(0, 0, 127)
	, bShowGridByDefault(true)
	, ExtrusionAmount(2)
	, bPadToPowerOf2(true)
	, bFillWithTransparentBlack(true)
{
}
