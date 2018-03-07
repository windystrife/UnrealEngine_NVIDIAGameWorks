// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTexture;
class UTextureFactory;

namespace NormalMapIdentification
{
	/**
	 * Handle callback when an asset is imported.
	 * @param	InFactory	The texture factory being used.
	 * @param	InTexture	The texture that was imported.
	 */
	void HandleAssetPostImport( UTextureFactory* InFactory, UTexture* InTexture );
}
