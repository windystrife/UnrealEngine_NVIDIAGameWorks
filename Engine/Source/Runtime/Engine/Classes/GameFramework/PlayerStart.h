// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/NavigationObjectBase.h"
#include "PlayerStart.generated.h"

/** 
 *	This class indicates a location where a player can spawn when the game begins
 *	
 *	@see https://docs.unrealengine.com/latest/INT/Engine/Actors/PlayerStart/
 */
UCLASS(Blueprintable, ClassGroup=Common, hidecategories=Collision)
class ENGINE_API APlayerStart : public ANavigationObjectBase
{
	GENERATED_BODY()
public:

	APlayerStart(const FObjectInitializer& ObjectInitializer);

	/*~ To take more control over PlayerStart selection, you can override the virtual AGameModeBase::FindPlayerStart and AGameModeBase::ChoosePlayerStart functions. */

	/** Used when searching for which playerstart to use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Object)
	FName PlayerStartTag;

	/** Arrow component to indicate forward direction of start */
#if WITH_EDITORONLY_DATA
private:
	UPROPERTY()
	class UArrowComponent* ArrowComponent;
public:
#endif

#if WITH_EDITORONLY_DATA
	/** Returns ArrowComponent subobject **/
	class UArrowComponent* GetArrowComponent() const;
#endif
};
