// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IMergeActorsTool;

/**
 * Merge Actors module interface
 */
class IMergeActorsModule : public IModuleInterface
{

public:

	/**
	 * Get reference to the Merge Actors module instance
	 */
	static inline IMergeActorsModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IMergeActorsModule>("MergeActors");
	}

	/**
	 * Register an IMergeActorsTool with the module, passing ownership to it
	 */
	MERGEACTORS_API virtual bool RegisterMergeActorsTool(TUniquePtr<IMergeActorsTool> Tool) = 0;

	/**
	 * Unregister an IMergeActorsTool with the module
	 */
	MERGEACTORS_API virtual bool UnregisterMergeActorsTool(IMergeActorsTool* Tool) = 0;
};
