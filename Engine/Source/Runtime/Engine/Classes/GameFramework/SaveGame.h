// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SaveGame.generated.h"

/** 
 *	This class acts as a base class for a save game object that can be used to save state about the game. 
 *	When you create your own save game subclass, you would add member variables for the information that you want to save.
 *	Then when you want to save a game, create an instance of this object using CreateSaveGameObject, fill in the data, and use SaveGameToSlot, providing a slot name.
 *	To load the game you then just use LoadGameFromSlot, and read the data from the resulting object.
 *
 *	@see https://docs.unrealengine.com/latest/INT/Gameplay/SaveGame
 */
UCLASS(abstract, Blueprintable, BlueprintType)
class ENGINE_API USaveGame : public UObject
{
	/**
	 *	@see UGameplayStatics::CreateSaveGameObject
	 *	@see UGameplayStatics::SaveGameToSlot
	 *	@see UGameplayStatics::DoesSaveGameExist
	 *	@see UGameplayStatics::LoadGameFromSlot
	 *	@see UGameplayStatics::DeleteGameInSlot
	 */

	GENERATED_UCLASS_BODY()
};
