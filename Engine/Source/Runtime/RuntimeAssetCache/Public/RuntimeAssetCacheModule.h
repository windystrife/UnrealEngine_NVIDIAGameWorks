// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FRuntimeAssetCacheInterface;

/** Forward declarations. */
class FRuntimeAssetCacheInterface;

RUNTIMEASSETCACHE_API FRuntimeAssetCacheInterface& GetRuntimeAssetCache();

/**
* Module for the RuntimeAssetCache.
*/
class FRuntimeAssetCacheModuleInterface : public IModuleInterface
{
public:
	/**
	 * Gets runtime asset cache.
	 * @return Runtime asset cache.
	 */
	virtual FRuntimeAssetCacheInterface& GetRuntimeAssetCache() = 0;
};
