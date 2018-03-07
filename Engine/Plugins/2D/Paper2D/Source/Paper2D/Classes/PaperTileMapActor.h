// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"

#include "PaperTileMapActor.generated.h"

/**
 * An instance of a UPaperTileMap in a level.
 *
 * This actor is created when you drag a tile map asset from the content browser into the level, and
 * it is just a thin wrapper around a UPaperTileMapComponent that actually references the asset.
 */
UCLASS(ComponentWrapperClass)
class PAPER2D_API APaperTileMapActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category=TileMapActor, VisibleAnywhere, BlueprintReadOnly, meta=(ExposeFunctionCategories="Sprite,Rendering,Physics,Components|Sprite", AllowPrivateAccess="true"))
	class UPaperTileMapComponent* RenderComponent;
public:

	// AActor interface
#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif
	// End of AActor interface

	/** Returns RenderComponent subobject **/
	FORCEINLINE class UPaperTileMapComponent* GetRenderComponent() const { return RenderComponent; }
};
