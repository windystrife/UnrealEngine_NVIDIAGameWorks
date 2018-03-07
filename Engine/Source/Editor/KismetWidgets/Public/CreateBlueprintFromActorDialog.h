// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

//////////////////////////////////////////////////////////////////////////
// FCreateBlueprintFromActorDialog

class FCreateBlueprintFromActorDialog
{
public:
	/** 
	 * Static function to access constructing this window.
	 *
	 * @param bInHarvest		true if the components of the selected actors should be harvested for the blueprint.
	 * @param ActorOverride		If set convert the specified actor, if null use the currently selected actor
	 */
	static KISMETWIDGETS_API void OpenDialog(bool bInHarvest, AActor* InActorOverride = nullptr);
private:

	/** 
	 * Will create the blueprint
	 *
	 * @param InAssetPath		Path of the asset to create
	 * @param bInHarvest		true if the components of the selected actors should be harvested for the blueprint.
	 */
	static void OnCreateBlueprint(const FString& InAssetPath, bool bInHarvest);
private:
	static TWeakObjectPtr<AActor> ActorOverride;
};
