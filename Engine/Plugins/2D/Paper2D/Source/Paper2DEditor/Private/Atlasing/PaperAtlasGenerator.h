// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UPaperSpriteAtlas;

//////////////////////////////////////////////////////////////////////////
// FPaperAtlasGenerator

class UPaperSpriteAtlas;

struct FPaperAtlasGenerator
{
public:
	static void HandleAssetChangedEvent(UPaperSpriteAtlas* Atlas);
};
